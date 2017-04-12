#include "sensor_microphone_thread.h"
#include "commons.h"
#include "sf_message_api.h"
#include "bsp_cfg.h"

void g_adc_framework_microphone_callback ( sf_adc_periodic_callback_args_t * p_args );
unsigned int sensor_microphone_formatDataForCloudPublish ( const event_sensor_payload_t * const eventPtr,
                                                           char * payload, size_t payloadLength );

uint32_t g_sampleAverageCount = 2000U;        // Sampling at 4Khz, average over .5 sec
uint16_t g_noiseLevelMin = 20U;
uint16_t g_noiseLevelMax = 200U;

ULONG sensor_microphone_thread_wait = 10; //TX_WAIT_FOREVER;

uint8_t g_sensor_microphone_thread_id;

/* Sensor Microphone (ICS-40181) Thread entry function */
void sensor_microphone_thread_entry ( void )
{
    ssp_err_t err;
    sf_message_header_t * message;
    ssp_err_t msgStatus;

    g_sensor_microphone_thread_id = getUniqueThreadId ();
    registerSensorForCloudPublish ( g_sensor_microphone_thread_id, NULL );

    /*
     * We have to setup the PGA for the MIC input because the SSP dosen't support it yet...
     *  To use the PGA Jumper P03 (PGAVSS000) to ground on the SK S7
     */

    adc_instance_ctrl_t * p_ctrl = (adc_instance_ctrl_t *) g_adc0.p_ctrl;

    R_S12ADC0_Type * p_adc = (R_S12ADC0_Type *) p_ctrl->p_reg;

    p_adc->ADPGACR_b.P000GEN = 1;   // Enable the gain setting
    p_adc->ADPGACR_b.P000ENAMP = 1; // Enable the amplifier
    p_adc->ADPGACR_b.P000SEL0 = 0;  // Don't bypass the PGA
    p_adc->ADPGACR_b.P000SEL1 = 1;  // Route signal through the PGA
    p_adc->ADPGADCR0_b.P000DEN = 0; // Disable differential input
    p_adc->ADPGAGS0_b.P000GAIN = 1; // Gain of 2.5

    err = g_sf_adc_periodic0.p_api->start ( g_sf_adc_periodic0.p_ctrl );
    APP_ERR_TRAP (err)

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sensor_microphone_thread_message_queue, (void **) &message,
                                       sensor_microphone_thread_wait );

        if ( msgStatus == SSP_SUCCESS )
        {
            switch ( message->event_b.class_code )
            {

                case SF_MESSAGE_EVENT_CLASS_SYSTEM :
                    //TODO: anything related to System Message Processing goes here
                    if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_CLOUD_AVAILABLE )
                    {
                        registerSensorForCloudPublish ( g_sensor_microphone_thread_id,
                                                        sensor_microphone_formatDataForCloudPublish );
                    }
                    else if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_CLOUD_DISCONNECTED )
                    {
                        registerSensorForCloudPublish ( g_sensor_microphone_thread_id, NULL );
                    }
                    else if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_BUTTON_S5_PRESSED )
                    {
                        postSensorEventMessage ( g_sensor_microphone_thread_id, SF_MESSAGE_EVENT_SENSOR_NEW_DATA,
                                                 NULL );
                    }
                    break;
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
}

volatile uint16_t max_sound;
volatile uint16_t g_sound_level = 0;
volatile uint16_t g_prev_sound_level = 0;
volatile int g_difference = 0;

void g_adc_framework_microphone_callback ( sf_adc_periodic_callback_args_t * p_args )
{
    static uint16_t sample_count = 0;
    static uint16_t max = 0U;
    static uint16_t min = 4095U;
    uint16_t data = g_adc_raw_data [ p_args->buffer_index ];

    if ( data > max )
    {
        max = data;
    }
    if ( data < min )
    {
        min = data;
    }

    sample_count++;

    if ( sample_count > g_sampleAverageCount )
    {
        sample_count = 0;
        g_sound_level = ( uint16_t ) ( max - min );
        max = 0U;
        min = 4095U;

        if ( g_sound_level > max_sound )
        {
            max_sound = g_sound_level;
        }

        g_difference = g_sound_level - g_prev_sound_level;
        uint16_t difference = (uint16_t) abs ( g_difference );

        if ( difference > g_noiseLevelMin )
        {
            postSensorEventMessage ( g_sensor_microphone_thread_id, SF_MESSAGE_EVENT_SENSOR_NEW_DATA, NULL );
        }

        g_prev_sound_level = g_sound_level;
    }
}

unsigned int sensor_microphone_formatDataForCloudPublish ( const event_sensor_payload_t * const eventPtr,
                                                           char * payload, size_t payloadLength )
{
    int bytesProcessed = 0;
    if ( eventPtr != NULL )
    {
        // it's a temperature publish event
        bytesProcessed = snprintf ( payload, payloadLength,
                                    "{\"noise\":{\"current_level\": %d, \"previous_level\":%d, \"difference\":%d}}",
                                    g_sound_level, ( g_sound_level - g_difference ), abs ( g_difference ) );
    }

    if ( bytesProcessed < 0 )
    {
        return 0;
    }

    return (unsigned int) bytesProcessed;
}


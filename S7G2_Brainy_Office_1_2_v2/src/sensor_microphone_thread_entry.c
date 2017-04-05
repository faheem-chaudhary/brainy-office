#include "sensor_microphone_thread.h"
#include "commons.h"
#include "sf_message_api.h"
#include "bsp_cfg.h"

void g_adc_framework_microphone_callback ( sf_adc_periodic_callback_args_t * p_args );
unsigned int sensor_microphone_formatDataForCloudPublish ( const event_sensor_payload_t * const eventPtr,
                                                           char * payload, size_t payloadLength );

#define AVG_CNT 2000        // Sampling at 4Khz, average over .5 sec

#define NOISE_LEVEL_MIN 20U
#define NOISE_LEVEL_MAX 200U

#define ADC_BASE_PTR  R_S12ADC0_Type *

ssp_err_t postAdcFrameworkEventMessage ( void * dataPtr );

ULONG sensor_microphone_thread_wait = TX_WAIT_FOREVER;

uint8_t g_sensor_microphone_thread_id;

/* Sensor Microphone (ADC) Thread entry function */
void sensor_microphone_thread_entry ( void )
{
    ssp_err_t err;
    sf_message_header_t * message;
    ssp_err_t msgStatus;

    g_sensor_microphone_thread_id = getUniqueThreadId ();

    bool isCloudConnected = false;
    do
    {
        msgStatus = messageQueuePend ( &sensor_humidity_temperature_thread_message_queue, (void **) &message,
                                       TX_WAIT_FOREVER );
        if ( msgStatus == SSP_SUCCESS )
        {
            if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_SYSTEM )
            {
                isCloudConnected = ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_CLOUD_AVAILABLE );
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
    while ( !isCloudConnected );

    /*
     * We have to setup the PGA for the MIC input because the SSP dosen't support it yet...
     *  To use the PGA Jumper P03 (PGAVSS000) to ground on the SK S7
     */

    ADC_BASE_PTR p_adc;
    adc_instance_ctrl_t * p_ctrl = (adc_instance_ctrl_t *) g_adc0.p_ctrl;

    p_adc = (ADC_BASE_PTR) p_ctrl->p_reg;

    p_adc->ADPGACR_b.P000GEN = 1;   // Enable the gain setting
    p_adc->ADPGACR_b.P000ENAMP = 1; // Enable the amplifier
    p_adc->ADPGACR_b.P000SEL0 = 0;  // Don't bypass the PGA
    p_adc->ADPGACR_b.P000SEL1 = 1;  // Route signal through the PGA
    p_adc->ADPGADCR0_b.P000DEN = 0; // Disable differential input
    p_adc->ADPGAGS0_b.P000GAIN = 1; // Gain of 2.5

    err = g_sf_adc_periodic0.p_api->start ( g_sf_adc_periodic0.p_ctrl );
    APP_ERR_TRAP (err)

    registerSensorForCloudPublish ( g_sensor_microphone_thread_id, sensor_microphone_formatDataForCloudPublish );

    bool isForedDataCalculation = false;

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
                        isCloudConnected = true;
                        registerSensorForCloudPublish ( g_sensor_microphone_thread_id,
                                                        sensor_microphone_formatDataForCloudPublish );
                    }
                    else if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_CLOUD_DISCONNECTED )
                    {
                        isCloudConnected = false;
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
volatile uint16_t sound_level = 0;
volatile uint16_t g_prev_sound_level = 0;

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

    if ( sample_count > AVG_CNT )
    {
        sample_count = 0;
        sound_level = ( uint16_t ) ( max - min );
        max = 0U;
        min = 4095U;

        if ( sound_level > max_sound )
        {
            max_sound = sound_level;
        }

        uint16_t difference = ( uint16_t ) ( sound_level - g_prev_sound_level );

        if ( difference > NOISE_LEVEL_MIN )
        {
            postSensorEventMessage ( g_sensor_microphone_thread_id, SF_MESSAGE_EVENT_SENSOR_NEW_DATA, NULL );
        }

//        tx_semaphore_ceiling_put ( &g_microphone_data_ready, 1 );
    }
}

unsigned int sensor_microphone_formatDataForCloudPublish ( const event_sensor_payload_t * const eventPtr,
                                                           char * payload, size_t payloadLength )
{
    int bytesProcessed = 0;
    if ( eventPtr != NULL )
    {
        // it's a temperature publish event
        bytesProcessed = snprintf ( payload, payloadLength, "{\"noise\":{\"raw\": %d}", sound_level );
    }

    if ( bytesProcessed < 0 )
    {
        return 0;
    }

    return (unsigned int) bytesProcessed;
}


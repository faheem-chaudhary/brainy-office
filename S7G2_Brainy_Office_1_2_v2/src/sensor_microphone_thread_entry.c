#include "sensor_microphone_thread.h"
#include "commons.h"
#include "event_adc_framework_mic_data_api.h"
#include "sf_message_api.h"
#include "bsp_cfg.h"

void g_adc_framework_microphone_callback ( sf_adc_periodic_callback_args_t * p_args );

#define AVG_CNT 2000        // Sampling at 4Khz, average over .5 sec

#define NOISE_LEVEL_MIN 20U
#define NOISE_LEVEL_MAX 200U

#define ADC_BASE_PTR  R_S12ADC0_Type *

ssp_err_t postAdcFrameworkEventMessage ( void * dataPtr );

volatile uint16_t max_sound;
volatile uint16_t sound_level = 0;

ULONG sensor_microphone_thread_wait = TX_WAIT_FOREVER;

/* Sensor Microphone (ADC) Thread entry function */
void sensor_microphone_thread_entry ( void )
{
    ssp_err_t err;
    ADC_BASE_PTR p_adc;
    adc_instance_ctrl_t * p_ctrl = (adc_instance_ctrl_t *) g_adc0.p_ctrl;

    /*
     * We have to setup the PGA for the MIC input because the SSP dosen't support it yet...
     *
     *  To use the PGA Jumper P03 (PGAVSS000) to ground on the SK S7
     */

    p_adc = (ADC_BASE_PTR) p_ctrl->p_reg;

    p_adc->ADPGACR_b.P000GEN = 1;   // Enable the gain setting
    p_adc->ADPGACR_b.P000ENAMP = 1; // Enable the amplifier
    p_adc->ADPGACR_b.P000SEL0 = 0;  // Don't bypass the PGA
    p_adc->ADPGACR_b.P000SEL1 = 1;  // Route signal through the PGA
    p_adc->ADPGADCR0_b.P000DEN = 0; // Disable differential input
    p_adc->ADPGAGS0_b.P000GAIN = 1; // Gain of 2.5

    err = g_sf_adc_periodic0.p_api->start ( g_sf_adc_periodic0.p_ctrl );
    APP_ERR_TRAP (err)

    sf_message_header_t * message;
    ssp_err_t msgStatus;

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
                    break;

                case SF_MESSAGE_EVENT_CLASS_ADC_FRAMEWORK_MIC_DATA :
                    if ( sound_level > NOISE_LEVEL_MIN )
                    {
                        //ledOn (AMBER);
                    }
                    else
                    {
                        //ledOff (AMBER);
                    }

                    if ( sound_level > NOISE_LEVEL_MAX )
                    {
                        //ledOn (RED);
                    }
                    else
                    {
                        //ledOff (RED);
                    }

                    if ( sound_level > max_sound )
                    {
                        max_sound = sound_level;
                    }
                    break;
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
}

ssp_err_t postAdcFrameworkEventMessage ( void * dataPtr )
{
    event_adc_framework_mic_data_payload_t* message;
    ssp_err_t status = messageQueueAcquireBuffer ( (void **) &message );
    if ( status == SSP_SUCCESS )
    {
        message->header.event_b.class_code = SF_MESSAGE_EVENT_CLASS_ADC_FRAMEWORK_MIC_DATA;
        message->header.event_b.code = 0x00;
        message->dataPointer = dataPtr;

        status = messageQueuePost ( (void **) &message );

        if ( status != SSP_SUCCESS )
        {
            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
    return status;
}

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

    if ( sample_count++ > AVG_CNT )
    {
        sample_count = 0;
        sound_level = ( uint16_t ) ( max - min );
        max = 0U;
        min = 4095U;
        postAdcFrameworkEventMessage (NULL);
//        tx_semaphore_ceiling_put ( &g_microphone_data_ready, 1 );
    }
}


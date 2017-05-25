#include "sensor_microphone_thread.h"
#include "commons.h"
#include "sf_message_api.h"
#include "bsp_cfg.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
///                        -- None --                        ///

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
///                        -- None --                        ///

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
///                        -- None --                        ///

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
static uint32_t sampleAverageCount = 2000U;        // Sampling at 4Khz, average over .5 sec
static uint16_t noiseLevelMin = 20U;
static uint16_t noiseLevelMax = 200U;
static volatile uint16_t max_sound;
static volatile uint16_t sound_level = 0;
static volatile uint16_t prev_sound_level = 0;
static volatile int sound_level_difference = 0;
static ULONG sensor_microphone_thread_wait = 10; //TX_WAIT_FOREVER;
static uint8_t sensor_microphone_thread_id;

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
void sensor_microphone_thread_entry ( void );
void g_adc_framework_microphone_callback ( sf_adc_periodic_callback_args_t * p_args );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
static unsigned int sensor_microphone_formatDataForCloudPublish ( const event_sensor_payload_t * const eventPtr,
                                                                  char * payload, size_t payloadLength );
static unsigned int sensor_microphone_formatFileHeader ( char * payload, size_t payloadLength );
static unsigned int sensor_microphone_formatDataForFileLogging ( const event_sensor_payload_t * const eventPtr,
                                                                 char * payload, size_t payloadLength );

/// --  END OF: Static (file scope) Function Declarations -- ///

/* Sensor Microphone (ICS-40181) Thread entry function */
void sensor_microphone_thread_entry ( void )
{
    ssp_err_t err;
    sf_message_header_t * message;
    ssp_err_t msgStatus;

    sensor_microphone_thread_id = getUniqueThreadId ();
    registerSensorForCloudPublish ( sensor_microphone_thread_id, NULL );

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
    handleError ( err );

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
                    switch ( message->event_b.code )
                    {
                        case SF_MESSAGE_EVENT_SYSTEM_CLOUD_AVAILABLE :
                            registerSensorForCloudPublish ( sensor_microphone_thread_id,
                                                            sensor_microphone_formatDataForCloudPublish );
                            break;
                        case SF_MESSAGE_EVENT_SYSTEM_CLOUD_DISCONNECTED :
                            registerSensorForCloudPublish ( sensor_microphone_thread_id, NULL );
                            break;
                        case SF_MESSAGE_EVENT_SYSTEM_BUTTON_S5_PRESSED :
                            postSensorEventMessage ( sensor_microphone_thread_id, SF_MESSAGE_EVENT_SENSOR_NEW_DATA,
                                                     NULL );
                            break;
                        case SF_MESSAGE_EVENT_SYSTEM_FILE_LOGGING_AVAILABLE :
                            registerSensorForFileLogging ( sensor_microphone_thread_id, "mic", "csv",
                                                           sensor_microphone_formatFileHeader,
                                                           sensor_microphone_formatDataForFileLogging );
                            break;

                        case SF_MESSAGE_EVENT_SYSTEM_FILE_LOGGING_UNAVAILABLE :
                            registerSensorForFileLogging ( sensor_microphone_thread_id, "", "", NULL, NULL );
                            break;
                    }
                    break;
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
}

__attribute__((no_instrument_function))
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

    if ( sample_count > sampleAverageCount )
    {
        sample_count = 0;
        sound_level = ( uint16_t ) ( max - min );
        max = 0U;
        min = 4095U;

        if ( sound_level > max_sound )
        {
            max_sound = sound_level;
        }

        sound_level_difference = sound_level - prev_sound_level;
        uint16_t difference = (uint16_t) abs ( sound_level_difference );

        if ( difference > noiseLevelMin )
        {
            postSensorEventMessage ( sensor_microphone_thread_id, SF_MESSAGE_EVENT_SENSOR_NEW_DATA, NULL );
        }

        prev_sound_level = sound_level;
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
                                    sound_level, ( sound_level - sound_level_difference ),
                                    abs ( sound_level_difference ) );
    }

    if ( bytesProcessed < 0 )
    {
        return 0;
    }

    return (unsigned int) bytesProcessed;
}

unsigned int sensor_microphone_formatFileHeader ( char * payload, size_t payloadLength )
{
    char * header = "current_level,previous_level,difference\n";
    snprintf ( payload, payloadLength, "%s", header );
    return strlen ( header );
}

unsigned int sensor_microphone_formatDataForFileLogging ( const event_sensor_payload_t * const eventPtr, char * payload,
                                                          size_t payloadLength )
{
    int bytesProcessed = 0;
    if ( eventPtr != NULL )
    {
        bytesProcessed = snprintf ( payload, payloadLength, "%d,%d,%d\n", sound_level,
                                    ( sound_level - sound_level_difference ), abs ( sound_level_difference ) );
    }

    if ( bytesProcessed < 0 )
    {
        return 0;
    }

    return (unsigned int) bytesProcessed;
}


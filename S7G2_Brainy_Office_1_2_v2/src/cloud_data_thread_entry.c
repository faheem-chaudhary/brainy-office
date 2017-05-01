#include "cloud_data_thread.h"
#include "event_system_api.h"
#include "event_sensor_api.h"
#include "event_config_api.h"
#include "commons.h"
#include "cloud_medium1_adapter.h"
#include "sf_message.h"
#include "tx_api.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
#define __CLOUD_ADAPTER_MEDIUM_ONE__    1
#define __CLOUD_ADAPTER_BUG_LABS__      2

#define __CLOUD_SERVICE_CONNECTOR__ __CLOUD_ADAPTER_MEDIUM_ONE__

#define CURR_NET_STATUS_BIT_MASK    0x02
#define CURR_CLOUD_PROV_BIT_MASK    0x01
#define CURR_CLOUD_INIT_BIT_MASK    0x04
#define isCloudConnectable  ((cloudInitImpl!=NULL)&&((cloudAvailabilityStatus&0x3)==0x3))
#define isCloudAvailable    ((cloudPublishImpl!=NULL)&&((cloudAvailabilityStatus&0x7)==0x7))

#define CLOUD_MESSAGE_BUFFER_LENGTH   256
#define CONFIG_FILE "config.txt"

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
extern FX_MEDIA * gp_media;

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
///                        -- None --                        ///

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
static ULONG cloud_data_thread_wait = 5;
static uint8_t cloud_data_thread_id;
static stringDataFunction_t cloudPublishImpl = NULL;
static stringDataFunction_t cloudConfigImpl = NULL;
static stringDataFunction_t cloudInitImpl = NULL;
static cloudHouseKeepFunction_t cloudHouseKeepImpl = NULL;
static sensorFormatFunction_t registeredCloudPublishers [ MAX_THREAD_COUNT ] =
{ NULL };
static int cloudAvailabilityStatus = 0;
static char cloudMessageBuffer [ CLOUD_MESSAGE_BUFFER_LENGTH ];

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
void cloud_data_thread_entry ( void );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
static bool cloud_data_thread_provisionFromUsbConfiguration ();
static void cloud_data_thread_processConfigMessage ( event_config_payload_t *configEventMsg );
static void cloud_data_thread_processSensorMessage ( event_sensor_payload_t *sensorEventMsg );
static void cloud_data_thread_processSystemMessage ( event_system_payload_t *systemEventMsg );

/// --  END OF: Static (file scope) Function Declarations -- ///

/* Cloud Data Thread entry function */
void cloud_data_thread_entry ( void )
{
    cloud_data_thread_id = getUniqueThreadId ();

#if ( __CLOUD_SERVICE_CONNECTOR__ == __CLOUD_ADAPTER_MEDIUM_ONE__ )

    cloudPublishImpl = mediumOnePublishImpl;
    cloudConfigImpl = mediumOneConfigImpl;
    cloudInitImpl = mediumOneInitImpl;
    cloudHouseKeepImpl = mediumOneHouseKeepImpl;

#elif ( __CLOUD_SERVICE_CONNECTOR__ == __CLOUD_ADAPTER_BUG_LABS__ )

#endif

    sf_message_header_t * message;
    ssp_err_t msgStatus;

    ULONG prevTime = 0, currTime = 0;
    ULONG houseKeepInterval = ( 60 * 100 ); // Every 60 seconds

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &cloud_data_thread_message_queue, (void **) &message, cloud_data_thread_wait );

        if ( msgStatus == SSP_SUCCESS )
        {
            switch ( message->event_b.class_code )
            {
                case SF_MESSAGE_EVENT_CLASS_CONFIG :
                    cloud_data_thread_processConfigMessage ( (event_config_payload_t *) message );
                    break;

                case SF_MESSAGE_EVENT_CLASS_SENSOR :
                    cloud_data_thread_processSensorMessage ( (event_sensor_payload_t *) message );
                    break;

                case SF_MESSAGE_EVENT_CLASS_SYSTEM :
                    cloud_data_thread_processSystemMessage ( (event_system_payload_t *) message );
                    break;
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
        else if ( msgStatus != SSP_ERR_MESSAGE_QUEUE_EMPTY )
        {
            // if any error other than empty queue
        }

        if ( cloudHouseKeepImpl != NULL )
        {
            currTime = tx_time_get ();
            if ( currTime - prevTime >= houseKeepInterval )
            {
                prevTime = currTime;
                cloudHouseKeepImpl ();
            }
        }
    }
}

void registerSensorForCloudPublish ( uint8_t threadId, sensorFormatFunction_t cloudDataFormatter )
{
    if ( threadId < MAX_THREAD_COUNT )
    {
        registeredCloudPublishers [ threadId ] = cloudDataFormatter;
    }
}

void cloud_data_thread_processConfigMessage ( event_config_payload_t *configEventMsg )
{
    switch ( configEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_CONFIG_CLOUD_PROVISION :
            break;

        case SF_MESSAGE_EVENT_CONFIG_SET_CLOUD_TRANSMITTER :
            break;
    }
}

void cloud_data_thread_processSensorMessage ( event_sensor_payload_t *sensorEventMsg )
{
    switch ( sensorEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_SENSOR_NEW_DATA :
            if ( isCloudAvailable )
            {
                if ( registeredCloudPublishers [ sensorEventMsg->senderId ] != NULL )
                {
                    // Get the data Payload
                    unsigned int dataLength = registeredCloudPublishers [ sensorEventMsg->senderId ] (
                            sensorEventMsg, cloudMessageBuffer,
                            CLOUD_MESSAGE_BUFFER_LENGTH );

                    if ( dataLength > 0 )
                    {
                        // publish to Implemented Adapter
                        cloudPublishImpl ( cloudMessageBuffer, dataLength );
                    }
                }
            }
            break;
    }
}

void cloud_data_thread_processSystemMessage ( event_system_payload_t *systemEventMsg )
{
    bool existingCloudConnectable = isCloudConnectable;
    bool existingCloudAvailability = isCloudAvailable;

    switch ( systemEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_SYSTEM_NETWORK_AVAILABLE :
            cloudAvailabilityStatus |= CURR_NET_STATUS_BIT_MASK;
            break;

        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY :
            //TODO: Provision from USB
            if ( cloud_data_thread_provisionFromUsbConfiguration () == true )
            {
                cloudAvailabilityStatus |= CURR_CLOUD_PROV_BIT_MASK;
            }
            else
            {
                cloudAvailabilityStatus &= ~CURR_CLOUD_PROV_BIT_MASK;
            }
            break;

        case SF_MESSAGE_EVENT_SYSTEM_NETWORK_DISCONNECTED :
            cloudAvailabilityStatus &= ~CURR_NET_STATUS_BIT_MASK;
            break;

        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED :
            break;
    }

    if ( existingCloudConnectable == false && isCloudConnectable )
    {
        if ( cloudInitImpl ( NULL, 0 ) )
        {
            cloudAvailabilityStatus |= CURR_CLOUD_INIT_BIT_MASK;
            // publish Cloud available system message

            postSystemEventMessage ( cloud_data_thread_id, SF_MESSAGE_EVENT_SYSTEM_CLOUD_AVAILABLE );
        }
        else
        {
            cloudAvailabilityStatus &= ~CURR_CLOUD_INIT_BIT_MASK;
        }
    }

    if ( existingCloudAvailability == true && !isCloudAvailable )
    {
        // publish Cloud NOT available system message
        postSystemEventMessage ( cloud_data_thread_id, SF_MESSAGE_EVENT_SYSTEM_CLOUD_DISCONNECTED );
    }
}

bool cloud_data_thread_provisionFromUsbConfiguration ()
{
    UINT keysRead = 0;
    FX_FILE file;
    UINT status = FX_SUCCESS;

    const ULONG bufferSize = 512;
    char configurationBuffer [ bufferSize ];
    ULONG bytesRead;

    if ( cloudConfigImpl != NULL )
    {
        status = fx_file_open ( gp_media, &file, CONFIG_FILE, FX_OPEN_FOR_READ );

        if ( status == FX_SUCCESS )
        {
            if ( fx_file_read ( &file, configurationBuffer, bufferSize, &bytesRead ) == FX_SUCCESS )
            {
                // bytesRead will contain the number of bytes read, from this call
                keysRead = cloudConfigImpl ( configurationBuffer, bytesRead );
            }

            fx_file_close ( &file );
        }
    }

    return ( keysRead > 0 );
}

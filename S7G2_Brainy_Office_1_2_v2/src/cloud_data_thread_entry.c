#include "cloud_data_thread.h"
#include "event_system_api.h"
#include "event_sensor_api.h"
#include "event_config_api.h"
#include "commons.h"
#include "cloud_medium1_adapter.h"
#include "sf_message.h"
#include "tx_api.h"

bool configureUSBCloudProvisioning ();

void processConfigMessage ( event_config_payload_t *configEventMsg );
void processSensorMessage ( event_sensor_payload_t *sensorEventMsg );
void processSystemMessage ( event_system_payload_t *systemEventMsg );

ULONG cloud_data_thread_wait = 5;

uint8_t g_cloud_data_thread_id;

/* Cloud Data Thread entry function */
void cloud_data_thread_entry ( void )
{
    g_cloud_data_thread_id = getUniqueThreadId ();

    setCloudImplementationFunctions ( mediumOneConfigImpl, mediumOneInitImpl, mediumOnePublishImpl );

    sf_message_header_t * message;
    ssp_err_t msgStatus;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &cloud_data_thread_message_queue, (void **) &message, cloud_data_thread_wait );

        if ( msgStatus == SSP_SUCCESS )
        {
            // TODO: Process System Message here
            switch ( message->event_b.class_code )
            {
                case SF_MESSAGE_EVENT_CLASS_CONFIG :
                    processConfigMessage ( (event_config_payload_t *) message );
                    break;

                case SF_MESSAGE_EVENT_CLASS_SENSOR :
                    processSensorMessage ( (event_sensor_payload_t *) message );
                    break;

                case SF_MESSAGE_EVENT_CLASS_SYSTEM :
                    processSystemMessage ( (event_system_payload_t *) message );
                    break;
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
        else if ( msgStatus != SSP_ERR_MESSAGE_QUEUE_EMPTY )
        {
            // if any error other than empty queue
        }
    }
}

stringDataFunction g_cloudPublishImpl = NULL;
stringDataFunction g_cloudConfigImpl = NULL;
stringDataFunction g_cloudInitImpl = NULL;

sensorFormatFunction g_registeredSenders [ MAX_THREAD_COUNT ] =
{ NULL };

extern FX_MEDIA * gp_media;

void registerSensorForCloudPublish ( uint8_t threadId, sensorFormatFunction cloudDataFormatter )
{
    if ( threadId < MAX_THREAD_COUNT )
    {
        g_registeredSenders [ threadId ] = cloudDataFormatter;
    }
}

void setCloudImplementationFunctions ( stringDataFunction configImpl, stringDataFunction initImpl,
                                       stringDataFunction publishImpl )
{
    g_cloudPublishImpl = publishImpl;
    g_cloudConfigImpl = configImpl;
    g_cloudInitImpl = initImpl;
}

void processConfigMessage ( event_config_payload_t *configEventMsg )
{
    switch ( configEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_CONFIG_CLOUD_PROVISION :
            break;

        case SF_MESSAGE_EVENT_CONFIG_SET_CLOUD_TRANSMITTER :
            break;
    }
}

#define CURR_NET_STATUS_BIT_MASK    0x02
#define CURR_CLOUD_PROV_BIT_MASK    0x01
#define CURR_CLOUD_INIT_BIT_MASK    0x04

int g_cloudAvailabilityStatus = 0;

#define isCloudConnectable  ((g_cloudInitImpl!=NULL)&&((g_cloudAvailabilityStatus&0x3)==0x3))
#define isCloudAvailable    ((g_cloudPublishImpl!=NULL)&&((g_cloudAvailabilityStatus&0x7)==0x7))

#define BUFFER_LENGTH   256
char buffer [ BUFFER_LENGTH ];

void processSensorMessage ( event_sensor_payload_t *sensorEventMsg )
{
    switch ( sensorEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_SENSOR_NEW_DATA :
            if ( isCloudAvailable )
            {
                if ( g_registeredSenders [ sensorEventMsg->senderId ] != NULL )
                {
                    // Get the data Payload
                    unsigned int dataLength = g_registeredSenders [ sensorEventMsg->senderId ] ( sensorEventMsg, buffer,
                    BUFFER_LENGTH );

                    if ( dataLength > 0 )
                    {
                        // publish to Implemented Adapter
                        g_cloudPublishImpl ( buffer, dataLength );
                    }
                }
            }
            break;
    }
}

void processSystemMessage ( event_system_payload_t *systemEventMsg )
{
    bool existingCloudConnectable = isCloudConnectable;
    bool existingCloudAvailability = isCloudAvailable;

    switch ( systemEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_SYSTEM_NETWORK_AVAILABLE :
            g_cloudAvailabilityStatus |= CURR_NET_STATUS_BIT_MASK;
            break;

        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY :
            //TODO: Provision from USB
            if ( configureUSBCloudProvisioning () == true )
            {
                g_cloudAvailabilityStatus |= CURR_CLOUD_PROV_BIT_MASK;
            }
            else
            {
                g_cloudAvailabilityStatus &= ~CURR_CLOUD_PROV_BIT_MASK;
            }
            break;

        case SF_MESSAGE_EVENT_SYSTEM_NETWORK_DISCONNECTED :
            g_cloudAvailabilityStatus &= ~CURR_NET_STATUS_BIT_MASK;
            break;

        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED :
            break;
    }

    if ( existingCloudConnectable == false && isCloudConnectable )
    {
        if ( g_cloudInitImpl ( NULL, 0 ) )
        {
            g_cloudAvailabilityStatus |= CURR_CLOUD_INIT_BIT_MASK;
            // publish Cloud available system message

            postSystemEventMessage ( g_cloud_data_thread_id, SF_MESSAGE_EVENT_SYSTEM_CLOUD_AVAILABLE );
        }
        else
        {
            g_cloudAvailabilityStatus &= ~CURR_CLOUD_INIT_BIT_MASK;
        }
    }

    if ( existingCloudAvailability == true && !isCloudAvailable )
    {
        // publish Cloud NOT available system message
        postSystemEventMessage ( g_cloud_data_thread_id, SF_MESSAGE_EVENT_SYSTEM_CLOUD_DISCONNECTED );
    }
}

#define CONFIG_FILE "config.txt"

bool configureUSBCloudProvisioning ()
{
    UINT keysRead = 0;
    FX_FILE file;
    UINT status = FX_SUCCESS;

    const ULONG bufferSize = 512;
    char configurationBuffer [ bufferSize ];
    ULONG bytesRead;

    if ( g_cloudConfigImpl != NULL )
    {
        status = fx_file_open ( gp_media, &file, CONFIG_FILE, FX_OPEN_FOR_READ );

        if ( status == FX_SUCCESS )
        {
            if ( fx_file_read ( &file, configurationBuffer, bufferSize, &bytesRead ) == FX_SUCCESS )
            {
                // bytesRead will contain the number of bytes read, from this call
                keysRead = g_cloudConfigImpl ( configurationBuffer, bytesRead );
            }

            fx_file_close ( &file );
        }
    }

    return ( keysRead > 0 );
}

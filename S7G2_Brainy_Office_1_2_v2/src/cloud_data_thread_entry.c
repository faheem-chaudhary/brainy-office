#include "cloud_data_thread.h"
#include "event_system_api.h"
#include "event_sensor_api.h"
#include "event_config_api.h"
#include "commons.h"
#include "cloud_medium1_adapter.h"
#include "sf_message.h"

void processConfigMessage ( event_config_payload_t *configEventMsg );
void processSensorMessage ( event_sensor_payload_t *sensorEventMsg );
//void processSystemMessage ( event_system_payload_t *systemEventMsg );

stringDataFunction registeredSenders [ MAX_THREAD_COUNT ] =
{ NULL };

ULONG cloud_data_thread_wait = 5;

stringDataFunction g_cloudPublishImpl = NULL;
//stringDataFunction g_cloudConfigImpl = NULL;
stringDataFunction g_cloudInitImpl = NULL;

extern TX_THREAD cloud_data_thread;

/* Cloud Data Thread entry function */
void cloud_data_thread_entry ( void )
{
//    setCloudPublishingFunction ( mediumOneCloudImpl, mediumOneConfigImpl, mediumOneInitImpl );

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

//                case SF_MESSAGE_EVENT_CLASS_SYSTEM :
//                    processSystemMessage ( (event_system_payload_t *) message );
//                    break;
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
        else if ( msgStatus != SSP_ERR_MESSAGE_QUEUE_EMPTY )
        {
            // if any error other than empty queue
        }
    }
}

stringDataFunction setCloudPublishingFunction ( stringDataFunction publishImpl, stringDataFunction configImpl,
                                                stringDataFunction initImpl )
{
    stringDataFunction existingPtr = g_cloudPublishImpl;
    g_cloudPublishImpl = publishImpl;
//    g_cloudConfigImpl = configImpl;
    g_cloudInitImpl = initImpl;
    return existingPtr;
}

#define CURR_NET_STATUS_BIT_MASK    0x02
#define CURR_CLOUD_PROV_BIT_MASK    0x01
#define CURR_CLOUD_INIT_BIT_MASK    0x04

int g_cloudAvailabilityStatus = 0;

bool isCloudAvailable ();
bool isCloudConnectable ();

inline bool isCloudAvailable ()
{
    return ( ( g_cloudPublishImpl != NULL ) && ( ( g_cloudAvailabilityStatus & 0x7 ) == 0x7 ) );
}

inline bool isCloudConnectable ()
{
    return ( ( g_cloudInitImpl != NULL ) && ( ( g_cloudAvailabilityStatus & 0x3 ) == 0x3 ) );
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

#define BUFFER_LENGTH   256
char buffer [ BUFFER_LENGTH ];

void processSensorMessage ( event_sensor_payload_t *sensorEventMsg )
{
    uint8_t threadId = 0;
    switch ( sensorEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_SENSOR_NEW_DATA :
            if ( isCloudAvailable () )
            {
                threadId = ( ( sensorEventMsg->sender->tx_thread_id ) % MAX_THREAD_COUNT );
                if ( registeredSenders [ threadId ] != NULL )
                {
                    // Get the data Payload
                    unsigned int dataLength = registeredSenders [ threadId ] ( buffer, BUFFER_LENGTH );

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

//void processSystemMessage ( event_system_payload_t *systemEventMsg )
//{
//    bool existingCloudConnectable = isCloudConnectable ();
//    bool existingCloudAvailability = isCloudAvailable ();
//
//    switch ( systemEventMsg->header.event_b.code )
//    {
//        case SF_MESSAGE_EVENT_SYSTEM_NETWORK_AVAILABLE :
//            g_cloudAvailabilityStatus |= CURR_NET_STATUS_BIT_MASK;
//            break;
//
//        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY :
//            //TODO: Provision from USB
//            if ( configureUSBCloudProvisioning () == true )
//            {
//                g_cloudAvailabilityStatus |= CURR_CLOUD_PROV_BIT_MASK;
//            }
//            else
//            {
//                g_cloudAvailabilityStatus &= ~CURR_CLOUD_PROV_BIT_MASK;
//            }
//            break;
//
//        case SF_MESSAGE_EVENT_SYSTEM_NETWORK_DISCONNECTED :
//            g_cloudAvailabilityStatus &= ~CURR_NET_STATUS_BIT_MASK;
//            break;
//
//        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED :
//            break;
//    }
//
//    if ( existingCloudConnectable == false && isCloudConnectable () )
//    {
//        if ( g_cloudInitImpl ( NULL, 0 ) )
//        {
//            g_cloudAvailabilityStatus |= CURR_CLOUD_INIT_BIT_MASK;
//            // publish Cloud available system message
////            postSystemEventMessage ( &cloud_data_thread, SF_MESSAGE_EVENT_CLASS_SYSTEM,
////                    SF_MESSAGE_EVENT_ );
//        }
//        else
//        {
//            g_cloudAvailabilityStatus &= ~CURR_CLOUD_INIT_BIT_MASK;
//        }
//    }
//
//    if ( existingCloudConnectable == true && !isCloudAvailable () )
//    {
//        // publish Cloud NOT available system message
//    }
//}


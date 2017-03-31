#include "sys_main_thread.h"
#include "commons.h"
#include "cloud_medium1_adapter.h"

#define APP_USB_INS_FLAG    (0x1UL)
#define APP_USB_REM_FLAG    (0x2UL)

UINT usb_host_event_notification_callback ( ULONG event, UX_HOST_CLASS * class, VOID * instance );

FX_MEDIA * gp_media = FX_NULL;

uint8_t get_sys_main_thread_Id ();
uint8_t sys_main_thread_id = MAX_THREAD_COUNT * 2;

uint8_t get_sys_main_thread_Id ()
{
    if ( sys_main_thread_id > MAX_THREAD_COUNT )
    {
        sys_main_thread_id = getUniqueThreadId ();
    }
    return sys_main_thread_id;
}

//extern TX_THREAD cloud_data_thread;
//extern TX_THREAD sensor_air_quality_thread;
//extern TX_THREAD sensor_color_proximity_thread;
//extern TX_THREAD sensor_humidity_temperature_thread;
//extern TX_THREAD sensor_lightning_thread;
//extern TX_THREAD sensor_microphone_thread;
//extern TX_THREAD sensor_vibration_compass_thread;
//
//extern TX_THREAD sys_blinker_thread;
//extern TX_THREAD sys_display_thread;
//extern TX_THREAD sys_file_logger_thread;
//extern TX_THREAD sys_main_thread;
//extern TX_THREAD sys_network_thread;
//extern TX_THREAD sys_touch_thread;

ULONG sys_main_thread_wait = 10;

// System Main Thread entry function
void sys_main_thread_entry ( void )
{
//    setCloudImplementationFunctions ( mediumOneConfigImpl, mediumOneInitImpl, mediumOnePublishImpl );

    while ( 1 )
    {
//        msgStatus = messageQueuePend ( &sys_main_thread_message_queue, (void **) &message, sys_main_thread_wait );
//
//        if ( msgStatus == SSP_SUCCESS )
//        {
//            if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_TOUCH )
//            {
//                if ( nextThreadToActivate < CURRENT_ACTIVE_THREADS )
//                {
//                    tx_thread_resume ( threadPointerArray [ nextThreadToActivate ] );
//
//                    nextThreadToActivate++;
//                }
//            }
//
//            messageQueueReleaseBuffer ( (void **) &message );
//        }
//        else if ( msgStatus != SSP_ERR_MESSAGE_QUEUE_EMPTY )
//        {
//            // if any error other than empty queue
//        }
        tx_thread_sleep ( 10 );
    }
}

static UX_HOST_CLASS_STORAGE * p_usb_storage = UX_NULL;
static UX_HOST_CLASS_STORAGE_MEDIA * p_usb_storage_media = UX_NULL;
UINT usb_host_event_notification_callback ( ULONG event, UX_HOST_CLASS * class, VOID * instance )
{
    UINT status = TX_SUCCESS;
    UX_HOST_CLASS * p_storage_class = NULL;

// Check if there is a device insertion.
    if ( UX_DEVICE_INSERTION == event )
    {
        // Get the storage class.
        status = ux_host_stack_class_get ( _ux_system_host_class_storage_name, &p_storage_class );

        if ( UX_SUCCESS == status )
        {
            // Check if we got a storage class device.
            if ( p_storage_class == class )
            {
                // We only get the first media attached to the class container.
                if ( FX_NULL == gp_media )// Is this check needed?
                {
                    p_usb_storage = instance;
                    p_usb_storage_media = class->ux_host_class_media;
                    gp_media = &p_usb_storage_media->ux_host_class_storage_media;
                }

                postSystemEventMessage ( get_sys_main_thread_Id (), SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY );
            }
        }
    }

    // Check if some device is removed.
    else if ( UX_DEVICE_REMOVAL == event )
    {
        //  Check if the storage device is removed.
        if ( instance == p_usb_storage )
        {
            // Set pointers to null, so that the demo thread will not try to access the media any more.
            gp_media = FX_NULL;
            p_usb_storage_media = UX_NULL;
            p_usb_storage = UX_NULL;

            postSystemEventMessage ( get_sys_main_thread_Id (), SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED );
        }
    }

    else
    {
        // Default case, nothing to do
    }

    return ( status );
}

uint8_t g_currentThreadId = 0;
uint8_t getUniqueThreadId ()
{
    uint8_t threadId;
    tx_mutex_get ( &g_generate_thread_id_mutex, TX_WAIT_FOREVER );
    threadId = ( ( g_currentThreadId++ ) % MAX_THREAD_COUNT );
    tx_mutex_put (&g_generate_thread_id_mutex);
    return threadId;
}

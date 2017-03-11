#include "sys_main_thread.h"
#include "commons.h"
extern TX_THREAD sys_main_thread;

UINT usb_host_event_notification_callback ( ULONG event, UX_HOST_CLASS * class, VOID * instance );

FX_MEDIA * gp_media = FX_NULL;
static UX_HOST_CLASS_STORAGE * p_usb_storage = UX_NULL;
static UX_HOST_CLASS_STORAGE_MEDIA * p_usb_storage_media = UX_NULL;

/* System Main Thread entry function */
void sys_main_thread_entry ( void )
{
    sf_message_header_t * message;
    ssp_err_t msgStatus = SSP_SUCCESS;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sys_main_thread_message_queue, (void **) &message, TX_WAIT_FOREVER );

        if ( msgStatus == SSP_SUCCESS )
        {
            if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_SYSTEM )
            {
                if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_NETWORK_AVAILABLE )
                {
                    while ( gp_media == FX_NULL )
                    {
                        tx_thread_sleep ( 10 );
                    }
                    postSystemEventMessage ( &sys_main_thread, SF_MESSAGE_EVENT_CLASS_SYSTEM,
                                             SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY );

                    while ( gp_media != FX_NULL )
                    {
                        tx_thread_sleep ( 10 );
                    }
                    postSystemEventMessage ( &sys_main_thread, SF_MESSAGE_EVENT_CLASS_SYSTEM,
                                             SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED );
                }
            }
        }
    }
}

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
        }
    }

    else
    {
        // Default case, nothing to do
    }

    return ( status );
}

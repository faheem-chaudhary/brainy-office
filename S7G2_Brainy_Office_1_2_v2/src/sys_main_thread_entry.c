#include "sys_main_thread.h"
#include "commons.h"
#include "cloud_medium1_adapter.h"

#define APP_USB_INS_FLAG    (0x1UL)
#define APP_USB_REM_FLAG    (0x2UL)

UINT usb_host_event_notification_callback ( ULONG event, UX_HOST_CLASS * class, VOID * instance );

FX_MEDIA * gp_media = FX_NULL;

// System Main Thread entry function
void sys_main_thread_entry ( void )
{
    setCloudFunctions ( mediumOneConfigImpl, mediumOneInitImpl, mediumOnePublishImpl );

    while ( 1 )
    {
        tx_thread_sleep ( 10 );
    }
}

static UX_HOST_CLASS_STORAGE * p_usb_storage = UX_NULL;
static UX_HOST_CLASS_STORAGE_MEDIA * p_usb_storage_media = UX_NULL;
UINT usb_host_event_notification_callback ( ULONG event, UX_HOST_CLASS * class, VOID * instance )
{
    UINT status = TX_SUCCESS;
    UX_HOST_CLASS * p_storage_class = NULL;
    ULONG flags = 0;

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

                postSystemEventMessage ( SF_MESSAGE_EVENT_CLASS_SYSTEM,
                        SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY );
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

            postSystemEventMessage ( SF_MESSAGE_EVENT_CLASS_SYSTEM,
                    SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED );
        }
    }

    else
    {
        // Default case, nothing to do
    }

    return ( status );
}

#include "sys_main_thread.h"
#include "commons.h"
#include "cloud_medium1_adapter.h"

#define APP_USB_INS_FLAG    (0x1UL)
#define APP_USB_REM_FLAG    (0x2UL)

// Configuration function to set the Cloud Implementation Adapter
// @param [in]  cloudImpl   Implementation Function Pointer to publish payload to cloud.  Use NULL to remove the existing implementation.
// @param [in]  configImpl  Implementation Function Pointer to setup Cloud Connection.  Use NULL to remove the existing implementation.
// @param [in]  initImpl    Implementation Function Pointer to initialize Cloud Connection.  Use NULL to remove the existing implementation.  This function will be called right after successful execution of configImpl and is not NULL.  This function will also be called whenever network connection is successfully established.
// @return                  NULL or existing Cloud Implementation function pointer (if any set in the prior call to this function, NOT the Configuration Setup Impl)
stringDataFunction setCloudPublishingFunction ( stringDataFunction cloudImpl, stringDataFunction configImpl,
                                                stringDataFunction initImpl );

//TODO: This function needs refactoring ... break down the reading part and send the bits to M1 configuration
bool configureUSBCloudProvisioning ();

extern TX_THREAD sys_main_thread;

stringDataFunction g_cloudConfigImpl = mediumOneConfigImpl;
UINT usb_host_event_notification_callback ( ULONG event, UX_HOST_CLASS * class, VOID * instance );

FX_MEDIA * gp_media = FX_NULL;

/// System Main Thread entry function
void sys_main_thread_entry ( void )
{
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

                //Clear the APP_USB_REM_FLAG event flag if it wasn't cleared by the app and set the APP_USB_INS_FLAG
                postSystemEventMessage ( &sys_main_thread, SF_MESSAGE_EVENT_CLASS_SYSTEM,
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

            postSystemEventMessage ( &sys_main_thread, SF_MESSAGE_EVENT_CLASS_SYSTEM,
                    SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED );
        }
    }

    else
    {
        // Default case, nothing to do
    }

    return ( status );
}
/*
 #if (USBX_CONFIGURED)
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
 #else
 bool configureUSBCloudProvisioning ()
 {
 UINT keysRead = 0;
 const ULONG bufferSize = 512;
 char configurationBuffer [ bufferSize ];

 if ( g_cloudConfigImpl != NULL )
 {
 sprintf ( configurationBuffer, "name=desktop-kit-1\r\n"
 "api_key=WG46JL5TPKJ3Q272SDCEJDJQGQ4DEOJZGM2GEZBYGRQTAMBQ\r\n"
 "project_id=o3adQcvIn0Q\r\n"
 "user_id=kHgc2feq1bg\r\n"
 "password=Dec2kPok\r\n"
 "host=mqtt2.mediumone.com\r\n"
 "port=61619\r\n" );
 keysRead = g_cloudConfigImpl ( configurationBuffer, bufferSize );
 if ( keysRead > 0 && g_cloudInitImpl != NULL )
 {
 g_cloudInitImpl ( NULL, 0 );
 }
 }

 return ( keysRead > 0 );
 }
 #endif
 */

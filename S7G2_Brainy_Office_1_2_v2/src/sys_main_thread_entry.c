#include "sys_main_thread.h"
#include "commons.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
///                        -- None --                        ///

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
FX_MEDIA * gp_media = FX_NULL;

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
///                        -- None --                        ///

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
static uint8_t sys_main_thread_id = MAX_THREAD_COUNT * 2;
static UX_HOST_CLASS_STORAGE * p_usb_storage = UX_NULL;
static UX_HOST_CLASS_STORAGE_MEDIA * p_usb_storage_media = UX_NULL;
static ULONG sys_main_thread_wait = 10;
static TX_MUTEX generate_thread_id_mutex;

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
void sys_main_thread_entry ( void );
UINT usb_host_event_notification_callback ( ULONG event, UX_HOST_CLASS * class, VOID * instance );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
static uint8_t get_sys_main_thread_Id ();

/// --  END OF: Static (file scope) Function Declarations -- ///

// System Main Thread entry function
void sys_main_thread_entry ( void )
{
    tx_mutex_create ( &generate_thread_id_mutex, (CHAR *) "Generate Thread ID Mutex", TX_NO_INHERIT );

    extern TX_THREAD sensor_microphone_thread;
    extern TX_THREAD sensor_temperature_thread;

    g_button_s4_irq11.p_api->open ( g_button_s4_irq11.p_ctrl, g_button_s4_irq11.p_cfg );
    g_button_s4_irq11.p_api->enable ( g_button_s4_irq11.p_ctrl );

    g_button_s5_irq10.p_api->open ( g_button_s5_irq10.p_ctrl, g_button_s5_irq10.p_cfg );
    g_button_s5_irq10.p_api->enable ( g_button_s5_irq10.p_ctrl );

    tx_thread_resume ( &sensor_temperature_thread );
    tx_thread_sleep ( 100 ); // wait for a second
    tx_thread_resume ( &sensor_microphone_thread );

    while ( 1 )
    {
        tx_thread_sleep ( sys_main_thread_wait );
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

uint8_t getUniqueThreadId ()
{
    static uint8_t currentThreadId = 0;

    uint8_t threadId;
    tx_mutex_get ( &generate_thread_id_mutex, TX_WAIT_FOREVER );
    threadId = ( ( currentThreadId++ ) % MAX_THREAD_COUNT );
    tx_mutex_put ( &generate_thread_id_mutex );
    return threadId;
}

void button_s4_irq11_callback ( external_irq_callback_args_t * p_args )
{
    SSP_PARAMETER_NOT_USED ( p_args );
    postSystemEventMessage ( get_sys_main_thread_Id (), SF_MESSAGE_EVENT_SYSTEM_BUTTON_S4_PRESSED );
}

void button_s5_irq10_callback ( external_irq_callback_args_t * p_args )
{
    SSP_PARAMETER_NOT_USED ( p_args );
    postSystemEventMessage ( get_sys_main_thread_Id (), SF_MESSAGE_EVENT_SYSTEM_BUTTON_S5_PRESSED );
}

uint8_t get_sys_main_thread_Id ()
{
    if ( sys_main_thread_id > MAX_THREAD_COUNT )
    {
        sys_main_thread_id = getUniqueThreadId ();
    }
    return sys_main_thread_id;
}

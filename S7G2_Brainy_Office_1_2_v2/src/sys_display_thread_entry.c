#include "sys_display_thread.h"
#include "commons.h"

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
static ULONG sys_display_thread_wait = 10;

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
void sys_display_thread_entry ( void );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
///                        -- None --                        ///

/// --  END OF: Static (file scope) Function Declarations -- ///

/* System Display Thread entry function */
void sys_display_thread_entry ( void )
{
    sf_message_header_t * message;
    ssp_err_t msgStatus;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sys_display_thread_message_queue, (void **) &message, sys_display_thread_wait );

        if ( msgStatus == SSP_SUCCESS )
        {
            if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_SYSTEM )
            {
                //TODO: anything related to System Message Processing goes here
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
}

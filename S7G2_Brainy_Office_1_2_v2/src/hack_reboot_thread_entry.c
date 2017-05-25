#include "hack_reboot_thread.h"

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
static ULONG hack_reboot_thread_wait = 60 * 60 * 100; // one hour 360000 ticks

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
///                        -- None --                        ///
void hack_reboot_thread_entry ( void );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
///                        -- None --                        ///

/// --  END OF: Static (file scope) Function Declarations -- ///

/**
 * This is a hack to reboot the kit every elapsed time
 **/
/* Hack Reboot Thread entry function */

void hack_reboot_thread_entry ( void )
{
    tx_thread_sleep ( hack_reboot_thread_wait );
    NVIC_SystemReset ();
}

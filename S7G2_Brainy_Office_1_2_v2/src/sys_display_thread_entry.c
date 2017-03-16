#include "sys_display_thread.h"

/* System Display Thread entry function */
void sys_display_thread_entry ( void )
{
    /* TODO: add your own code here */
    while ( 1 )
    {
        tx_thread_sleep ( 1 );
    }
}

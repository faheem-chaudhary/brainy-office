#include "sys_touch_thread.h"

/* System Touch Thread entry function */
void sys_touch_thread_entry ( void )
{
    /* TODO: add your own code here */
    while ( 1 )
    {
        tx_thread_sleep ( 10 );
    }
}

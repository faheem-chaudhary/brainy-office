#include "sys_touch_thread.h"
#include "commons.h"

ULONG sys_touch_thread_wait = 10;

/* System Touch Thread entry function */
void sys_touch_thread_entry ( void )
{
    /* TODO: add your own code here */

    sf_message_header_t * message;
    ssp_err_t msgStatus;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sys_touch_thread_message_queue, (void **) &message, sys_touch_thread_wait );

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

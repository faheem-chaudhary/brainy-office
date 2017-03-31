#include "sensor_color_proximity_thread.h"
#include "commons.h"

ULONG sensor_color_proximity_thread_wait = 10;

/* Sensor Color and Proximity (AMS TMD3782) Thread entry function */
void sensor_color_proximity_thread_entry ( void )
{
    sf_message_header_t * message;
    ssp_err_t msgStatus;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sensor_color_proximity_thread_message_queue, (void **) &message,
                                       sensor_color_proximity_thread_wait );

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

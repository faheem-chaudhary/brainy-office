#include "sensor_air_quality_thread.h"
#include "commons.h"

ULONG sensor_air_quality_thread_wait = 10;

/* Sensor Air Quality (AMS iAQ) Thread entry function */
void sensor_air_quality_thread_entry ( void )
{
    sf_message_header_t * message;
    ssp_err_t msgStatus;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sensor_air_quality_thread_message_queue, (void **) &message,
                                       sensor_air_quality_thread_wait );

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


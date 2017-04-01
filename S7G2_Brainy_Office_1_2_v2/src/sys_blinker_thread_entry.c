#include "sys_blinker_thread.h"
#include "commons.h"

#define TOTAL_LED_COUNT 3

ULONG sys_blinker_thread_wait = 10;

void ledToggle ( LED _led );
void ledOn ( LED _led );
void ledOff ( LED _led );

void ledToggle ( LED _led )
{
    ioport_level_t ledValue;
    g_ioport.p_api->pinRead ( g_bsp_leds.p_leds [ _led ], &ledValue );
    g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ _led ], !ledValue );
}

void ledOn ( LED _led )
{
    g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ _led ], LED_ON );
}

void ledOff ( LED _led )
{
    g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ _led ], LED_OFF );
}

typedef enum
{
    OFF = 0, TOGGLE = 1, ON = 2,
} LedMode_t;

LedMode_t ledModes [ TOTAL_LED_COUNT ] =
{ OFF };

// System Blinker Thread entry function
void sys_blinker_thread_entry ( void )
{
    sf_message_header_t * message;
    ssp_err_t msgStatus;

//    ledModes [ RED ] = TOGGLE;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sys_blinker_thread_message_queue, (void **) &message, sys_blinker_thread_wait );

        if ( msgStatus == SSP_SUCCESS )
        {
            /// Connectivity --> AMBER
            /// OFF = Network Not connected
            /// Blink = Network Connected, Cloud not connected
            /// ON = Network and Cloud are both connected
            if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_SYSTEM )
            {
                switch ( message->event_b.code )
                {
                    case SF_MESSAGE_EVENT_SYSTEM_NETWORK_DISCONNECTED :
                        ledModes [ AMBER ]--;
                        break;

                    case SF_MESSAGE_EVENT_SYSTEM_NETWORK_AVAILABLE :
                        ledModes [ AMBER ]++;
                        break;

                    case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY :
                        ledModes [ AMBER ]++;
                        break;

                    case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED :
                        ledModes [ AMBER ]--;
                        break;

                    case SF_MESSAGE_EVENT_SYSTEM_BUTTON_S4_PRESSED :
                        ledModes [ RED ] = ON;
                        break;

                    case SF_MESSAGE_EVENT_SYSTEM_BUTTON_S5_PRESSED :
                        ledModes [ RED ] = OFF;
                        break;
                }
            }
            else if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_SENSOR )
            {
                ledModes [ GREEN ] = TOGGLE;
            }
            else if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_TOUCH )
            {
                if ( ledModes [ RED ] != ON )
                {
                    ledModes [ RED ] = ON;
                }
                else
                {
                    ledModes [ RED ] = OFF;
                }
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
        else if ( msgStatus != SSP_ERR_MESSAGE_QUEUE_EMPTY )
        {
            // if any error other than empty queue
        }

        for ( int i = 0; i < g_bsp_leds.led_count; i++ )
        {
            switch ( ledModes [ i ] )
            {
                case OFF :
                    ledOff ( i );
                    break;

                case ON :
                    ledOn ( i );
                    break;

                case TOGGLE :
                    ledToggle ( i );
                    break;
            }
        }
    }
}

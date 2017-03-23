/*
 * commons.h
 *
 *  Created on: Oct 12, 2016
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2016 Renesas Electronics America (REA) and Faheem Inayat
 */

#ifndef COMMONS_H_
    #define COMMONS_H_

//    #define DUMMY_AMS_DATA

    #include "bsp_pin_cfg.h"
    #include "bsp_api.h"
    #include "r_ioport_api.h"
    #include "hal_data.h"

    #define MAX_THREAD_COUNT    16
    #define USBX_CONFIGURED 1
extern const bsp_leds_t g_bsp_leds;

typedef enum
{
    RED = 1, AMBER = 2, BLUE = 0, GREEN = 0,
} LED;

    #define LED_ON  IOPORT_LEVEL_LOW
    #define LED_OFF IOPORT_LEVEL_HIGH

// Generic String Processing function signature.  All errors need to be silenced
// @param [in,out]  payload         - Data Buffer to perform processing on
// @param [in,out]  payloadLength   - Maximum number of bytes/chars of the 'payload' buffer
// @return          non-negative number of bytes/chars modified starting from the 'payload' buffer
typedef unsigned int (*stringDataFunction) ( char * payload, size_t payloadLength );

    #define applicationErrorTrap(status)    {\
                                                if ( status != SSP_SUCCESS ) \
                                                { \
                                                    g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ RED ], LED_ON ); \
                                                    g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ AMBER ], LED_OFF ); \
                                                    g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ GREEN ], LED_OFF ); \
                                                    __asm("BKPT"); \
                                                } \
                                            }

    #define APP_ERR_TRAP(a) applicationErrorTrap(a);

    #ifndef min
        #define min(a,b) ((a) < (b))? (a) : (b)
    #endif

    #ifndef max
        #define max(a,b) ((a) > (b))? (a) : (b)
    #endif

// Utility function to wrap on the existing pend function.  Just for the sake of ease and avoid copy-paste errors
// @param [in]      queue               MQ to pend on
// @param [in,out]  message             Message Payload Buffer Pointer
// @param [in]      wait_option         Pending wait option -- USE TX_WAIT_OPTIONS
// @return SSP_SUCESS                   Message Pending was successful
// @return SSP_ERR_ASSERTION            msg buffer pointer is NULL
// @return SSP_ERR_NOT_OPEN             Messaging Framework module is not open yet
// @return SSP_ERR_MESSAGE_QUEUE_EMPTY  Queue is empty (timeout occurred before receiving the message, if timeout is provided)
// @return SSP_ERR_TIMEOUT              OS Service call return timed out
// @return SSP_ERR_INTERNAL             OS Service call fails
ssp_err_t messageQueuePend ( TX_QUEUE * queue, void ** message, ULONG wait_option );
ssp_err_t messageQueuePost ( void ** message );
ssp_err_t messageQueueAcquireBuffer ( void ** message );
ssp_err_t messageQueueReleaseBuffer ( void ** message );
ssp_err_t postSystemEventMessage ( sf_message_event_class_t eventClass, sf_message_event_t eventCode );
ssp_err_t postSensorEventMessage ( sf_message_event_class_t eventClass, sf_message_event_t eventCode, void * dataPtr );

// Configuration function to set the Cloud Implementation Adapter
// @param [in]  configImpl  Implementation Function Pointer to setup Cloud Connection.  Use NULL to remove the existing implementation.
// @param [in]  initImpl    Implementation Function Pointer to initialize Cloud Connection.  Use NULL to remove the existing implementation.  This function will be called right after successful execution of configImpl and is not NULL.  This function will also be called whenever network connection is successfully established.
// @param [in]  publishImpl   Implementation Function Pointer to publish payload to cloud.  Use NULL to remove the existing implementation.
void setCloudFunctions ( stringDataFunction configImpl, stringDataFunction initImpl,
                                  stringDataFunction publishImpl );

bool readValueForKey ( const char * buffer, const size_t bufferSize, const char * key, const int maxValueLength,
                       char * value, size_t * charsRead );
void skipWhiteSpaces ( const char * buffer, const size_t bufferSize, size_t * charsRead );

#endif /* COMMONS_H_ */

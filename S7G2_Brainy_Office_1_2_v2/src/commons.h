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

    #include "bsp_pin_cfg.h"
    #include "bsp_api.h"
    #include "r_ioport_api.h"
    #include "hal_data.h"
    #include "tx_api.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
    #define LED_ON              IOPORT_LEVEL_LOW
    #define LED_OFF             IOPORT_LEVEL_HIGH
    #define MAX_THREAD_COUNT    16

    #define logDebug(format, args...)   logApplicationEvent("%s:%d:%s() | [DEBUG]: "format"\r\n",__FILE__,__LINE__,__func__,##args)
    #define logTrace(format, args...)   logApplicationEvent("%s:%d:%s() | [TRACE]: "format"\r\n",__FILE__,__LINE__,__func__,##args)
    #define logInfo(format, args...)    logApplicationEvent("%s:%d:%s() | [INFO]: "format"\r\n",__FILE__,__LINE__,__func__,##args)
    #define logWarn(format, args...)    logApplicationEvent("%s:%d:%s() | [WARN]: "format"\r\n",__FILE__,__LINE__,__func__,##args)
    #define logError(format, args...)   logApplicationEvent("%s:%d:%s() | [ERROR]: "format"\r\n",__FILE__,__LINE__,__func__,##args)

    #define ERROR_HANLDE_TRAP_TO_BREAKPOINT    0x01
    #define ERROR_HANDLE_LOG_TO_FILE           0x02
    #define ERROR_HANLDE_BEHAVIOR              ERROR_LOG_TO_FILE
    #if ( ERROR_HANLDE_BEHAVIOR == ERROR_LOG_TO_FILE )
        #define handleError(errorCode) {\
                                        if ( errorCode != SSP_SUCCESS ) \
                                        { \
                                            g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ RED ], LED_ON ); \
                                            logApplicationEvent("%s:%d:%s() | [ERROR]: %s: %d\r\n",__FILE__,__LINE__,__func__,#errorCode,errorCode); \
                                        } \
                                    }
    #elif ( ERROR_HANLDE_BEHAVIOR == ERROR_HANLDE_TRAP_TO_BREAKPOINT )
        #define handleError(errorCode) {\
                                        if ( errorCode != SSP_SUCCESS ) \
                                        { \
                                            g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ RED ], LED_ON ); \
                                            g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ AMBER ], LED_OFF ); \
                                            g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ GREEN ], LED_OFF ); \
                                            __asm("BKPT"); \
                                        } \
                                    }
    #elif ( ERROR_HANLDE_BEHAVIOR == ( ERROR_HANLDE_TRAP_TO_BREAKPOINT + ERROR_HANDLE_LOG_TO_FILE ) )
        #define handleError(errorCode) {\
                                        if ( errorCode != SSP_SUCCESS ) \
                                        { \
                                            g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ RED ], LED_ON ); \
                                            logApplicationEvent("%s:%d:%s() | [ERROR]: %s: %d\r\n",__FILE__,__LINE__,__func__,#errorCode,errorCode); \
                                            g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ AMBER ], LED_OFF ); \
                                            g_ioport.p_api->pinWrite ( g_bsp_leds.p_leds [ GREEN ], LED_OFF ); \
                                            __asm("BKPT"); \
                                        } \
                                    }
    #endif

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
extern const bsp_leds_t g_bsp_leds;

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
typedef enum
{
    RED = 1,   //
    AMBER = 2, //
    GREEN = 0,
} LED;

//typedef enum
//{
//    NONE = 0x00,  //
//    DEBUG = 0x01, //
//    TRACE = 0x02, //
//    INFO = 0x04,  //
//    WARN = 0x40,  //
//    ERROR = 0x80
//} LogLevel_t;

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
///                        -- None --                        ///

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
/// Generic String Processing function signature.  All errors need to be silenced
/// @param [in,out]  payload         - Data Buffer to perform processing on
/// @param [in,out]  payloadLength   - Maximum number of bytes/chars of the 'payload' buffer
/// @return          non-negative number of bytes/chars modified starting from the 'payload' buffer
typedef unsigned int (*stringDataFunction_t) ( char * payload, size_t payloadLength );

/// Generic cloud house-keeping (e.g. keep-alive ping) function.  All errors need to be silenced.
/// @param  None
/// @return None
typedef void (*cloudHouseKeepFunction_t) ( void );

/// Do NOT call this function twice in the same thread
uint8_t getUniqueThreadId ();

/// Utility function to wrap on the existing pend function.  Just for the sake of ease and avoid copy-paste errors
/// @param [in]      queue               MQ to pend on
/// @param [in,out]  message             Message Payload Buffer Pointer
/// @param [in]      wait_option         Pending wait option -- USE TX_WAIT_OPTIONS
/// @return SSP_SUCESS                   Message Pending was successful
/// @return SSP_ERR_ASSERTION            msg buffer pointer is NULL
/// @return SSP_ERR_NOT_OPEN             Messaging Framework module is not open yet
/// @return SSP_ERR_MESSAGE_QUEUE_EMPTY  Queue is empty (timeout occurred before receiving the message, if timeout is provided)
/// @return SSP_ERR_TIMEOUT              OS Service call return timed out
/// @return SSP_ERR_INTERNAL             OS Service call fails
ssp_err_t messageQueuePend ( TX_QUEUE * queue, void ** message, ULONG wait_option );
ssp_err_t messageQueuePost ( void ** message );
ssp_err_t messageQueueAcquireBuffer ( void ** message );
ssp_err_t messageQueueReleaseBuffer ( void ** message );
ssp_err_t postSystemEventMessage ( uint8_t threadId, sf_message_event_t eventCode );
ssp_err_t postSensorEventMessage ( uint8_t threadId, sf_message_event_t eventCode, void * dataPtr );
ssp_err_t postConfigEventMessage ( uint8_t threadId, sf_message_event_t eventCode,
                                   stringDataFunction_t stringDataFunctionPtr );

/// Generic String Processing function signature.  All errors need to be silenced
/// @param [in]      eventPtr        - Sensor Event Pointer that will be passed back to caller to perform any operations on
/// @param [in,out]  payload         - Data Buffer to perform processing on
/// @param [in,out]  payloadLength   - Maximum number of bytes/chars of the 'payload' buffer
/// @return          non-negative number of bytes/chars modified starting from the 'payload' buffer
typedef unsigned int (*sensorFormatFunction_t) ( const event_sensor_payload_t * const eventPtr, char * payload,
                                                 size_t payloadLength );

/// Register the sensor to set it's own function to publish data to cloud
/// @param [in]  threadId           Unique identifier for the thread, obtained from the call to getUniqueThreadId
/// @param [in]  cloudDataFormatter Implementation Function Pointer to format data for Cloud Publishing
/// @see getUniqueThreadId
void registerSensorForCloudPublish ( uint8_t threadId, sensorFormatFunction_t cloudDataFormatter );

/// Register the sensor to set it's own function to log data on file
/// @param [in]  threadId               Unique identifier for the thread, obtained from the call to getUniqueThreadId
/// @param [in]  fileNameStart          Max of Eight(8)-letter distinct identifier for the sensor log file.  It is the
///                                     prefix of the actual filename.
///                                     This identifier will become part of the log file name, and will help in
///                                     avoiding confusion of getting all the logs from all the sensorts into the
///                                     same file.
/// @param [in]  fileExtension          Max of Three(3)-letter extension of log file.  Good examples are "log" or "csv".
/// @param [in]  fileHeaderFormatter    String function that will be called to add the header line whenever a new file
///                                     for the sensor is generated.  This function can return empty string, if no header
///                                     line log is required.
/// @param [in]  logDataFormatter       Implementation Function Pointer to format data for File Logging
void registerSensorForFileLogging ( uint8_t threadId, const char * fileNameStart, const char * fileExtension,
                                    stringDataFunction_t fileHeaderFormatter, sensorFormatFunction_t logDataFormatter );

bool logApplicationEvent ( const char *format, ... );

bool readValueForKey ( const char * buffer, const size_t bufferSize, const char * key, const int maxValueLength,
                       char * value, size_t * charsRead );
void skipWhiteSpaces ( const char * buffer, const size_t bufferSize, size_t * charsRead );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
///                        -- None --                        ///

/// --  END OF: Static (file scope) Function Declarations -- ///

#endif /* COMMONS_H_ */

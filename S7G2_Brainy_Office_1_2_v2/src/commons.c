/*
 * commons.c
 *
 *  Created on: Oct 12, 2016
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2016 Renesas Electronics America (REA) and Faheem Inayat
 */

#include "commons.h"
#include "r_ioport_api.h"
#include "hal_data.h"
#include "tx_api.h"

#include "event_system_api.h"
#include "event_sensor_api.h"
#include "event_config_api.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
#define KEY_VALUE_DELIMITER   '\n'

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
static sf_message_post_cfg_t post_cfg =
{   .priority = 3, .p_callback = NULL, .p_context = NULL};

static sf_message_post_err_t post_err =
{   .p_queue = NULL};

static sf_message_acquire_cfg_t acquire_cfg =
{   .buffer_keep = false};

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
///                        -- None --                        ///

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
///                        -- None --                        ///

/// --  END OF: Static (file scope) Function Declarations -- ///

ssp_err_t messageQueuePend ( TX_QUEUE * queue, void ** message, ULONG wait_option )
{
    return g_sf_message.p_api->pend ( g_sf_message.p_ctrl, queue, (sf_message_header_t **) message, wait_option );
}

ssp_err_t messageQueuePost ( void ** message )
{
    return g_sf_message.p_api->post ( g_sf_message.p_ctrl, (sf_message_header_t*) ( *message ), &post_cfg, &post_err,
                                      TX_NO_WAIT );
}

ssp_err_t messageQueueAcquireBuffer ( void ** message )
{
    return g_sf_message.p_api->bufferAcquire ( g_sf_message.p_ctrl, (sf_message_header_t **) message, &acquire_cfg,
                                               TX_NO_WAIT );

}

ssp_err_t messageQueueReleaseBuffer ( void ** message )
{
    return g_sf_message.p_api->bufferRelease ( g_sf_message.p_ctrl, (sf_message_header_t *) ( *message ),
                                               SF_MESSAGE_RELEASE_OPTION_NONE );
}

ssp_err_t postSystemEventMessage ( uint8_t threadId, sf_message_event_t eventCode )
{
    event_system_payload_t* message;
    ssp_err_t status = messageQueueAcquireBuffer ( (void **) &message );
    if ( status == SSP_SUCCESS )
    {
        message->header.event_b.class_code = SF_MESSAGE_EVENT_CLASS_SYSTEM;
        message->header.event_b.code = eventCode;
        message->senderId = threadId;

        status = messageQueuePost ( (void **) &message );

        if ( status != SSP_SUCCESS )
        {
            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
    return status;
}

ssp_err_t postSensorEventMessage ( uint8_t threadId, sf_message_event_t eventCode, void * dataPtr )
{
    event_sensor_payload_t* message;
    ssp_err_t status = messageQueueAcquireBuffer ( (void **) &message );
    if ( status == SSP_SUCCESS )
    {
        message->header.event_b.class_code = SF_MESSAGE_EVENT_CLASS_SENSOR;
        message->header.event_b.code = eventCode;
        message->senderId = threadId;
        message->dataPointer = dataPtr;

        status = messageQueuePost ( (void **) &message );

        if ( status != SSP_SUCCESS )
        {
            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
    return status;
}

ssp_err_t postConfigEventMessage ( uint8_t threadId, sf_message_event_t eventCode,
                                   stringDataFunction_t dataFunctionPtr )
{
    SSP_PARAMETER_NOT_USED ( dataFunctionPtr );

    event_config_payload_t* message;
    ssp_err_t status = messageQueueAcquireBuffer ( (void **) &message );
    if ( status == SSP_SUCCESS )
    {
        message->header.event_b.class_code = SF_MESSAGE_EVENT_CLASS_CONFIG;
        message->header.event_b.code = eventCode;
        message->senderId = threadId;
//        message->stringDataFunctionPtr = dataFunctionPtr;

        status = messageQueuePost ( (void **) &message );

        if ( status != SSP_SUCCESS )
        {
            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
    return status;
}

void skipWhiteSpaces ( const char * buffer, const size_t bufferSize, size_t * charsRead )
{
    bool isWhiteSpace = false;

    do
    {
        switch ( buffer [ ( *charsRead ) ] )
        {
            case ' ' :
            case '\t' :
            case '\n' :
            case '\r' :
                isWhiteSpace = true;
                ( *charsRead )++;
                break;

            default :
                isWhiteSpace = false;
                break;
        }
    }
    while ( isWhiteSpace && ( *charsRead ) < bufferSize );
}

bool readValueForKey ( const char * buffer, const size_t bufferSize, const char * key, const int maxValueLength,
                       char * value, size_t * charsRead )
{
    bool retVal = false;
    size_t index = 0;
    const size_t keyLength = strlen ( key );
    bool isComment = false;

    do
    {
        do
        {
            isComment = false;
            skipWhiteSpaces ( buffer, bufferSize, &index );

            if ( buffer [ index ] == '#' )
            {
                // It is a comment, skip till the end of line
                isComment = true;
                char * eol = (char *) memchr ( buffer + index, '\n', ( size_t ) ( bufferSize - index ) );
                if ( eol != NULL )
                {
                    index += ( ( size_t ) ( eol - ( buffer + index ) ) ) + 1;
                }
            }
        }
        while ( isComment );

        if ( strncmp ( buffer + index, key, keyLength ) == 0 )
        {
            // if matches with the key, skip till '='
            char * valueStart = (char *) memchr ( buffer + index + keyLength, '=',
                                                  ( size_t ) ( bufferSize - index + keyLength ) );
            if ( valueStart != NULL )
            {
                valueStart++; // move to next char, skip '='

                char * valueEnd = (char *) memchr ( valueStart, '\r', (size_t) maxValueLength );

                if ( valueEnd != NULL )
                {
                    int lengthDifference = ( valueEnd - valueStart );
                    int valueLength = maxValueLength;
                    if ( lengthDifference < maxValueLength )
                    {
                        valueLength = lengthDifference;
                    }
                    memcpy ( value, valueStart, (size_t) valueLength );
                    value [ valueLength ] = 0;
                    retVal = true;
                    index = ( size_t ) ( valueEnd - buffer );
                }
            }
            break;
        }
        else
        {
            // move to next line
            char * eol = (char *) memchr ( buffer + index, '\n', ( size_t ) ( bufferSize - index ) );
            if ( eol != NULL )
            {
                index += ( ( size_t ) ( eol - ( buffer + index ) ) );
            }
            else
            {
                // there is no next line.  Skip to the end
                index = bufferSize;
            }
        }
    }
    while ( index < bufferSize );

    ( *charsRead ) = index;

    return retVal;
}

//char * translateLogLevel ( LogLevel_t level )
//{
//    switch ( level )
//    {
//        case ERROR :
//            return "ERROR";
//        case WARN :
//            return "WARN";
//        case INFO :
//            return "INFO";
//        case DEBUG :
//            return "DEBUG";
//        case TRACE :
//            return "TRACE";
//        case NONE :
//            return "NONE";
//        default :
//            return "NOT-DEFINED";
//    }
//    return "NONE";
//}


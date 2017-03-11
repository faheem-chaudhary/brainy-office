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

#define KEY_VALUE_DELIMITER   '\n'

static sf_message_post_cfg_t post_cfg =
{   .priority = 3, .p_callback = NULL, .p_context = NULL};

static sf_message_post_err_t post_err =
{   .p_queue = NULL};

static sf_message_acquire_cfg_t acquire_cfg =
{   .buffer_keep = false};

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

ssp_err_t postSystemEventMessage ( TX_THREAD * sender, sf_message_event_class_t eventClass,
                                   sf_message_event_t eventCode )
{
    event_system_payload_t* message;
    ssp_err_t status = messageQueueAcquireBuffer ( (void **) &message );
    if ( status == SSP_SUCCESS )
    {
        message->header.event_b.class_code = eventClass;
        message->header.event_b.code = eventCode;
        message->sender = sender;

        status = messageQueuePost ( (void **) &message );

        if ( status != SSP_SUCCESS )
        {
            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
    return status;
}

ssp_err_t postSensorEventMessage ( TX_THREAD * sender, sf_message_event_class_t eventClass,
                                   sf_message_event_t eventCode, void * dataPtr )
{
    event_sensor_payload_t* message;
    ssp_err_t status = messageQueueAcquireBuffer ( (void **) &message );
    if ( status == SSP_SUCCESS )
    {
        message->header.event_b.class_code = eventClass;
        message->header.event_b.code = eventCode;
        message->sender = sender;
        message->dataPointer = dataPtr;

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
                    int valueLength = min ( ( valueEnd - valueStart ), maxValueLength );
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

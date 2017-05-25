#include "sys_file_logger_thread.h"
#include "tx_api.h"
#include "fx_api.h"
#include <stdarg.h>
#include "event_logging_api.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
#define FILE_LOG_MESSAGE_BUFFER_LENGTH      256
#define LOG_FILE_SIZE_LIMIT                 (128*1024)
#define FILE_NAME_LENGTH                    32
//#define FILE_NAME_EXTENSION                 "csv"
#define ERROR_LOG_SINGLE_MESSAGE_MAX_LENGTH 256
#define ERROR_LOG_MESSAGE_BUFFER_SIZE       8

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
extern FX_MEDIA* gp_media; // global pointer to USB media

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
typedef struct LogFileDescriptor
{
        char fileNamePrefix [ 7 ];
        char fileNameExtension [ 4 ];
        FX_FILE file;
        char fileName [ FILE_NAME_LENGTH ];
        char fileExtension [ 4 ];
        uint8_t fileNameCounter;
        bool isFileOpen;
} LogFileDescriptor_t;

typedef struct FileLogPublisherData
{
        stringDataFunction_t headerFormatter;
        sensorFormatFunction_t dataFormatter;
        LogFileDescriptor_t fileDescriptor;
} FileLogPublisherData_t;

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
static ULONG sys_file_logger_thread_wait = 10;
static uint8_t file_logger_thread_id;
static bool isUsbAvailable = false;
static FileLogPublisherData_t registeredFileLogPublishers [ MAX_THREAD_COUNT ];
static char fileLogMessageBuffer [ FILE_LOG_MESSAGE_BUFFER_LENGTH + 1 ];
static LogFileDescriptor_t logFileDescriptor;
static uint8_t errorLogMessageBufferHead = 0;
static uint8_t errorLogMessageBufferTail = 0;
static char errorLogMessageBuffer [ ERROR_LOG_MESSAGE_BUFFER_SIZE ] [ ERROR_LOG_SINGLE_MESSAGE_MAX_LENGTH ];
static TX_MUTEX errorLogEventMutex;

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
void sys_file_logger_thread_entry ( void );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
static UINT openLogFile ( LogFileDescriptor_t * fileDescriptor, stringDataFunction_t headerFormatter );

static void sys_file_logger_thread_processConfigMessage ( event_config_payload_t *configEventMsg );
static void sys_file_logger_thread_processSensorMessage ( event_sensor_payload_t *sensorEventMsg );
static void sys_file_logger_thread_processSystemMessage ( event_system_payload_t *systemEventMsg );
static void sys_file_logger_thread_processLogMessage ( event_logging_payload_t *loggingEventMsg );

/// --  END OF: Static (file scope) Function Declarations -- ///

// System File Logger Thread entry function
void sys_file_logger_thread_entry ( void )
{
    sf_message_header_t * message;
    ssp_err_t msgStatus;

    file_logger_thread_id = getUniqueThreadId ();
    tx_mutex_create ( &errorLogEventMutex, (CHAR *) "Error Log Event Mutex", TX_NO_INHERIT );

    strcpy ( logFileDescriptor.fileNamePrefix, "system" );
    strcpy ( logFileDescriptor.fileNameExtension, "log" );

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sys_file_logger_thread_message_queue, (void **) &message,
                                       sys_file_logger_thread_wait );

        if ( msgStatus == SSP_SUCCESS )
        {
            // TODO: Process System Message here
            switch ( message->event_b.class_code )
            {
                case SF_MESSAGE_EVENT_CLASS_CONFIG :
                    sys_file_logger_thread_processConfigMessage ( (event_config_payload_t *) message );
                    break;

                case SF_MESSAGE_EVENT_CLASS_SENSOR :
                    sys_file_logger_thread_processSensorMessage ( (event_sensor_payload_t *) message );
                    break;

                case SF_MESSAGE_EVENT_CLASS_SYSTEM :
                    sys_file_logger_thread_processSystemMessage ( (event_system_payload_t *) message );
                    break;

                case SF_MESSAGE_EVENT_CLASS_LOGGING :
                    sys_file_logger_thread_processLogMessage ( (event_logging_payload_t *) message );
                    break;
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }
    }
}

void sys_file_logger_thread_processConfigMessage ( event_config_payload_t *configEventMsg )
{
    switch ( configEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_CONFIG_SET_LOG_FORMATTER :
            break;
    }
}

void registerSensorForFileLogging ( uint8_t threadId, const char * fileNameStart, const char * fileExtension,
                                    stringDataFunction_t fileHeaderFormatter, sensorFormatFunction_t logDataFormatter )
{
    if ( threadId < MAX_THREAD_COUNT )
    {
        strncpy ( registeredFileLogPublishers [ threadId ].fileDescriptor.fileNamePrefix, fileNameStart, 6 );
        strncpy ( registeredFileLogPublishers [ threadId ].fileDescriptor.fileNameExtension, fileExtension, 3 );
        registeredFileLogPublishers [ threadId ].fileDescriptor.fileNamePrefix [ 6 ] = '\0';
        registeredFileLogPublishers [ threadId ].headerFormatter = fileHeaderFormatter;
        registeredFileLogPublishers [ threadId ].dataFormatter = logDataFormatter;
    }
}

void sys_file_logger_thread_processSensorMessage ( event_sensor_payload_t *sensorEventMsg )
{
    UINT apiStatus;
    unsigned int dataLength;
    LogFileDescriptor_t * fileDescriptor = NULL;
    FileLogPublisherData_t * publisher = NULL;

    switch ( sensorEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_SENSOR_NEW_DATA :
            if ( isUsbAvailable )
            {
                if ( sensorEventMsg->senderId < MAX_THREAD_COUNT )
                {
                    publisher = & ( registeredFileLogPublishers [ sensorEventMsg->senderId ] );

                    if ( publisher->dataFormatter != NULL )
                    {
                        fileDescriptor = & ( publisher->fileDescriptor );
                        apiStatus = openLogFile ( fileDescriptor, publisher->headerFormatter );

                        if ( apiStatus == 0 )
                        {
                            // Get the data Payload
                            dataLength = publisher->dataFormatter ( sensorEventMsg, fileLogMessageBuffer,
                            FILE_LOG_MESSAGE_BUFFER_LENGTH );

                            if ( dataLength > 0 )
                            {
                                fx_file_write ( & ( fileDescriptor->file ), fileLogMessageBuffer, dataLength );
                                fx_media_flush ( gp_media );
                                fx_media_cache_invalidate ( gp_media );
                            }
                        }
                    }
                }
            }
            break;
    }
}

void sys_file_logger_thread_processSystemMessage ( event_system_payload_t *systemEventMsg )
{
    switch ( systemEventMsg->header.event_b.code )
    {
        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_READY :
            postSystemEventMessage ( file_logger_thread_id, SF_MESSAGE_EVENT_SYSTEM_FILE_LOGGING_AVAILABLE );
            isUsbAvailable = true;
            break;

        case SF_MESSAGE_EVENT_SYSTEM_USB_STORAGE_REMOVED :
            postSystemEventMessage ( file_logger_thread_id, SF_MESSAGE_EVENT_SYSTEM_FILE_LOGGING_UNAVAILABLE );
            isUsbAvailable = false;
            break;
    }
}

UINT openLogFile ( LogFileDescriptor_t * fileDescriptor, stringDataFunction_t headerFormatter )
{
    bool createFile = true;
    UINT fileStatus = FX_SUCCESS;

    if ( fileDescriptor->isFileOpen )
    {
        createFile = false;

        if ( ( fileDescriptor->file.fx_file_current_file_size ) > LOG_FILE_SIZE_LIMIT )
        {
            fx_file_close ( & ( fileDescriptor->file ) );
            fx_media_flush ( gp_media );
            fx_media_cache_invalidate ( gp_media );

            createFile = true;
        }
    }

    if ( createFile )
    {
        fileDescriptor->isFileOpen = false;
        {
            do
            {
                sprintf ( fileDescriptor->fileName, "%s_%04X.%s", fileDescriptor->fileNamePrefix,
                          fileDescriptor->fileNameCounter, fileDescriptor->fileNameExtension );
                fileDescriptor->fileNameCounter++;
                fileStatus = fx_file_create ( gp_media, fileDescriptor->fileName );
            }
            while ( fileStatus == FX_ALREADY_CREATED );
        }

        if ( ( fileStatus == FX_SUCCESS ) )
        {
            fileStatus = fx_file_open ( gp_media, & ( fileDescriptor->file ), fileDescriptor->fileName,
                                        FX_OPEN_FOR_WRITE );

            if ( fileStatus == FX_SUCCESS )
            {
                fileDescriptor->isFileOpen = true;

                if ( headerFormatter != NULL )
                {
                    char fileHeader [ FILE_LOG_MESSAGE_BUFFER_LENGTH + 1 ];
                    size_t headerLength = headerFormatter ( fileHeader, FILE_LOG_MESSAGE_BUFFER_LENGTH );
                    if ( headerLength > 0 )
                    {
                        fx_file_write ( & ( fileDescriptor->file ), fileHeader, headerLength );
                    }
                }
            }
        }
    }

    return fileStatus;
}

bool logApplicationEvent ( const char *format, ... )
{
    bool retVal = false;
    event_logging_payload_t* message;

    if ( isUsbAvailable == true && ( gp_media != NULL ) )
    {

        tx_mutex_get ( &errorLogEventMutex, TX_WAIT_FOREVER );

        uint8_t nextIndex = ( uint8_t ) ( errorLogMessageBufferHead + 1 );

        if ( nextIndex >= ERROR_LOG_MESSAGE_BUFFER_SIZE ) // roll over
        {
            nextIndex = 0;
        }

        if ( nextIndex != errorLogMessageBufferTail )
        {
            va_list args;
            va_start ( args, format );
            vsprintf ( errorLogMessageBuffer [ errorLogMessageBufferHead ], format, args );
            va_end ( args );
            errorLogMessageBufferHead = nextIndex;
            retVal = true;
        }

        tx_mutex_put ( &errorLogEventMutex );

        if ( retVal == true ) // if everything is good, publish a logging event
        {
            ssp_err_t status = messageQueueAcquireBuffer ( (void **) &message );
            if ( status == SSP_SUCCESS )
            {
                message->header.event_b.class_code = SF_MESSAGE_EVENT_CLASS_LOGGING;
                message->header.event_b.code = SF_MESSAGE_EVENT_LOGGING_NEW_DATA;

                status = messageQueuePost ( (void **) &message );

                if ( status != SSP_SUCCESS )
                {
                    messageQueueReleaseBuffer ( (void **) &message );
                }
            }
        }
    }

    return retVal;
}

void sys_file_logger_thread_processLogMessage ( event_logging_payload_t *loggingEventMsg )
{
    if ( loggingEventMsg->header.event_b.code == SF_MESSAGE_EVENT_LOGGING_NEW_DATA )
    {
        if ( errorLogMessageBufferHead != errorLogMessageBufferTail )
        {
            UINT apiStatus = openLogFile ( &logFileDescriptor, NULL );

            if ( apiStatus == 0 )
            {
                tx_mutex_get ( &errorLogEventMutex, TX_WAIT_FOREVER );

                do
                {
                    uint8_t nextIndex = ( uint8_t ) ( errorLogMessageBufferTail + 1 );

                    if ( nextIndex >= ERROR_LOG_MESSAGE_BUFFER_SIZE ) // roll over
                    {
                        nextIndex = 0;
                    }

                    fx_file_write ( & ( logFileDescriptor.file ), errorLogMessageBuffer [ errorLogMessageBufferTail ],
                                    strlen ( errorLogMessageBuffer [ errorLogMessageBufferTail ] ) );
                    errorLogMessageBufferTail = nextIndex;

                }
                while ( errorLogMessageBufferHead != errorLogMessageBufferTail ); // empty the queue, whenever get into this event

                tx_mutex_put ( &errorLogEventMutex );

                fx_media_flush ( gp_media );
                fx_media_cache_invalidate ( gp_media );
            }
        }
    }
}

// compiler switch: -finstrument-functions
// __attribute__((no_instrument_function))
// __attribute__ ((destructor))
// __attribute__ ((constructor))

//__attribute__((no_instrument_function))
//void __cyg_profile_func_enter ( void *this_fn, void *call_site )
//{
//    if ( isUsbAvailable == true && ( gp_media != NULL ) )
//    {
//    }
//}
//
//__attribute__((no_instrument_function))
//void __cyg_profile_func_exit ( void *this_fn, void *call_site )
//{
//    if ( isUsbAvailable == true && ( gp_media != NULL ) )
//    {
//    }
//}
//

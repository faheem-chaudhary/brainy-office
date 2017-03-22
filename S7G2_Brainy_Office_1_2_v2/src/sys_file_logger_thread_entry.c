#include "sys_file_logger_thread.h"
/*
 #if (USBX_CONFIGURED)
 #include "fx_api.h"

 UINT openLogFile ( bool alreadyOpen, FX_FILE* file, ULONG64* fileSize, char * fileName, const char * suffix,
 const char * extension, unsigned int* fileNameCounter, const char * fileHeader );
 #endif
 */
// System File Logger Thread entry function
void sys_file_logger_thread_entry ( void )
{
    // TODO: add your own code here
    while ( 1 )
    {
        tx_thread_sleep ( 1 );
    }
}
/*
 #if (USBX_CONFIGURED)
 #define LOG_FILE_SIZE_LIMIT (128*1024)

 UINT openLogFile ( bool alreadyOpen, FX_FILE* file, ULONG64* fileSize, char * fileName, const char * suffix,
 const char * extension, unsigned int* fileNameCounter, const char * fileHeader )
 {
 extern FX_MEDIA* gp_media; // global pointer to USB media

 bool createFile = true;
 UINT fileStatus = FX_SUCCESS;

 if ( alreadyOpen )
 {
 createFile = false;

 ( *fileSize ) = file->fx_file_current_file_size;

 if ( *fileSize > LOG_FILE_SIZE_LIMIT )
 {
 fx_file_close ( file );
 fx_media_flush ( gp_media );
 fx_media_cache_invalidate ( gp_media );

 createFile = true;
 }
 }

 if ( createFile )
 {
 {
 extern char * g_deviceName;

 do
 {
 sprintf ( fileName, "%s_%s_%04X.%s", g_deviceName, suffix, ( *fileNameCounter ), extension );
 *fileNameCounter = ( ( *fileNameCounter ) + 1 );
 fileStatus = fx_file_create ( gp_media, fileName );
 }
 while ( fileStatus == FX_ALREADY_CREATED );
 }

 if ( ( fileStatus == FX_SUCCESS ) )
 {
 fileStatus = fx_file_open ( gp_media, file, fileName, FX_OPEN_FOR_WRITE );
 if ( fileStatus == FX_SUCCESS )
 {
 size_t headerLength = strlen ( fileHeader );
 if ( headerLength > 0 )
 {
 fx_file_write ( file, (char *) fileHeader, headerLength );
 }
 *fileSize = file->fx_file_current_file_size;
 }
 }
 }

 return fileStatus;
 }
 #endif
 */

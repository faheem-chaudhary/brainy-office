/*
 * cloud_medium1_adapter.c
 *
 *  Created on: Feb 21, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#include "stddef.h"

#include "cloud_medium1_adapter.h"
#include "commons.h"

#include "libemqtt.h"
#include "nx_dns.h"
#include "libemqtt_netx_impl.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
#define M1_CONFIG_VALUE_LENGTH  33
#define M1_API_KEY_VALUE_LENGTH 65

#undef MQTT_CONF_USERNAME_LENGTH
#undef MQTT_CONF_PASSWORD_LENGTH

#define MQTT_CONF_USERNAME_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_CONFIG_VALUE_LENGTH + 2)
#define MQTT_CONF_PASSWORD_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_API_KEY_VALUE_LENGTH + 2)

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
extern NX_DNS g_dns_client;

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
typedef struct
{
        char name [ M1_CONFIG_VALUE_LENGTH ];
        char apiKey [ M1_API_KEY_VALUE_LENGTH ];
        char projectId [ M1_CONFIG_VALUE_LENGTH ];
        char userId [ M1_CONFIG_VALUE_LENGTH ];
        char password [ M1_CONFIG_VALUE_LENGTH ];
        char hostName [ M1_API_KEY_VALUE_LENGTH ];
        unsigned int port;

} MediumOneDeviceCredentials_t;

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
static MediumOneDeviceCredentials_t mediumOneDeviceCredentials;
static MqttConnection_t mqttConnection;
//static int connectFailureRetries = 0;
static char topic_name [ 255 ];
static char publishMessageBuffer [ 512 ];

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
///                        -- None --                        ///

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
static bool mqttReconnect ();

/// --  END OF: Static (file scope) Function Declarations -- ///

unsigned int mediumOneConfigImpl ( char * configData, size_t dataLength )
{
    MediumOneDeviceCredentials_t m1Creds;
    size_t charsRead = 0;
    unsigned int keysRead = 0;
    char portStr [ 6 ];

    // bytesRead will contain the number of bytes read, from this call
    if ( readValueForKey ( configData, dataLength, "name", M1_CONFIG_VALUE_LENGTH, m1Creds.name, &charsRead ) )
    {
        keysRead++;
    }

    if ( readValueForKey ( configData, dataLength, "api_key", M1_API_KEY_VALUE_LENGTH, m1Creds.apiKey, &charsRead ) )
    {
        keysRead++;
    }

    if ( readValueForKey ( configData, dataLength, "project_id", M1_CONFIG_VALUE_LENGTH, m1Creds.projectId,
                           &charsRead ) )
    {
        keysRead++;
    }

    if ( readValueForKey ( configData, dataLength, "user_id", M1_CONFIG_VALUE_LENGTH, m1Creds.userId, &charsRead ) )
    {
        keysRead++;
    }

    if ( readValueForKey ( configData, dataLength, "password", M1_CONFIG_VALUE_LENGTH, m1Creds.password, &charsRead ) )
    {
        keysRead++;
    }

    if ( readValueForKey ( configData, dataLength, "host", M1_API_KEY_VALUE_LENGTH, m1Creds.hostName, &charsRead ) )
    {
        keysRead++;
    }

    if ( readValueForKey ( configData, dataLength, "port", M1_CONFIG_VALUE_LENGTH, portStr, &charsRead ) )
    {
        m1Creds.port = (unsigned int) atoi ( portStr );
        keysRead++;
    }

    if ( keysRead == 7 )
    {
        memcpy ( mediumOneDeviceCredentials.name, m1Creds.name, M1_CONFIG_VALUE_LENGTH );
        memcpy ( mediumOneDeviceCredentials.apiKey, m1Creds.apiKey, M1_API_KEY_VALUE_LENGTH );
        memcpy ( mediumOneDeviceCredentials.projectId, m1Creds.projectId, M1_CONFIG_VALUE_LENGTH );
        memcpy ( mediumOneDeviceCredentials.userId, m1Creds.userId, M1_CONFIG_VALUE_LENGTH );
        memcpy ( mediumOneDeviceCredentials.password, m1Creds.password, M1_CONFIG_VALUE_LENGTH );
        memcpy ( mediumOneDeviceCredentials.hostName, m1Creds.hostName, M1_API_KEY_VALUE_LENGTH );
        mediumOneDeviceCredentials.port = m1Creds.port;

        return keysRead;
    }

    return 0;
}

unsigned int mediumOneInitImpl ( char * configData, size_t dataLength )
{
    SSP_PARAMETER_NOT_USED ( configData );
    SSP_PARAMETER_NOT_USED ( dataLength );

    int status = 0;

    sprintf ( mqttConnection.userName, "%s/%s", mediumOneDeviceCredentials.projectId,
              mediumOneDeviceCredentials.userId );
    sprintf ( mqttConnection.password, "%s/%s", mediumOneDeviceCredentials.apiKey,
              mediumOneDeviceCredentials.password );

    { // resolve the hostName to IP Address
        nx_dns_host_by_name_get ( &g_dns_client, (UCHAR *) mediumOneDeviceCredentials.hostName,
                                  &mqttConnection.hostIpAddress, TX_WAIT_FOREVER );

        mqttConnection.port = (unsigned short) mediumOneDeviceCredentials.port;
        mqttConnection.isKeepAlive = true;
        mqttConnection.keepAliveDelay = 60;
        mqttConnection.isRetryOnDisconnect = true;
        mqttConnection.retrylimit = 5;
        mqttConnection.retryDelay = 5;
#if (LIBEMQTT_NETX_SOCKETS_IMPL==LIBEMQTT_NETX_IMPL_DEFAULT)
        mqttConnection.ipStackPtr = &g_ip;
#endif
    }

    mqtt_netx_disconnect (); // Just in case if there is any socket residue, clean it up first.
    status = mqtt_netx_connect ( mediumOneDeviceCredentials.name, &mqttConnection );

    if ( status == 1 )
    {
        //TODO: Setup Subscription function here, at some point in time

        ///    0/<project_mqtt_id>/<user_mqtt_id>/<device_id>
        sprintf ( topic_name, "0/%s/%s/%s", mediumOneDeviceCredentials.projectId, mediumOneDeviceCredentials.userId,
                  mediumOneDeviceCredentials.name );

        mediumOnePublishImpl ( "{\"connected\":true}", 0 );
//        connectFailureRetries = 0;
        tx_thread_sleep ( 50 ); // wait for half a second to let the message processed by the broker
    }

    return ( status == 1 );
}

unsigned int mediumOnePublishImpl ( char * message, size_t messageLength )
{
    SSP_PARAMETER_NOT_USED ( messageLength );

    int status = sprintf ( publishMessageBuffer, "{\"event_data\":{\"%s\":%s}}", mediumOneDeviceCredentials.name,
                           message );
    if ( status > 0 )
    {
        status = mqtt_netx_publish ( topic_name, publishMessageBuffer, 0 );

        if ( status <= 0 ) // Connection Issues, try to connect again
        {
            if ( mqttReconnect () == true )
            {
                status = mqtt_netx_publish ( topic_name, publishMessageBuffer, 0 );
            }
        }
    }

    return ( status > 0 );
}

void mediumOneHouseKeepImpl ( void )
{
    if ( mqttConnection.isKeepAlive )
    {
        int status = mqtt_netx_ping ();

        if ( status <= 0 ) // Connection Issues, try to connect again
        {
            mqttReconnect ();
        }
    }
}

bool mqttReconnect ()
{
    int status;

    mqtt_netx_disconnect ();

    ULONG ipAddress;

    // test if NetX stack is working.  HostName should resolve in order to make it to application protocol.  Otherwise, make it a system trap
    UINT dnsResolutionStatus = nx_dns_host_by_name_get ( &g_dns_client, (UCHAR *) mediumOneDeviceCredentials.hostName,
                                                         &ipAddress, ( 1 * 30 * 100 ) ); // 30 seconds wait, at max
    handleError ( dnsResolutionStatus );
//    logApplicationEvent ( "nx_dns_host_by_name_get returned %2X with DNS address resolved to: %X\r\n", dnsResolutionStatus, ipAddress );

    status = mqtt_netx_connect ( mediumOneDeviceCredentials.name, &mqttConnection );

    if ( status == 1 )
    {
        char reconnectMessage [ 256 ];
        sprintf ( reconnectMessage, "{\"event_data\":{\"%s\":{\"reconnected\":true}}}",
                  mediumOneDeviceCredentials.name );
        status = mqtt_netx_publish ( topic_name, reconnectMessage, 0 );

        if ( status > 0 )
        {
            tx_thread_sleep ( 50 ); // wait for half a second to let the message processed by the broker
        }
    }
    else
    {
//        connectFailureRetries++;

        // Make a hard reboot, and let system take care of it
//        if ( connectFailureRetries > 5 )
//        {
        NVIC_SystemReset ();
//        }
    }

    return ( status > 0 );
}

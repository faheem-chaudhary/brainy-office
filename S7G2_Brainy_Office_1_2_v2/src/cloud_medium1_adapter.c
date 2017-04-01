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

#define M1_CONFIG_VALUE_LENGTH  33
#define M1_API_KEY_VALUE_LENGTH 65

#undef MQTT_CONF_USERNAME_LENGTH
#undef MQTT_CONF_PASSWORD_LENGTH

#define MQTT_CONF_USERNAME_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_CONFIG_VALUE_LENGTH + 2)
#define MQTT_CONF_PASSWORD_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_API_KEY_VALUE_LENGTH + 2)

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

MediumOneDeviceCredentials_t g_mediumOneDeviceCredentials;
MqttConnection_t g_mqttConnection;

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
        memcpy ( g_mediumOneDeviceCredentials.name, m1Creds.name, M1_CONFIG_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.apiKey, m1Creds.apiKey, M1_API_KEY_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.projectId, m1Creds.projectId, M1_CONFIG_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.userId, m1Creds.userId, M1_CONFIG_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.password, m1Creds.password, M1_CONFIG_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.hostName, m1Creds.hostName, M1_API_KEY_VALUE_LENGTH );
        g_mediumOneDeviceCredentials.port = m1Creds.port;

        return keysRead;
    }

    return 0;
}

char g_topic_name [ 255 ];
extern NX_DNS g_dns_client;

unsigned int mediumOneInitImpl ( char * configData, size_t dataLength )
{
    SSP_PARAMETER_NOT_USED ( configData );
    SSP_PARAMETER_NOT_USED ( dataLength );

    int status = 0;

    sprintf ( g_mqttConnection.userName, "%s/%s", g_mediumOneDeviceCredentials.projectId,
              g_mediumOneDeviceCredentials.userId );
    sprintf ( g_mqttConnection.password, "%s/%s", g_mediumOneDeviceCredentials.apiKey,
              g_mediumOneDeviceCredentials.password );

    { // resolve the hostName to IP Address
        nx_dns_host_by_name_get ( &g_dns_client, (UCHAR *) g_mediumOneDeviceCredentials.hostName,
                                  &g_mqttConnection.hostIpAddress, TX_WAIT_FOREVER );

        g_mqttConnection.port = (unsigned short) g_mediumOneDeviceCredentials.port;
        g_mqttConnection.isKeepAlive = true;
        g_mqttConnection.keepAliveDelay = 60;
        g_mqttConnection.isRetryOnDisconnect = true;
        g_mqttConnection.retrylimit = 5;
        g_mqttConnection.retryDelay = 5;
    }

    status = mqtt_netx_connect ( g_mediumOneDeviceCredentials.name, &g_mqttConnection );

    if ( status == 1 )
    {
        //TODO: Setup Subscription function here, at some point in time

        ///    0/<project_mqtt_id>/<user_mqtt_id>/<device_id>/
        sprintf ( g_topic_name, "0/%s/%s/%s", g_mediumOneDeviceCredentials.projectId,
                  g_mediumOneDeviceCredentials.userId, g_mediumOneDeviceCredentials.name );

        mediumOnePublishImpl ( "{\"connected\":true}", 0 );
    }

    return ( status == 1 );
}

char g_publishMessageBuffer [ 512 ];

unsigned int mediumOnePublishImpl ( char * message, size_t messageLength )
{
    int status = sprintf ( g_publishMessageBuffer, "{\"event_data\":{\"%s\":%s}}", g_mediumOneDeviceCredentials.name,
                           message );
    if ( status > 0 )
    {
        status = mqtt_netx_publish ( g_topic_name, g_publishMessageBuffer, 0 );

        if ( status == 0 ) // Connection Issues, try to connect again
        {
            status = mqtt_netx_connect ( g_mediumOneDeviceCredentials.name, &g_mqttConnection );

            if ( status == 1 )
            {
                mediumOnePublishImpl ( "{\"reconnected\":true}", 0 );
                status = mqtt_netx_publish ( g_topic_name, g_publishMessageBuffer, 0 );
            }
        }
    }

    return ( status > 0 );
}

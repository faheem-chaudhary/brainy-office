/*
 * cloud_medium1_adapter.c
 *
 *  Created on: Feb 21, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#include "cloud_medium1_adapter.h"
#include "commons.h"

#define DUMMY_CLOUD 0

#undef MQTT_CONF_USERNAME_LENGTH
#undef MQTT_CONF_PASSWORD_LENGTH

#define MQTT_CONF_USERNAME_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_CONFIG_VALUE_LENGTH + 2)
#define MQTT_CONF_PASSWORD_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_API_KEY_VALUE_LENGTH + 2)

#include "libemqtt.h"
#include "nx_dns.h"
#include "libemqtt_netx_impl.h"

MediumOneDeviceCredentials_t g_mediumOneDeviceCredentials;

#if(DUMMY_CLOUD)
unsigned int mediumOneConfigImpl ( char * configData, size_t dataLength )
{
    return (unsigned int) 7;
}

unsigned int mediumOneInitImpl ( char * configData, size_t dataLength )
{
    return (unsigned int) 1;
}

char g_publishMessageBuffer [ 512 ];
unsigned int mediumOnePublishImpl ( char * message, size_t messageLength )
{
//    0/<project_mqtt_id>/<user_mqtt_id>/<device_id>/

    int length = snprintf ( g_publishMessageBuffer, messageLength, "{\"event_data\":{\"%s\":%s}}",
            g_mediumOneDeviceCredentials.name, message );
//    mqtt_netx_publish ( g_topic_name, g_publishMessageBuffer, 0 );
    return ( length > 0 );
}

#else
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

//static mqtt_broker_handle_t g_mqttBroker;
char g_topic_name [ 255 ];
extern NX_DNS g_dns_client;

unsigned int mediumOneInitImpl ( char * configData, size_t dataLength )
{
    SSP_PARAMETER_NOT_USED ( configData );
    SSP_PARAMETER_NOT_USED ( dataLength );

    int status = 0;

    char m1UsernameStr [ MQTT_CONF_USERNAME_LENGTH ];
    char m1Password [ MQTT_CONF_PASSWORD_LENGTH ];
    char hostIpAddress [ 16 ];

    sprintf ( m1UsernameStr, "%s/%s", g_mediumOneDeviceCredentials.projectId, g_mediumOneDeviceCredentials.userId );
    sprintf ( m1Password, "%s/%s", g_mediumOneDeviceCredentials.apiKey, g_mediumOneDeviceCredentials.password );

    { // resolve the hostName to IP Address
        ULONG ipAddressLong;
        nx_dns_host_by_name_get ( &g_dns_client, (UCHAR *) g_mediumOneDeviceCredentials.hostName, &ipAddressLong,
                                  TX_WAIT_FOREVER );

        sprintf ( hostIpAddress, "%d.%d.%d.%d", (int) ( ipAddressLong >> 24 ), (int) ( ipAddressLong >> 16 ) & 0xFF,
                  (int) ( ipAddressLong >> 8 ) & 0xFF, (int) ( ipAddressLong ) & 0xFF );
    }

    //"name=desktop-kit-1\r\napi_key=WG46JL5TPKJ3Q272SDCEJDJQGQ4DEOJZGM2GEZBYGRQTAMBQ\r\nproject_id=o3adQcvIn0Q\r\nuser_id=kHgc2feq1bg\r\npassword=Dec2kPok\r\nhost=mqtt2.mediumone.com\r\nport=61619" );
    status = mqtt_netx_connect ( g_mediumOneDeviceCredentials.name, hostIpAddress, g_mediumOneDeviceCredentials.port,
                                 m1UsernameStr, m1Password, 5, 5, 60, 1 );

    if ( status == 1 )
    {
        //TODO: Setup Subscription function here, at some point in time

        sprintf ( g_topic_name, "0/%s/%s/%s", g_mediumOneDeviceCredentials.projectId,
                  g_mediumOneDeviceCredentials.userId, g_mediumOneDeviceCredentials.name );

        mediumOnePublishImpl ( "{\"connected\":true}", 0 );
    }

    return ( status == 1 );
}

char g_publishMessageBuffer [ 512 ];

unsigned int mediumOnePublishImpl ( char * message, size_t messageLength )
{
//    0/<project_mqtt_id>/<user_mqtt_id>/<device_id>/

    int length = sprintf ( g_publishMessageBuffer, "{\"event_data\":{\"%s\":%s}}", g_mediumOneDeviceCredentials.name,
                           message );
    mqtt_netx_publish ( g_topic_name, g_publishMessageBuffer, 0 );
    return ( length > 0 );
}

#endif

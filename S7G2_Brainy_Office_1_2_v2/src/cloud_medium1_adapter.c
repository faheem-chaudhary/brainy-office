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

#define USE_M1_AGENT    0

#undef MQTT_CONF_USERNAME_LENGTH
#undef MQTT_CONF_PASSWORD_LENGTH

#define MQTT_CONF_USERNAME_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_CONFIG_VALUE_LENGTH + 2)
#define MQTT_CONF_PASSWORD_LENGTH   (M1_CONFIG_VALUE_LENGTH + M1_API_KEY_VALUE_LENGTH + 2)

#include "libemqtt.h"
#include "nx_dns.h"

MediumOneDeviceCredentials_t g_mediumOneDeviceCredentials;

void mqtt_message_callback ( int type, char * topic, char * payload, int length );

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

#if (USE_M1_AGENT)
    #include "m1_agent.h"
    #define M1_TLS_ENABLED 0

unsigned int mediumOneInitImpl ( char * configData, size_t dataLength )
{
    SSP_PARAMETER_NOT_USED ( configData );
    SSP_PARAMETER_NOT_USED ( dataLength );

    int status;

    m1_register_subscription_callback ( mqtt_message_callback );

    #if (M1_TLS_ENABLED)
    status = m1_connect ( "mqtt2.mediumone.com", 61620, g_mediumOneDeviceCredentials.userId,
            g_mediumOneDeviceCredentials.password, g_mediumOneDeviceCredentials.projectId,
            g_mediumOneDeviceCredentials.apiKey, g_mediumOneDeviceCredentials.name, 5, 5, 60, 1 );
    #else
    status = m1_connect ( "mqtt2.mediumone.com", 61619, g_mediumOneDeviceCredentials.userId,
            g_mediumOneDeviceCredentials.password, g_mediumOneDeviceCredentials.projectId,
            g_mediumOneDeviceCredentials.apiKey, g_mediumOneDeviceCredentials.name, 5, 5, 60, 0 );
    #endif

    char initialConnectMessage [ 256 ];
    sprintf ( initialConnectMessage, "{\"init_connect\":{\"name\":\"%s\"}}", g_mediumOneDeviceCredentials.name );

    m1_publish_event ( initialConnectMessage, NULL );

    //TODO: Is this needed?
    // extern const sf_comms_instance_t g_sf_comms0;
    // m1_initialize_comms(&g_sf_comms0);

    return ( status == M1_SUCCESS );
}

unsigned int mediumOneCloudImpl ( char * payload, size_t maxLength )
{
    return 0;
}

void mqtt_message_callback ( int type, char * topic, char * payload, int length )
{
    SSP_PARAMETER_NOT_USED ( type );
    SSP_PARAMETER_NOT_USED ( topic );
    SSP_PARAMETER_NOT_USED ( payload );
    SSP_PARAMETER_NOT_USED ( length );
}

#else
    #include "libemqtt_netx_impl.h"

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

//    m1_register_subscription_callback ( mqtt_message_callback );

        sprintf ( g_topic_name, "0/%s/%s/%s", g_mediumOneDeviceCredentials.projectId,
                  g_mediumOneDeviceCredentials.userId, g_mediumOneDeviceCredentials.name );

        char initialConnectMessage [ 256 ];
        sprintf ( initialConnectMessage, "{\"event_data\":{\"init_connect\":{\"name\":\"%s\"}}}",
                  g_mediumOneDeviceCredentials.name );
        mqtt_netx_publish ( g_topic_name, initialConnectMessage, 0 );

    }

    return ( status == 1 );
}

unsigned int mediumOnePublishImpl ( char * payload, size_t maxLength )
{
//    0/<project_mqtt_id>/<user_mqtt_id>/<device_id>/

    mqtt_netx_publish ( g_topic_name, payload, 0 );
    return 0;
}

void mqtt_message_callback ( int type, char * topic, char * payload, int length )
{
    SSP_PARAMETER_NOT_USED ( type );
    SSP_PARAMETER_NOT_USED ( topic );
    SSP_PARAMETER_NOT_USED ( payload );
    SSP_PARAMETER_NOT_USED ( length );
}

#endif

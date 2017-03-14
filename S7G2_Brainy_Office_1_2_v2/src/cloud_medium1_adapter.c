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

MediumOneDeviceCredentials_t g_mediumOneDeviceCredentials;

void mqtt_message_callback ( int type, char * topic, char * payload, int length );

unsigned int mediumOneConfigImpl ( char * configData, size_t dataLength )
{
    MediumOneDeviceCredentials_t m1Creds;
    size_t charsRead = 0;
    unsigned int keysRead = 0;

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

    if ( keysRead == 5 )
    {
        memcpy ( g_mediumOneDeviceCredentials.name, m1Creds.name, M1_CONFIG_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.apiKey, m1Creds.apiKey, M1_API_KEY_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.projectId, m1Creds.projectId, M1_CONFIG_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.userId, m1Creds.userId, M1_CONFIG_VALUE_LENGTH );
        memcpy ( g_mediumOneDeviceCredentials.password, m1Creds.password, M1_CONFIG_VALUE_LENGTH );

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

//static mqtt_broker_handle_t g_mqttBroker;
static char g_topic_name [ 255 ];

unsigned int mediumOneInitImpl ( char * configData, size_t dataLength )
{
    SSP_PARAMETER_NOT_USED ( configData );
    SSP_PARAMETER_NOT_USED ( dataLength );

    int status = 0;

    char m1UsernameStr [ MQTT_CONF_USERNAME_LENGTH ];
    char m1Password [ MQTT_CONF_PASSWORD_LENGTH ];

    sprintf ( m1UsernameStr, "%s/%s", g_mediumOneDeviceCredentials.projectId, g_mediumOneDeviceCredentials.userId );
    sprintf ( m1Password, "%s/%s", g_mediumOneDeviceCredentials.apiKey, g_mediumOneDeviceCredentials.password );

    status = mqtt_netx_connect ( g_mediumOneDeviceCredentials.name, "mqtt2.mediumone.com", 61619, m1UsernameStr,
                                 m1Password, 0, 0, 0, 0 );

    if ( status == 1 )
    {

//    m1_register_subscription_callback ( mqtt_message_callback );
//
//    status = m1_connect ( "mqtt2.mediumone.com", /*61620*/61619, g_mediumOneDeviceCredentials.userId,
//                          g_mediumOneDeviceCredentials.password, g_mediumOneDeviceCredentials.projectId,
//                          g_mediumOneDeviceCredentials.apiKey, g_mediumOneDeviceCredentials.name, 5, 5, 60, 1 );

        sprintf ( g_topic_name, "0/%s/%s/%s", g_mediumOneDeviceCredentials.projectId,
                  g_mediumOneDeviceCredentials.userId, g_mediumOneDeviceCredentials.name );

        char initialConnectMessage [ 256 ];
        sprintf ( initialConnectMessage, "{\"init_connect\":{\"name\":\"%s\"}}", g_mediumOneDeviceCredentials.name );
        mqtt_netx_publish ( g_topic_name, initialConnectMessage, 0 );

//    m1_publish_event ( initialConnectMessage, NULL );

//TODO: Is this needed?
// extern const sf_comms_instance_t g_sf_comms0;
// m1_initialize_comms(&g_sf_comms0);
    }

    return ( status == 1 );
}

unsigned int mediumOneCloudImpl ( char * payload, size_t maxLength )
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

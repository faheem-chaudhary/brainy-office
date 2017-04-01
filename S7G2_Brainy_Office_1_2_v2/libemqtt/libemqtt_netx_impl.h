/*
 * libemqtt_netx_impl.h
 *
 *  Created on: Mar 2, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#ifndef LIBEMQTT_NETX_IMPL_H_
    #define LIBEMQTT_NETX_IMPL_H_
    #include "stdbool.h"
    #include "libemqtt.h"

typedef struct
{
        char userName [ MQTT_CONF_USERNAME_LENGTH ];
        char password [ MQTT_CONF_PASSWORD_LENGTH ];
        unsigned long hostIpAddress;
        unsigned short port;
        bool isKeepAlive;
        uint8_t keepAliveDelay;
        bool isRetryOnDisconnect;
        uint8_t retrylimit;
        uint8_t retryDelay;
} MqttConnection_t;

int mqtt_netx_connect ( const char * client_id, const MqttConnection_t * connection );

//                        const char * host, int mqtt_port, const char * username,
//                        const char * password, int keep_alive, int retry_limit, int retry_delay, int mqtt_heart_beat );

int mqtt_netx_disconnect ();

int mqtt_netx_publish ( const char* topic, const char* msg, uint8_t retain );

int mqtt_netx_publish_with_qos ( const char* topic, const char* msg, uint8_t retain, uint8_t qos,
                                 uint16_t* message_id );

int mqtt_netx_pubrel ( uint16_t message_id );

int mqtt_netx_subscribe (
        const char* topic, uint16_t* message_id,
        void (*subscription_callback) ( const char * topic, const char * message_id, uint16_t message_length ) );

int mqtt_netx_unsubscribe ( const char* topic, uint16_t* message_id );

int mqtt_netx_ping ();

#endif /* LIBEMQTT_NETX_IMPL_H_ */

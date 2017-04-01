/*
 * libemqtt_netx_impl.c
 *
 *  Created on: Mar 2, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

/*
 * This file is part of libemqtt.
 *
 * libemqtt is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libemqtt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with libemqtt.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *
 * Created by Vicente Ruiz Rodríguez
 * Copyright 2012 Vicente Ruiz Rodríguez <vruiz2.0@gmail.com>. All rights reserved.
 *
 */
#include "libemqtt_netx_impl.h"
#include <libemqtt.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "nx_bsd.h"
#include <signal.h>
#include "tx_api.h"

#define RCVBUFSIZE 1024
uint8_t packet_buffer [ RCVBUFSIZE ];

int socket_id;
mqtt_broker_handle_t g_broker;
TX_THREAD * gp_thread;

void alive ( int sig );
int init_netx_socket ( mqtt_broker_handle_t* broker, const MqttConnection_t * connection );
int send_packet ( void* socket_info, const void* buf, unsigned int count );
int close_socket ( mqtt_broker_handle_t* broker );
int read_packet ( int timeout );

int send_packet ( void* socket_info, const void* buf, unsigned int count )
{
    int fd = * ( (int*) socket_info );
    int bytesSend = (int) send ( fd, buf, (int) count, 0 );
    return bytesSend;
}

int init_netx_socket ( mqtt_broker_handle_t* broker, const MqttConnection_t * connection )
{
    int socketStatus = 0;

    // Create the socket
    socketStatus = ( socket_id = socket ( PF_INET, SOCK_STREAM, 0 ) );
    if ( socketStatus < 0 )
    {
        return -1;
    }

    // Disable Nagle Algorithm ... disabling this because NetX BSD has this as default option
//    socketStatus = setsockopt ( socket_id, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof ( flag ) );
//    if ( socketStatus < 0 )
//        return -2;

    struct sockaddr_in socket_address;
    // Create the stuff we need to connect
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = connection->port;
    socket_address.sin_addr.s_addr = connection->hostIpAddress;

    // Connect the socket
    socketStatus = ( connect ( socket_id, (struct sockaddr*) &socket_address, sizeof ( socket_address ) ) );
    if ( socketStatus < 0 )
    {
        return -1;
    }

    // MQTT stuffs
    if ( connection->isKeepAlive )
    {
        mqtt_set_alive ( broker, connection->keepAliveDelay );
    }
    broker->socket_info = (void*) &socket_id;
    broker->send = send_packet;

    gp_thread = tx_thread_identify ();
    return 0;
}

int close_socket ( mqtt_broker_handle_t* broker )
{
    int fd = * ( (int*) broker->socket_info );
    return close ( fd );
}

int read_packet ( int timeout )
{
    if ( timeout > 0 )
    {
        fd_set readfds;
        struct timeval tmv;

        // Initialize the file descriptor set
        FD_ZERO ( &readfds );
        FD_SET ( socket_id, &readfds );

        // Initialize the timeout data structure
        tmv.tv_sec = timeout;
        tmv.tv_usec = 0;

//        readfds [ 0 ] = socket_id;

// select returns 0 if successful, > 0 for found descriptors, -1 if error
        int selectStatus = select ( NX_BSD_SOCKFD_START + 1, &readfds, NULL, NULL, &tmv );
        if ( selectStatus < 0 )
            return -2;
    }

    int total_bytes = 0, bytes_rcvd, packet_length;
    memset ( packet_buffer, 0, sizeof ( packet_buffer ) );

    if ( ( bytes_rcvd = recv ( socket_id, ( packet_buffer + total_bytes ), RCVBUFSIZE, 0 ) ) <= 0 )
    {
        return -1;
    }

    total_bytes += bytes_rcvd; // Keep tally of total bytes
    if ( total_bytes < 2 )
    {
        return -1;
    }

    // now we have the full fixed header in packet_buffer
    // parse it for remaining length and number of bytes
    uint16_t rem_len = mqtt_parse_rem_len ( packet_buffer );
    uint8_t rem_len_bytes = mqtt_num_rem_len_bytes ( packet_buffer );

    //packet_length = packet_buffer[1] + 2; // Remaining length + fixed header length
    // total packet length = remaining length + byte 1 of fixed header + remaning length part of fixed header
    packet_length = rem_len + rem_len_bytes + 1;

    while ( total_bytes < packet_length )    // Reading the packet
    {
        if ( ( bytes_rcvd = recv ( socket_id, ( packet_buffer + total_bytes ), RCVBUFSIZE, 0 ) ) <= 0 )
        {
            return -1;
        }
        total_bytes += bytes_rcvd; // Keep tally of total bytes
    }

    return packet_length;
}

int mqtt_netx_disconnect ()
{
    int status = mqtt_disconnect ( &g_broker );
    return status;
}

int mqtt_netx_publish ( const char* topic, const char* msg, uint8_t retain )
{
    int status = mqtt_publish ( &g_broker, topic, msg, retain );
    return status;
}

int mqtt_netx_publish_with_qos ( const char* topic, const char* msg, uint8_t retain, uint8_t qos, uint16_t* message_id )
{
    int status = mqtt_publish_with_qos ( &g_broker, topic, msg, retain, qos, message_id );
    return status;
}

int mqtt_netx_pubrel ( uint16_t message_id )
{
    int status = mqtt_pubrel ( &g_broker, message_id );
    return status;
}

int mqtt_netx_subscribe (
        const char* topic, uint16_t* message_id,
        void (*subscription_callback) ( const char * topic, const char * message_id, uint16_t message_length ) )
{
    SSP_PARAMETER_NOT_USED ( subscription_callback );
    int status = mqtt_subscribe ( &g_broker, topic, message_id );
    return status;
}

int mqtt_netx_unsubscribe ( const char* topic, uint16_t* message_id )
{
    int status = mqtt_unsubscribe ( &g_broker, topic, message_id );
    return status;
}

int mqtt_netx_ping ()
{
    return mqtt_ping ( &g_broker );
}

unsigned int g_keepalive = 30;
void alive ( int sig )
{
    SSP_PARAMETER_NOT_USED ( sig );

    mqtt_ping ( &g_broker );
    alarm ( g_keepalive );
}

int mqtt_netx_connect ( const char * client_id, const MqttConnection_t * connection )
{
    int status = 0;
    mqtt_init ( &g_broker, client_id );
    mqtt_init_auth ( &g_broker, connection->userName, connection->password );
    init_netx_socket ( &g_broker, connection );
    status = mqtt_connect ( &g_broker );

    int packet_length = read_packet ( 1 );

    if ( packet_length < 0 )
    {
        fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
        return -1;
    }

    if ( MQTTParseMessageType ( packet_buffer ) != MQTT_MSG_CONNACK )
    {
        fprintf ( stderr, "CONNACK expected!\n" );
        return -2;
    }

    if ( packet_buffer [ 3 ] != 0x00 )
    {
        fprintf ( stderr, "CONNACK failed!\n" );
        return -2;
    }

    if ( connection->isKeepAlive )
    {
        g_keepalive = connection->keepAliveDelay;

        // Signals after connect MQTT
        signal ( SIGALRM, alive );
        alarm ( g_keepalive );
    }

    return status;
}
#if 0
int pub_test ( int argc, char* argv [] )
{
    int packet_length;
    uint16_t msg_id, msg_id_rcv;

    mqtt_init ( &g_broker, "avengalvon" );
    mqtt_init_auth ( &g_broker, "cid", "campeador" );
    init_socket ( &g_broker, "127.0.0.1", 1883, 30 );

    // >>>>> CONNECT
    mqtt_connect ( &g_broker );
    // <<<<< CONNACK
    packet_length = read_packet ( 1 );
    if ( packet_length < 0 )
    {
        fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
        return -1;
    }

    if ( MQTTParseMessageType ( packet_buffer ) != MQTT_MSG_CONNACK )
    {
        fprintf ( stderr, "CONNACK expected!\n" );
        return -2;
    }

    if ( packet_buffer [ 3 ] != 0x00 )
    {
        fprintf ( stderr, "CONNACK failed!\n" );
        return -2;
    }

    // >>>>> PUBLISH QoS 0
    printf ( "Publish: QoS 0\n" );
    mqtt_publish ( &g_broker, "hello/emqtt", "Example: QoS 0", 0 );

    // >>>>> PUBLISH QoS 1
    printf ( "Publish: QoS 1\n" );
    mqtt_publish_with_qos ( &g_broker, "hello/emqtt", "Example: QoS 1", 0, 1, &msg_id );
    // <<<<< PUBACK
    packet_length = read_packet ( 1 );
    if ( packet_length < 0 )
    {
        fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
        return -1;
    }

    if ( MQTTParseMessageType ( packet_buffer ) != MQTT_MSG_PUBACK )
    {
        fprintf ( stderr, "PUBACK expected!\n" );
        return -2;
    }

    msg_id_rcv = mqtt_parse_msg_id ( packet_buffer );
    if ( msg_id != msg_id_rcv )
    {
        fprintf ( stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv );
        return -3;
    }

    // >>>>> PUBLISH QoS 2
    printf ( "Publish: QoS 2\n" );
    mqtt_publish_with_qos ( &g_broker, "hello/emqtt", "Example: QoS 2", 1, 2, &msg_id );// Retain
    // <<<<< PUBREC
    packet_length = read_packet ( 1 );
    if ( packet_length < 0 )
    {
        fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
        return -1;
    }

    if ( MQTTParseMessageType ( packet_buffer ) != MQTT_MSG_PUBREC )
    {
        fprintf ( stderr, "PUBREC expected!\n" );
        return -2;
    }

    msg_id_rcv = mqtt_parse_msg_id ( packet_buffer );
    if ( msg_id != msg_id_rcv )
    {
        fprintf ( stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv );
        return -3;
    }

    // >>>>> PUBREL
    mqtt_pubrel ( &g_broker, msg_id );
    // <<<<< PUBCOMP
    packet_length = read_packet ( 1 );
    if ( packet_length < 0 )
    {
        fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
        return -1;
    }

    if ( MQTTParseMessageType ( packet_buffer ) != MQTT_MSG_PUBCOMP )
    {
        fprintf ( stderr, "PUBCOMP expected!\n" );
        return -2;
    }

    msg_id_rcv = mqtt_parse_msg_id ( packet_buffer );
    if ( msg_id != msg_id_rcv )
    {
        fprintf ( stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv );
        return -3;
    }

    // >>>>> DISCONNECT
    mqtt_disconnect ( &g_broker );
    close_socket ( &g_broker );
    return 0;
}

void term ( int sig )
{
    fprintf ( stderr, "Goodbye!\n" );
    // >>>>> DISCONNECT
    mqtt_disconnect ( &g_broker );
    close_socket ( &g_broker );

    exit ( 0 );
}

/**
 * Main routine
 *
 */
int sub_test ()
{
    int packet_length;
    uint16_t msg_id, msg_id_rcv;

    mqtt_init ( &g_broker, "client-id" );
    //mqtt_init_auth(&broker, "quijote", "rocinante");
    init_socket ( &g_broker, "127.0.0.1", 1883, g_keepalive );

    // >>>>> CONNECT
    mqtt_connect ( &g_broker );
    // <<<<< CONNACK
    packet_length = read_packet ( 1 );
    if ( packet_length < 0 )
    {
        fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
        return -1;
    }

    if ( MQTTParseMessageType ( packet_buffer ) != MQTT_MSG_CONNACK )
    {
        fprintf ( stderr, "CONNACK expected!\n" );
        return -2;
    }

    if ( packet_buffer [ 3 ] != 0x00 )
    {
        fprintf ( stderr, "CONNACK failed!\n" );
        return -2;
    }

    // Signals after connect MQTT
    signal ( SIGALRM, alive );
    alarm ( g_keepalive );
    signal ( SIGINT, term );

    // >>>>> SUBSCRIBE
    mqtt_subscribe ( &g_broker, "public/test/topic", &msg_id );
    // <<<<< SUBACK
    packet_length = read_packet ( 1 );
    if ( packet_length < 0 )
    {
        fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
        return -1;
    }

    if ( MQTTParseMessageType ( packet_buffer ) != MQTT_MSG_SUBACK )
    {
        fprintf ( stderr, "SUBACK expected!\n" );
        return -2;
    }

    msg_id_rcv = mqtt_parse_msg_id ( packet_buffer );
    if ( msg_id != msg_id_rcv )
    {
        fprintf ( stderr, "%d message id was expected, but %d message id was found!\n", msg_id, msg_id_rcv );
        return -3;
    }

    while ( 1 )
    {
        // <<<<<
        packet_length = read_packet ( 0 );
        if ( packet_length == -1 )
        {
            fprintf ( stderr, "Error(%d) on read packet!\n", packet_length );
            return -1;
        }
        else if ( packet_length > 0 )
        {
            printf ( "Packet Header: 0x%x...\n", packet_buffer [ 0 ] );
            if ( MQTTParseMessageType ( packet_buffer ) == MQTT_MSG_PUBLISH )
            {
                uint8_t topic [ 255 ], msg [ 1000 ];
                uint16_t len;
                len = mqtt_parse_pub_topic ( packet_buffer, topic );
                topic [ len ] = '\0'; // for printf
                len = mqtt_parse_publish_msg ( packet_buffer, msg );
                msg [ len ] = '\0';// for printf
                printf ( "%s %s\n", topic, msg );
            }
        }

    }
    return 0;
}
#endif

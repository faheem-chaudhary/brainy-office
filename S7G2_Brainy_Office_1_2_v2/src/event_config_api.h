/*
 * event_config_api.h
 *
 *  Created on: Feb 16, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#ifndef EVENT_CONFIG_API_H_
    #define EVENT_CONFIG_API_H_

    #include "commons.h"
    #include "sf_message.h"
    #include "sf_message_port.h"

//re-defining the stringDataFunction here, due to incomprehensible compiler error
typedef unsigned int (*stringDataFunction) ( char * payload, size_t payloadLength );

typedef struct
{
        sf_message_header_t header;
        TX_THREAD * sender;
        stringDataFunction stringDataFunctionPtr;
} event_config_payload_t;

#endif /* EVENT_CONFIG_API_H_ */

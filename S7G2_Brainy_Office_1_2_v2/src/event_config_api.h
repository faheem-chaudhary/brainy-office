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

typedef struct
{
        sf_message_header_t header;
        uint8_t senderId;
} event_config_payload_t;

#endif /* EVENT_CONFIG_API_H_ */

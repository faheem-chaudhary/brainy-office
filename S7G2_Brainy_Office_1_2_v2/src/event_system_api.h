/*
 * event_system_api.h
 *
 *  Created on: Feb 16, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#ifndef EVENT_SYSTEM_API_H_
    #define EVENT_SYSTEM_API_H_

    #include "sf_message.h"
    #include "sf_message_port.h"
    #include "tx_api.h"

typedef struct
{
        sf_message_header_t header;
        TX_THREAD * sender;
} event_system_payload_t;

#endif /* EVENT_SYSTEM_API_H_ */

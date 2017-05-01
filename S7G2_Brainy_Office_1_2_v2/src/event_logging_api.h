/*
 * event_logging.h
 *
 *  Created on: Apr 28, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#ifndef EVENT_LOGGING_API_H_
    #define EVENT_LOGGING_API_H_

    #include "sf_message.h"
    #include "sf_message_port.h"

typedef struct
{
        sf_message_header_t header;
} event_logging_payload_t;

#endif /* EVENT_LOGGING_API_H_ */

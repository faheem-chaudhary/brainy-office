/*
 * cloud_medium1_adapter.h
 *
 *  Created on: Feb 21, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#ifndef CLOUD_MEDIUM1_ADAPTER_H_
    #define CLOUD_MEDIUM1_ADAPTER_H_

    #include "stddef.h"

    #define M1_CONFIG_VALUE_LENGTH  33
    #define M1_API_KEY_VALUE_LENGTH 65

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

extern MediumOneDeviceCredentials_t g_mediumOneDeviceCredentials;

unsigned int mediumOneConfigImpl ( char * configData, size_t dataLength );
unsigned int mediumOneInitImpl ( char * configData, size_t dataLength );
unsigned int mediumOnePublishImpl ( char * payload, size_t maxLength );

#endif /* CLOUD_MEDIUM1_ADAPTER_H_ */

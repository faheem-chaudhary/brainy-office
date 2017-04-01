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

unsigned int mediumOneConfigImpl ( char * configData, size_t dataLength );
unsigned int mediumOneInitImpl ( char * configData, size_t dataLength );
unsigned int mediumOnePublishImpl ( char * payload, size_t maxLength );

#endif /* CLOUD_MEDIUM1_ADAPTER_H_ */

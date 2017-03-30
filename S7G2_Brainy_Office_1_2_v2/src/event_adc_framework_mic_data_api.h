/*
 * adc_framework_mic_data_api.h
 *
 *  Created on: Mar 27, 2017
 *      Author: Faheem Inayat
 * Created for: Renesas Electronics America HQ, Santa Clara, CA, USA
 *
 * Copyright (c) 2017 Renesas Electronics America (REA) and Faheem Inayat
 */

#ifndef EVENT_ADC_FRAMEWORK_MIC_DATA_API_H_
    #define EVENT_ADC_FRAMEWORK_MIC_DATA_API_H_

    #include "commons.h"
    #include "sf_message.h"
    #include "sf_message_port.h"
    #include "sf_message_api.h"

typedef struct
{
        sf_message_header_t header;
        void * dataPointer;
} event_adc_framework_mic_data_payload_t;

#endif /* EVENT_ADC_FRAMEWORK_MIC_DATA_API_H_ */

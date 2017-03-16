#include "sensor_humidity_temperature_thread.h"

void AMS_ENS210_thread_entry ( void );

#define PART_ID 0x00
#define SYS_CTRL 0x10
#define SYS_STAT 0x11
#define SENS_RUN 0x21
#define H_RUN 0x02
#define T_RUN 0x01
#define SENS_START 0x22
#define H_START 0x02
#define T_START 0x01
#define T_VAL 0x30
#define H_VAL 0x33

typedef union AMS_ENS210_REG30
{
        uint32_t lword;
        uint8_t bytes [ 4 ];
        struct
        {
                uint8_t :8;
                uint8_t crc :7;
                uint8_t valid :1;
                uint16_t data;
        } bit;
} AmsTemperatureData_t;

typedef union AMS_ENS210_REG33
{
        uint32_t lword;
        uint8_t bytes [ 4 ];
        struct
        {
                uint8_t :8;
                uint8_t crc :7;
                uint8_t valid :1;
                uint16_t data;
        } bit;
} AmsHumidityData_t;

AmsHumidityData_t g_humidity;
AmsTemperatureData_t g_temperature;

/* Sensor Humidity and Temperature (AMS EN210) Thread entry function */
void sensor_humidity_temperature_thread_entry ( void )
{
    volatile ssp_err_t err;
    uint8_t commandTemperature = T_VAL;
    uint8_t commandHumidity = H_VAL;
    uint8_t ENS210_init_seq [] =
    { SENS_RUN, H_RUN | T_RUN, H_START | T_START };

    err = g_ams_en210_temp_humid.p_api->open ( g_ams_en210_temp_humid.p_ctrl, g_ams_en210_temp_humid.p_cfg );
    APP_ERR_TRAP (err)

    err = g_ams_en210_temp_humid.p_api->write ( g_ams_en210_temp_humid.p_ctrl, ENS210_init_seq,
                                                sizeof ( ENS210_init_seq ), false, 10 );
    APP_ERR_TRAP (err)

    tx_thread_sleep ( 13 ); // 122 ms conversion time

    while ( 1 )
    {
        err = g_ams_en210_temp_humid.p_api->write ( g_ams_en210_temp_humid.p_ctrl, &commandTemperature, 1, false, 10 );
        APP_ERR_TRAP (err)

        err = g_ams_en210_temp_humid.p_api->read ( g_ams_en210_temp_humid.p_ctrl, g_temperature.bytes, 3, false, 10 );
        APP_ERR_TRAP (err)

        err = g_ams_en210_temp_humid.p_api->write ( g_ams_en210_temp_humid.p_ctrl, &commandHumidity, 1, false, 10 );
        APP_ERR_TRAP (err)

        err = g_ams_en210_temp_humid.p_api->read ( g_ams_en210_temp_humid.p_ctrl, g_humidity.bytes, 3, false, 10 );
        APP_ERR_TRAP (err)

        tx_thread_sleep ( 20 );
    }
}

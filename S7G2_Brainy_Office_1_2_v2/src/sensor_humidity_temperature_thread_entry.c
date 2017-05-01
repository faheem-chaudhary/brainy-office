#include "sensor_humidity_temperature_thread.h"
#include "commons.h"
#include "math.h"

/// -------------------------------------------------------- ///
///   SECTION: Macro Definitions                             ///
// 7654 3210
// Polynomial 0b 1000 1001 ~ x^7+x^3+x^0
// 0x 8 9
#define CRC7WIDTH 7 // 7 bits CRC has polynomial of 7th order (has 8 terms)
#define CRC7POLY 0x89 // The 8 coefficients of the polynomial
#define CRC7IVEC 0x7F // Initial vector has all 7 bits high
// Payload data
#define DATA7WIDTH 17
#define DATA7MASK ((1UL<<DATA7WIDTH)-1) // 0b 0 1111 1111 1111 1111
#define DATA7MSB (1UL<<(DATA7WIDTH-1)) // 0b 1 0000 0000 0000 0000

/// --  END OF: Macro Definitions -------------------------  ///

/// -------------------------------------------------------- ///
///   SECTION: Global/extern Variable Declarations           ///
///                        -- None --                        ///

/// --  END OF: Global/extern Variable Declarations -------- ///

/// -------------------------------------------------------- ///
///   SECTION: Local Type Definitions                        ///
typedef union AMS_ENS210_DATA
{
        uint32_t lword;
        uint8_t bytes [ 4 ];
        struct
        {
                uint16_t data;
                uint8_t valid :1;
                uint8_t crc :7;
                uint8_t :8;
        } bit;
} AmsEns210Data_t;

/// --  END OF: Local Type Definitions --------------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Variable Declarations     ///
// minimum values sensor can read
static double temperatureCelsius = -40.0;
static double temperatureFahrenheit = -40.0;
static double humidityPercent = 0.0;
static ULONG sensor_humidity_temperature_thread_wait = 20;

/// --  END OF: Static (file scope) Variable Declarations -- ///

/// -------------------------------------------------------- ///
///   SECTION: Global Function Declarations                  ///
void sensor_humidity_temperature_thread_entry ( void );

/// --  END OF: Global Function Declarations --------------- ///

/// -------------------------------------------------------- ///
///   SECTION: Static (file scope) Function Declarations     ///
static unsigned int sensor_humidity_temperature_formatDataForCloudPublish (
        const event_sensor_payload_t * const eventPtr, char * payload, size_t payloadLength );
static bool validateEns210SensorData ( AmsEns210Data_t * value );
static uint32_t calculateCrc7 ( uint32_t val );

/// --  END OF: Static (file scope) Function Declarations -- ///

/**
 +-------------------------------------------------------------------------------------------------------+
 |Symbol  |   Parameter                 |    Conditions                 | Min  | Typ   | Max   | Unit    |
 +-------------------------------------------------------------------------------------------------------+
 |T(range)| Temperature range           |                               | -40  |       | 100   | °C      |
 +-------------------------------------------------------------------------------------------------------+
 |        |                             |    T(A) = 0°C to 70°C; 3σ     |      |       | 0.2   | °C      |
 |T(acc)  | Temperature accuracy        +----------------------------------------------------------------+
 |        |                             |   T(A) = −40°C to 100°C; 3σ   |      |       | 0.5   | °C      |
 +-------------------------------------------------------------------------------------------------------+
 |T(res)  | Temperature resolution      |                               |      | 0.016 |       | °C      |
 +-------------------------------------------------------------------------------------------------------+
 |t(resp) | Response time               | T step of 10°C by submersion  |      |   1   |       |  sec    |
 |        |                             | (in 0°C to 70°C range); τ63 % |      |       |       |         |
 +-------------------------------------------------------------------------------------------------------+
 |T(rep)  | Temperature repeatability   | 3σ of consecutive measurement |      |       |       |         |
 |        |                             | values at constant conditions | -0.1 |       |  0.1  | °C      |
 +-------------------------------------------------------------------------------------------------------+
 |ΔT      | Temperature long term drift |                               |      | 0.005 |       | °C/year |
 +-------------------------------------------------------------------------------------------------------+
 **/

/**
 +-------------------------------------------------------------------------------------------------------+
 |Symbol  |   Parameter                 |    Conditions                 | Min  | Typ   | Max   | Unit    |
 +-------------------------------------------------------------------------------------------------------+
 |H(range)| Relative humidity range     |                               |  0   |       | 100   | %RH     |
 +-------------------------------------------------------------------------------------------------------+
 |        |                             | T(A) = 25°C; RH=20% to 80%    |      |       | 3.5   | %RH     |
 |H(acc)  | Relative humidity accuracy  +----------------------------------------------------------------+
 |        |                             | T(A) = 25°C; RH=0% to 100%    |      |       |  5    | %RH     |
 +-------------------------------------------------------------------------------------------------------+
 |H(res)  | Relative humidity resolution|                               |      |  0.03 |       | %RH     |
 +-------------------------------------------------------------------------------------------------------+
 |        |                             | RH step of 20%RH (in 40%RH to |      |       |       |         |
 |t(resp) | Response time               | 80%RH range); τ63%(1);        |      |   3   |   5   |  sec    |
 |        |                             | 1m/s flow; TA=25°C            |      |       |       |         |
 +-------------------------------------------------------------------------------------------------------+
 |H(hys)  | Relative humidity hysteresis| TA = 25°C; RH = 20%RH - 90%RH;|      | +/-1  |       | %RH     |
 |        |                             | 30minutes exposure time       |      |       |       |         |
 +-------------------------------------------------------------------------------------------------------+
 |H(rep)  | Relative humidity           | 3σ of consecutive measurement |      |       |       |         |
 |        | repeatability               | values at TA=25°C and RH=40%  |      |+/-0.1 |       | %RH     |
 +-------------------------------------------------------------------------------------------------------+
 |ΔH      | Humidity long term drift    | TA = 25°C                     |      | 0.25  |       | %RH/year|
 +-------------------------------------------------------------------------------------------------------+
 **/

/* Sensor Humidity and Temperature (AMS EN210) Thread entry function */
void sensor_humidity_temperature_thread_entry ( void )
{
    volatile ssp_err_t err;
    uint8_t commandTemperature = 0x30;
    uint8_t commandHumidity = 0x33;
    uint8_t ENS210_init_seq [] =
    { 0x21, 0x03, 0x03 }; // Start, read both Temperature and Humidity, run both Temperature and Humidity in Continous Mode

    AmsEns210Data_t humiditySensorReading;
    AmsEns210Data_t temperatureSensorReading;
    double difference = 0.0;
    double tempThresholdInCelsius = 0.2;
    double humidityThresholdInPercent = 3.5;

//    uint16_t temperatureThreshold = ( uint16_t ) ( ( ( uint16_t ) ( tempThresholdInCelsius + 273.15 ) ) * 64 );
//    uint16_t humidityThreshold = ( uint16_t ) ( 3.5 * 512 );

    // impossible minimum values for sensor to detect
    double prevTemperatureCelsius = -200.0;
//    double prevTemperatureFahrenheit = -200.0;
    double prevHumidityPercent = -100.0;

    sf_message_header_t * message;
    ssp_err_t msgStatus;

    const uint8_t sensor_humidity_temperature_thread_id = getUniqueThreadId ();
    registerSensorForCloudPublish ( sensor_humidity_temperature_thread_id, NULL );

    err = g_i2c_device_en210_temp_humid.p_api->open ( g_i2c_device_en210_temp_humid.p_ctrl,
                                                      g_i2c_device_en210_temp_humid.p_cfg );
    handleError ( err );

    err = g_i2c_device_en210_temp_humid.p_api->write ( g_i2c_device_en210_temp_humid.p_ctrl, ENS210_init_seq,
                                                       sizeof ( ENS210_init_seq ), false, 10 );
    handleError ( err );

    tx_thread_sleep ( 13 ); // 122 ms conversion time

    bool isForedDataCalculation = false;

    while ( 1 )
    {
        msgStatus = messageQueuePend ( &sensor_humidity_temperature_thread_message_queue, (void **) &message,
                                       sensor_humidity_temperature_thread_wait );

        if ( msgStatus == SSP_SUCCESS )
        {
            if ( message->event_b.class_code == SF_MESSAGE_EVENT_CLASS_SYSTEM )
            {
                //TODO: anything related to System Message Processing goes here

                if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_CLOUD_AVAILABLE )
                {
                    registerSensorForCloudPublish ( sensor_humidity_temperature_thread_id,
                                                    sensor_humidity_temperature_formatDataForCloudPublish );
                }
                else if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_CLOUD_DISCONNECTED )
                {
                    registerSensorForCloudPublish ( sensor_humidity_temperature_thread_id, NULL );
                }
                else if ( message->event_b.code == SF_MESSAGE_EVENT_SYSTEM_BUTTON_S4_PRESSED )
                {
                    isForedDataCalculation = true;
                }
            }

            messageQueueReleaseBuffer ( (void **) &message );
        }

        err = g_i2c_device_en210_temp_humid.p_api->write ( g_i2c_device_en210_temp_humid.p_ctrl, &commandTemperature, 1,
                                                           false, 10 );
        handleError ( err );

        err = g_i2c_device_en210_temp_humid.p_api->read ( g_i2c_device_en210_temp_humid.p_ctrl,
                                                          temperatureSensorReading.bytes, 3, false, 10 );
        handleError ( err );

        err = g_i2c_device_en210_temp_humid.p_api->write ( g_i2c_device_en210_temp_humid.p_ctrl, &commandHumidity, 1,
                                                           false, 10 );
        handleError ( err );

        err = g_i2c_device_en210_temp_humid.p_api->read ( g_i2c_device_en210_temp_humid.p_ctrl,
                                                          humiditySensorReading.bytes, 3, false, 10 );
        handleError ( err );

        if ( validateEns210SensorData ( &temperatureSensorReading ) == true )
        {
            // float TinK = (double) t_data / 64; // Temperature in Kelvin
            // float TinC = TinK - 273.15; // Temperature in Celsius
            // float TinF = TinC * 1.8 + 32.0; // Temperature in Fahrenheit

            temperatureCelsius = ( ( ( (double) temperatureSensorReading.bit.data ) / 64 ) - 273.15 );

            difference = fabs ( temperatureCelsius - prevTemperatureCelsius );

            if ( difference > tempThresholdInCelsius || isForedDataCalculation )
            {
                temperatureFahrenheit = ( ( temperatureCelsius * 1.8 ) + 32.0 );

                // post Sensor Message
                postSensorEventMessage ( sensor_humidity_temperature_thread_id, SF_MESSAGE_EVENT_SENSOR_NEW_DATA,
                                         &temperatureFahrenheit );

                prevTemperatureCelsius = temperatureCelsius;
            }
        }

        if ( validateEns210SensorData ( &humiditySensorReading ) == true )
        {
            // float H = (double) h_data / 512; // relative humidity (in %)
            humidityPercent = ( ( (double) humiditySensorReading.bit.data ) / 512 );

            difference = fabs ( humidityPercent - prevHumidityPercent );

            if ( difference > humidityThresholdInPercent || isForedDataCalculation )
            {
                // post sensor message
                postSensorEventMessage ( sensor_humidity_temperature_thread_id, SF_MESSAGE_EVENT_SENSOR_NEW_DATA,
                                         &humidityPercent );

                prevHumidityPercent = humidityPercent;
            }
        }

        isForedDataCalculation = false;
    }
}

// Compute the CRC-7 of 'val' (should only have 17 bits)
uint32_t calculateCrc7 ( uint32_t val )
{
    // Setup polynomial
    uint32_t pol = CRC7POLY;

    // Align polynomial with data
    pol = pol << ( DATA7WIDTH - CRC7WIDTH - 1 );

    // Loop variable (indicates which bit to test, start with highest)
    uint32_t bit = DATA7MSB;

    // Make room for CRC value
    val = val << CRC7WIDTH;
    bit = bit << CRC7WIDTH;
    pol = pol << CRC7WIDTH;

    // Insert initial vector
    val |= CRC7IVEC;

    // Apply division until all bits done
    while ( bit & ( DATA7MASK << CRC7WIDTH ) )
    {
        if ( bit & val )
        {
            val ^= pol;
        }
        bit >>= 1;
        pol >>= 1;
    }
    return val;
}

bool validateEns210SensorData ( AmsEns210Data_t * value )
{
    if ( value->bit.valid == 1 )
    {
        uint32_t calculatedCrc = calculateCrc7 ( ( uint32_t ) ( value->bit.data | 0x10000 ) );

        if ( calculatedCrc == value->bit.crc )
        {
            return true;
        }
    }
    return false;
}

unsigned int sensor_humidity_temperature_formatDataForCloudPublish ( const event_sensor_payload_t * const eventPtr,
                                                                     char * payload, size_t payloadLength )
{
    int bytesProcessed = 0;
    if ( eventPtr != NULL )
    {
        if ( ( eventPtr->dataPointer ) == &temperatureFahrenheit )
        {
            // it's a temperature publish event
            bytesProcessed = snprintf ( payload, payloadLength,
                                        "{\"temperature\":{\"fahrenheit\": %.3f,\"celsius\": %.3f}}",
                                        temperatureFahrenheit, temperatureCelsius );
        }
        else if ( ( eventPtr->dataPointer ) == &humidityPercent )
        {
            // it's a humidity publish event
            bytesProcessed = snprintf ( payload, payloadLength, "{\"relative_humidity\":{\"percent\":%.3f}}",
                                        humidityPercent );
        }
    }

    if ( bytesProcessed < 0 )
    {
        return 0;
    }

    return (unsigned int) bytesProcessed;
}



/*
這個程式範例用於 SHT40 溫溼度感測器 及 SCD41 感測器**  
This Example Code is for the SHT40 Temperature & Humidity Sensor and SCD41 Sensor**  

SHT40 溫溼度感測器  
購買連結: [https://store.makdev.net/products/SHT40](https://store.makdev.net/products/SHT40)  

SHT40 Temperature & Humidity Sensor  
Purchase Link: [https://store.makdev.net/products/SHT40](https://store.makdev.net/products/SHT40)  

SCD41 感測器  
購買連結: [https://store.makdev.net/products/SCD41_Module](https://store.makdev.net/products/SCD41_Module)  

SCD41 Sensor  
Purchase Link: [https://store.makdev.net/products/SCD41_Module](https://store.makdev.net/products/SCD41_Module)  

說明  
為了要強調 QWIIC 硬體功能特色，所以將兩個同樣是 QWIIC 介面的感測器串接起來。  

Description  
To highlight the QWIIC hardware functionality, both sensors, which use the QWIIC interface, are connected in series.  

必需安裝程式庫  
請在 Arduino IDE 中安裝以下程式庫：  
- Adafruit_SHT4X  
- Sensirion SCD4x sensors  

### Required Libraries  
Please install the following libraries in Arduino IDE:  
- Adafruit_SHT4X  
- Sensirion SCD4x sensors  


*/
#include "Adafruit_SHT4x.h"
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include "Adafruit_SHT4x.h"

// macro definitions
// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

SensirionI2cScd4x sensor;
static char errorMessage[64];
static int16_t error;

uint8_t u8state;
unsigned long u32wait;

Adafruit_SHT4x sht4 = Adafruit_SHT4x();

void PrintUint64(uint64_t& value) {
    Serial.print("0x");
    Serial.print((uint32_t)(value >> 32), HEX);
    Serial.print((uint32_t)(value & 0xFFFFFFFF), HEX);
}

void setup()
{
    uint64_t serialNumber = 0;
    
    Serial.begin(115200);
    Wire.begin();
    sensor.begin(Wire, SCD41_I2C_ADDR_62);

    Serial.println("Adafruit SHT4x test");
    if (! sht4.begin()) {
        Serial.println("Couldn't find SHT4x");
        while (1) delay(1);
    }
    Serial.println("Found SHT4x sensor");	
    Serial.print("Serial number 0x");
    Serial.println(sht4.readSerial(), HEX);

    // You can have 3 different precisions, higher precision takes longer
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    switch (sht4.getPrecision()) {
        case SHT4X_HIGH_PRECISION: 
        Serial.println("High precision");
        break;
        case SHT4X_MED_PRECISION: 
        Serial.println("Med precision");
        break;
        case SHT4X_LOW_PRECISION: 
        Serial.println("Low precision");
        break;
    }

    // You can have 6 different heater settings
    // higher heat and longer times uses more power
    // and reads will take longer too!
    sht4.setHeater(SHT4X_NO_HEATER);
    switch (sht4.getHeater()) {
        case SHT4X_NO_HEATER: 
        Serial.println("No heater");
        break;
        case SHT4X_HIGH_HEATER_1S: 
        Serial.println("High heat for 1 second");
        break;
        case SHT4X_HIGH_HEATER_100MS: 
        Serial.println("High heat for 0.1 second");
        break;
        case SHT4X_MED_HEATER_1S: 
        Serial.println("Medium heat for 1 second");
        break;
        case SHT4X_MED_HEATER_100MS: 
        Serial.println("Medium heat for 0.1 second");
        break;
        case SHT4X_LOW_HEATER_1S: 
        Serial.println("Low heat for 1 second");
        break;
        case SHT4X_LOW_HEATER_100MS: 
        Serial.println("Low heat for 0.1 second");
        break;
    }

    
    
    delay(30);
    // Ensure sensor is in clean state
    error = sensor.wakeUp();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute wakeUp(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
    }
    error = sensor.stopPeriodicMeasurement();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
    }
    error = sensor.reinit();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute reinit(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
    }
    // Read out information about the sensor
    error = sensor.getSerialNumber(serialNumber);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("serial number: ");
    PrintUint64(serialNumber);
    Serial.println();
    //
    // If temperature offset and/or sensor altitude compensation
    // is required, you should call the respective functions here.
    // Check out the header file for the function definitions.
    // Start periodic measurements (5sec interval)
    error = sensor.startPeriodicMeasurement();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute startPeriodicMeasurement(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    //
    // If low-power mode is required, switch to the low power
    // measurement function instead of the standard measurement
    // function above. Check out the header file for the definition.
    // For SCD41, you can also check out the single shot measurement example.
    //
    u32wait = millis() + 1000;
    u8state = 0; 
}

void loop()
{
    sensors_event_t humidity, temp;
    uint32_t timestamp = millis();

    switch( u8state ) {
        case 0:
            if (millis() > u32wait) u8state++; // wait state
        break;

        case 1:
            sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
            timestamp = millis() - timestamp;
            Serial.println("SHT40 sensor");
            Serial.print("  Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
            Serial.print("  Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

            Serial.print("  Read duration (ms): ");
            Serial.println(timestamp);
            u8state++;
            u32wait = millis() + 10;

        break;

        case 2:
            bool dataReady = false;
            uint16_t co2Concentration = 0;
            float temperature = 0.0;
            float relativeHumidity = 0.0;        

            if (millis() > u32wait)
            {
                error = sensor.getDataReadyStatus(dataReady);
                if (error != NO_ERROR) {
                    Serial.print("Error trying to execute getDataReadyStatus(): ");
                    errorToString(error, errorMessage, sizeof errorMessage);
                    Serial.println(errorMessage); 

                    break;
                }
                else
                {

                }

                if(!dataReady)
                {
                    u32wait = millis() + 200;
                    Serial.print(F(".")); // waiting for data ready
                }    
                else
                {
                    //
                    // If ambient pressure compenstation during measurement
                    // is required, you should call the respective functions here.
                    // Check out the header file for the function definition.
                    error =
                        sensor.readMeasurement(co2Concentration, temperature, relativeHumidity);
                    if (error != NO_ERROR) {
                        Serial.print("Error trying to execute readMeasurement(): ");
                        errorToString(error, errorMessage, sizeof errorMessage);
                        Serial.println(errorMessage);
                    }
                    else {
                        Serial.println();  
                        Serial.println("SCD41 sensor");
                        Serial.print("  CO2 concentration: ");
                        Serial.print(co2Concentration);
                        Serial.println(" ppm");
                        Serial.print("  Temperature: ");
                        Serial.print(temperature);
                        Serial.println(" degrees C");
                        Serial.print("  Relative humidity: ");
                        Serial.print(relativeHumidity);
                        Serial.println(" % rH");
                        
                        u8state = 0;              
                    }
                }     

            }




        break;
    }
	
}

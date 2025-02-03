/*************************************************** 
這個範例用於 SHT40溫溼度感測器 及 SGP41 VOC感測器

SHT40 溫溼度感測器  
購買連結: https://store.makdev.net/products/SHT40

SGP41 VOC感測器
購買連結: https://store.makdev.net/products/SGP41

SHT40感測器可以輔助 SGP41感測器的參數需求，搭配使用可使 VOC感測 更為精準

必需安裝程式庫  
請在 Arduino IDE 中安裝以下程式庫：
Adafruit_SHT4X  
Sensirion I2C SGP41
Sensirion Gas Index Algorithm

This example is for the SHT40 Temperature & Humidity Sensor and the SGP41 VOC Sensor.  

SHT40 Temperature & Humidity Sensor  
Purchase Link: https://store.makdev.net/products/SHT40

SGP41 VOC Sensor  
Purchase Link: https://store.makdev.net/products/SGP41

The SHT40 sensor can assist in providing parameter requirements for the SGP41 sensor. Using them together enhances the accuracy of VOC detection.  

Required Libraries  
Please install the following libraries in Arduino IDE:  
- Adafruit_SHT4X  
- Sensirion I2C SGP41  
- Sensirion Gas Index Algorithm  
 ****************************************************/

#include "Adafruit_SHT4x.h"
#include <SensirionI2CSgp41.h>
#include <NOxGasIndexAlgorithm.h>
#include <VOCGasIndexAlgorithm.h>
#include <Wire.h>

SensirionI2CSgp41 sgp41;
VOCGasIndexAlgorithm voc_algorithm;
NOxGasIndexAlgorithm nox_algorithm;

// time in seconds needed for NOx conditioning
uint16_t conditioning_s = 10;

uint8_t u8state;
unsigned long u32wait;
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

void setup() {
  
  uint16_t error;
  char errorMessage[256];

  Serial.begin(115200);
  Wire.begin();
/* SHT40 begin*/
  Serial.println("SHT4x test");
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

 
  /* SGP41 begin*/
  sgp41.begin(Wire);

  delay(1000);  
  uint8_t serialNumberSize = 3;
  uint16_t serialNumber[serialNumberSize];

  int32_t index_offset;
  int32_t learning_time_offset_hours;
  int32_t learning_time_gain_hours;
  int32_t gating_max_duration_minutes;
  int32_t std_initial;
  int32_t gain_factor;
  voc_algorithm.get_tuning_parameters(
      index_offset, learning_time_offset_hours, learning_time_gain_hours,
      gating_max_duration_minutes, std_initial, gain_factor);

  Serial.println("\nVOC Gas Index Algorithm parameters");
  Serial.print("Index offset:\t");
  Serial.println(index_offset);
  Serial.print("Learning time offset hours:\t");
  Serial.println(learning_time_offset_hours);
  Serial.print("Learning time gain hours:\t");
  Serial.println(learning_time_gain_hours);
  Serial.print("Gating max duration minutes:\t");
  Serial.println(gating_max_duration_minutes);
  Serial.print("Std inital:\t");
  Serial.println(std_initial);
  Serial.print("Gain factor:\t");
  Serial.println(gain_factor);

  nox_algorithm.get_tuning_parameters(
      index_offset, learning_time_offset_hours, learning_time_gain_hours,
      gating_max_duration_minutes, std_initial, gain_factor);

  Serial.println("\nNOx Gas Index Algorithm parameters");
  Serial.print("Index offset:\t");
  Serial.println(index_offset);
  Serial.print("Learning time offset hours:\t");
  Serial.println(learning_time_offset_hours);
  Serial.print("Gating max duration minutes:\t");
  Serial.println(gating_max_duration_minutes);
  Serial.print("Gain factor:\t");
  Serial.println(gain_factor);
  Serial.println("");


  error = sgp41.getSerialNumber(serialNumber);

  if (error) {
      Serial.print("Error trying to execute getSerialNumber(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  } else {
      Serial.print("SerialNumber:");
      Serial.print("0x");
      for (size_t i = 0; i < serialNumberSize; i++) {
          uint16_t value = serialNumber[i];
          Serial.print(value < 4096 ? "0" : "");
          Serial.print(value < 256 ? "0" : "");
          Serial.print(value < 16 ? "0" : "");
          Serial.print(value, HEX);
      }
      Serial.println();
  }

  uint16_t testResult;
  error = sgp41.executeSelfTest(testResult);
  if (error) {
      Serial.print("Error trying to execute executeSelfTest(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  } else if (testResult != 0xD400) {
      Serial.print("executeSelfTest failed with error: ");
      Serial.println(testResult);
  }

  u32wait = millis() + 1000;
  u8state = 0; 

}


void loop() {

  sensors_event_t humidity, temp;

  uint16_t error;
  char errorMessage[256];
  uint16_t defaultRh = 0x8000;
  uint16_t defaultT = 0x6666;
  uint16_t srawVoc = 0;
  uint16_t srawNox = 0;
  uint16_t compensationRh = 0;              // in ticks as defined by SGP41
  uint16_t compensationT = 0;               // in ticks as defined by SGP41

  uint32_t timestamp = millis();
    switch( u8state ) {
        case 0: 
            if (millis() > u32wait) u8state++; // wait state
            break;        
        case 1:
            /* SHT40 */
            if(sht4.getEvent(&humidity, &temp))// populate temp and humidity objects with fresh data
            {
                timestamp = millis() - timestamp;

                Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
                Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

                Serial.print("Read duration (ms): ");
                Serial.println(timestamp);
                Serial.println();
                compensationT = static_cast<uint16_t>((temp.temperature + 45) * 65535 / 175);
                compensationRh = static_cast<uint16_t>(humidity.relative_humidity * 65535 / 100);                
            }
            else
            {
                compensationRh = defaultRh;
                compensationT = defaultT;   
            }
            u8state++;             
            u32wait = millis() + 1000;

            break;

        case 2:
              /* SGP41 */
             if (millis() > u32wait)
             {
                if (conditioning_s > 0) {
                    // During NOx conditioning (10s) SRAW NOx will remain 0
                    error = sgp41.executeConditioning(compensationRh, compensationT, srawVoc);
                    conditioning_s--;
                } else {
                    // Read Measurement
                    error = sgp41.measureRawSignals(compensationRh, compensationT, srawVoc, srawNox);
                }

                if (error) {
                    Serial.print("Error trying to execute measureRawSignals(): ");
                    errorToString(error, errorMessage, 256);
                    Serial.println(errorMessage);
                } else {
                    int32_t voc_index = voc_algorithm.process(srawVoc);
                    int32_t nox_index = nox_algorithm.process(srawNox);
                    Serial.print("VOC Index: ");
                    Serial.print(voc_index);
                    Serial.print("\t");
                    Serial.print("NOx Index: ");
                    Serial.println(nox_index);
                }

                u8state = 0; 
             }    
            
            break;
    }
    


  
}
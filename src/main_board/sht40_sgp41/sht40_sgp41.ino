/*************************************************** 
  This is an example for the SHT4x Humidity & Temp Sensor

  Designed specifically to work with the SHT4x sensor from Adafruit
  ----> https://www.adafruit.com/products/4885

  These sensors use I2C to communicate, 2 pins are required to  
  interface
 ****************************************************/

#include "Adafruit_SHT4x.h"
#include "SparkFun_SGP40_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_SGP40
#include <Wire.h>


uint8_t u8state;
unsigned long u32wait;

SGP40 mySensor; //create an object of the SGP40 class
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

void setup() {
  Serial.begin(115200);

  while (!Serial)
    delay(10);     // will pause Zero, Leonardo, etc until serial console opens

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

  Wire.begin();

  //mySensor.enableDebugging(); // Uncomment this line to print useful debug messages to Serial

  //Initialize sensor
  if (mySensor.begin() == false)
  {
    Serial.println(F("SGP40 not detected. Check connections. Freezing..."));
    while (1)
      ; // Do nothing more
  }

  u32wait = millis() + 1000;
  u8state = 0; 

}


void loop() {
  sensors_event_t humidity, temp;
  uint32_t timestamp = millis();
    switch( u8state ) {
        case 0: 
            if (millis() > u32wait) u8state++; // wait state
            break;        
        case 1:
            
            sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
            timestamp = millis() - timestamp;

            Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
            Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

            Serial.print("Read duration (ms): ");
            Serial.println(timestamp);
            u8state++;
            u32wait = millis() + 1000;
            break;

        case 2:
                
            if (millis() > u32wait) 
            {
                Serial.print(F("VOC Index is: "));
                Serial.println(mySensor.getVOCindex()); //Get the VOC Index using the default RH (50%) and T (25C)    
                Serial.println();            
                u8state = 0; 
            }
            break;
    }
    


  //delay(1000);
}
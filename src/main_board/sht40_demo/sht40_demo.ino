#include <U8g2lib.h>
#include "Adafruit_SHT4x.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
int fontWitdh;
int fontHigh;

Adafruit_SHT4x sht40 = Adafruit_SHT4x();

unsigned long CurrentTime,preTime;
const int intervalSwitch = 1000;

void setup() {
  // put your setup code here, to run once:
    Serial.begin(115200);

    if(!sht40.begin()){
        Serial.println("Could't find SHT40");
        while (1)
        {
            delay(1);
        }
        
    }

    Serial.println("Found SHT40 sensor");
    Serial.print("Serial number [");
    Serial.print(sht40.readSerial(),HEX);
    Serial.println("]");
    delay(1);

    //higher precision takes longer
    sht40.setPrecision(SHT4X_HIGH_PRECISION);

    u8g2.begin();
    
    u8g2.setFont(u8g2_font_8x13_mf);
    u8g2.setFontPosTop();
    fontWitdh = u8g2.getMaxCharWidth();
    fontHigh = u8g2.getMaxCharHeight();

}

void loop() {
  // put your main code here, to run repeatedly:
  sensors_event_t humidity, temp;

    CurrentTime = millis();
   if(CurrentTime - preTime > intervalSwitch){
    preTime = CurrentTime;

    uint32_t timestamp = millis();
    sht40.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    timestamp = millis() - timestamp;

    Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
    Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

    Serial.print("Read duration (ms): ");
    Serial.println(timestamp);

    u8g2.clearBuffer();
    u8g2.setCursor(0,fontHigh);
    u8g2.print("Temp:");
    u8g2.setCursor(0,fontHigh * 2);
    u8g2.print("Humi:");    

    u8g2.setCursor((fontWitdh * 4) + 8,fontHigh);
    u8g2.print(temp.temperature);

    u8g2.setCursor(fontWitdh * 4 + 8,fontHigh * 2);
    u8g2.print(humidity.relative_humidity);

    u8g2.sendBuffer(); 
   }   
}

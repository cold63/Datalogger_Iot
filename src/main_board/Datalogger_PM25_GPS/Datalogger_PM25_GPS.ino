#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#define S0  1
#define S1  0
#define pmsDataLen 32

static const int RXPin = 2, TXPin = 3;
static const uint32_t GPSBaud = 9600;

uint8_t buf[pmsDataLen];
int idx = 0;
int pm10 = 0;
int pm25 = 0;
int pm100 = 0;

uint8_t u8state;
uint32_t u32wait;
// The TinyGPSPlus object
TinyGPSPlus gps;

// The serial connection to the GPS device
SoftwareSerial mySerial(RXPin, TXPin);

void setup()
{
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
    
  Serial.begin(115200);
  mySerial.begin(GPSBaud);

  u32wait = millis() + 1000;
}

void loop()
{
    uint8_t c = 0;

    switch(u8state)
    {
        case 0:
            digitalWrite(S0, LOW);
            digitalWrite(S1, HIGH);     
            if (millis() > u32wait) u8state++; // wait state 
            
        break;

        case 1:
            u32wait = millis() + 5000; 
            u8state++;
        break;


        case 2:
            while (mySerial.available() > 0)
                if (gps.encode(mySerial.read()))
                displayInfo();   

             if (millis() > u32wait)// wait state  
             {
                digitalWrite(S0, LOW);
                digitalWrite(S1, LOW); 
                u32wait = millis() + 5000; 
                Serial.println();  
                u8state++;  
             }      
        break;

        case 3:
                c = 0;
                idx = 0;
                memset(buf, 0, pmsDataLen);

                while (true) {
                    while (c != 0x42) {
                        while (!mySerial.available());
                        c = mySerial.read();
                    }
                    while (!mySerial.available());
                    c = mySerial.read();
                    if (c == 0x4d) {
                        // now we got a correct header)
                        buf[idx++] = 0x42;
                        buf[idx++] = 0x4d;
                        break;
                    }
                }

                while (idx != pmsDataLen) {
                    while(!mySerial.available());
                    buf[idx++] = mySerial.read();
                    
                }

                pm10 = (buf[10] << 8) | buf[11];
                pm25 = (buf[12] << 8) | buf[13];
                pm100 = (buf[14] << 8) | buf[15];
                Serial.print("pm2.5: ");
                Serial.print(pm25);
                Serial.println(" ug/m3");      
                            
                if (millis() > u32wait) {u8state = 0;   Serial.println(); }
                        
        break;



    }


}

void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}
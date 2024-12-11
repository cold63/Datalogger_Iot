#include <WiFi.h>
#include <U8g2lib.h>
#include <ModbusRtu.h>
#include <SoftwareSerial.h>
#include "WS2812B.h"
#include <TimeLib.h>
#include <WiFiUdp.h>

#define S0  1
#define S1  0
#define NUM_OF_LEDS 1

//********** Time ********************************************
char timeServer[] = "time.stdtime.gov.tw";
const int timeZone = 8;

WiFiUDP udp;
unsigned int localPort = 2390;

const int NTP_PACKET_SIZE = 48;
byte packetbuffer[NTP_PACKET_SIZE];
/////Time End

#define pmsDataLen 32
uint8_t buf[pmsDataLen];
int idx = 0;
int pm10 = 0;
int pm25 = 0;
int pm100 = 0;

// data array for modbus network sharing
uint16_t au16data[16];
uint8_t u8state;
uint8_t u8Query;

int status = WL_IDLE_STATUS;

char ssid[] = "your_ssid";
char pass[] = "your_password";

unsigned long ConnectTimeOut;

char server[] ="api.thingspeak.com";
const String api_key = "your_token";

WS2812B led(SPI_MOSI, NUM_OF_LEDS);


WiFiClient client;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
int fontWitdh;
int fontHigh;

SoftwareSerial mySerial(2, 3); // RX, TX

/**
 *  Modbus object declaration
 *  u8id : node id = 0 for master, = 1..247 for slave
 *  port : serial port
 *  u8txenpin : 0 for RS-232 and USB-FTDI 
 *               or any pin number > 1 for RS-485
 */
Modbus master(0,mySerial); // this is master and RS-232 or USB-FTDI via software serial
/**
 * This is an structe which contains a query to an slave device
 */
modbus_t telegram[4];

unsigned long u32wait;

unsigned long CurrentTime,preTime;
const int intervalSwitch = 1000 *22;

unsigned long RelayCurrentTime,RelaypreTime;
const int RelayintervalSwitch = 3000;


unsigned long reTryConnTime,reTryConnPreTime;
const int reTryConnIntervalSwitch = 1000 * 10;
uint16_t temperature,humdidity,lightlux;
uint16_t soil_temperature,soil_humdidity;
bool online = false;
bool r_relay = false;
uint8_t dispCount,relayCount;

/*NTP code*/
void sendNTPpacket()
{
    memset(packetbuffer,0,NTP_PACKET_SIZE);

    packetbuffer[0] = 0b11100011;
    packetbuffer[1] = 0;
    packetbuffer[2] = 6;
    packetbuffer[3] = 0xEC;

    packetbuffer[12] = 49;
    packetbuffer[13] = 0x4E;
    packetbuffer[14] = 49;
    packetbuffer[15] = 52;

    udp.beginPacket(timeServer,123);
    udp.write(packetbuffer,NTP_PACKET_SIZE);
    udp.endPacket();
}

time_t getNtpTime()
{
    Serial.println("Transmit NTP Request");
    udp.setRecvTimeout(1500);
    sendNTPpacket();
    if (udp.read(packetbuffer,NTP_PACKET_SIZE) > 0){
        unsigned long secsSinec1900;
        //convert four bytes starting at location 40 to a long integer
        secsSinec1900 = (unsigned long)packetbuffer[40] << 24;
        secsSinec1900 |= (unsigned long)packetbuffer[41] << 16;
        secsSinec1900 |= (unsigned long)packetbuffer[42] << 8;
        secsSinec1900 |= (unsigned long)packetbuffer[43];
        return secsSinec1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    } else {
        Serial.println("No NTP Response");
        return 0;
    }
}

void showTime()
{
   u8g2.setFont(u8g2_font_ncenB08_tr);
   //u8g2.clearBuffer(); 
   int w = u8g2.getMaxCharWidth();
   u8g2.drawStr(0,0, (String(year())+ "/" + String(month()) + "/" + String(day()) ).c_str());
   u8g2.drawStr(w * 5 ,0, (String(hour())+ ":" + String(minute()) ).c_str());
}


void setColor(int pm25)
{
    if(pm25 < 15)
        led.fill(0, 32, 0, 0, NUM_OF_LEDS);
    else if(pm25 < 35)
        led.fill(32, 32, 0, 0, NUM_OF_LEDS);   
    else if(pm25 < 54)  
        led.fill(0, 32, 32, 0, NUM_OF_LEDS); 
    else if(pm25 < 150) 
        led.fill(32, 0, 0, 0, NUM_OF_LEDS);  
    else if(pm25 < 250) 
        led.fill(32, 0, 32, 0, NUM_OF_LEDS); 
    else 
        led.fill(24, 0, 24, 0, NUM_OF_LEDS);    

}

void reconnectWiFi()
{
    WiFi.begin(ssid,pass);
    ConnectTimeOut = millis();
    
    while(WiFi.status() != WL_CONNECTED){
         Serial.print("Attempting to connect to SSID:");
        Serial.println(ssid);

        u8g2.clearBuffer();
        u8g2.setCursor(0,fontHigh);
        u8g2.print("網路連線中...");
        u8g2.sendBuffer();

        if((millis() - ConnectTimeOut) > 10000)
            break;        

        delay(3000);             
    }

    status = WiFi.status();
    if (status == WL_CONNECTED){

        u8g2.clearBuffer();
        u8g2.setCursor(0,fontHigh);
        u8g2.print("連線成功");
        u8g2.sendBuffer(); 

        udp.begin(localPort);
        Serial.println("waiting for sync");
        setSyncProvider(getNtpTime);          
     
    }
    else
    {
        u8g2.clearBuffer();
        u8g2.setCursor(0,fontHigh);
        u8g2.print("連線失敗");
        u8g2.sendBuffer();        
    }
    
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN,OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); 
    
    mySerial.begin(9600); 
    //溫溼度 MD02
    telegram[0].u8id = 1;
    telegram[0].u8fct = 4;
    telegram[0].u16RegAdd = 1;
    telegram[0].u16CoilsNo = 2;
    telegram[0].au16reg = au16data;   
    //光照
    telegram[1].u8id = 2;
    telegram[1].u8fct = 3;
    telegram[1].u16RegAdd = 3;
    telegram[1].u16CoilsNo = 2;
    telegram[1].au16reg = au16data + 2;     
    //Relay
    telegram[2].u8id = 3;
    telegram[2].u8fct = 5;
    telegram[2].u16RegAdd = 0;
    //telegram[2].u16CoilsNo = 0;
    telegram[2].au16reg = au16data + 4;    
    //telegram[2].au16reg[0] = 1;   
    //au16data[4] = 5;

    telegram[3].u8id = 5;
    telegram[3].u8fct = 3;
    telegram[3].u16RegAdd = 0;
    telegram[3].u16CoilsNo = 2;
    telegram[3].au16reg = au16data + 5;

    master.start(); // start the ModBus object.
    master.setTimeOut( 2000 ); // if there is no answer in 2000 ms, roll over
    
   

    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);

    digitalWrite(S0, LOW);
    digitalWrite(S1, LOW);    

    led.begin();

    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setFont(u8g2_font_unifont_t_chinese);
    u8g2.setFontPosTop();
    fontWitdh = u8g2.getMaxCharWidth();
    fontHigh = u8g2.getMaxCharHeight();

    //u8g2.clear();
    u8g2.clearBuffer();
    u8g2.setCursor(0, fontHigh);
    u8g2.print("初始化...");
    u8g2.sendBuffer();

    reconnectWiFi();
    u8state =u8Query= 0; 

     //Fill the entire LED strip with the same color
    led.fill(24, 24, 24, 0, NUM_OF_LEDS);
    led.show(); 

    u32wait = millis() + 1000;
}

void loop()
{
  //sensors_event_t humidity, temp;
    
    uint8_t c = 0;

    switch( u8state ) 
    {
        case 0: 
            
            digitalWrite(S0, HIGH);
            digitalWrite(S1, LOW);          
            if (millis() > u32wait) u8state++; // wait state           
            break;
        case 1: 
           // if(u8Query == 2)
           //     (telegram[2].au16reg[0] == 1) ? (telegram[2].au16reg[0] = 0):(telegram[2].au16reg[0] = 1);

            //Serial.print("telegram[2].au16reg[0]= ");
            //Serial.println(telegram[2].au16reg[0]);
            //if(u8Query != 0)
            master.query(telegram[u8Query]);
            u8Query++;
            u8state++;

            if(u8Query > 3) u8Query = 0;
            break;
        case 2:       
            master.poll();
           
            if(master.getState() == COM_IDLE){
                u8state++;
                u32wait = millis() + 2000; 
                //if(u8Query == 0)
                {
                    temperature = telegram[0].au16reg[0];
                    humdidity = telegram[0].au16reg[1]; 
                }

                //if(u8Query == 1)
                {
                    lightlux = au16data[3];
                   // lightlux |= telegram[1].au16reg[1];
                }
                //if(u8Query == 3)
                {
                    soil_temperature = telegram[3].au16reg[0];
                    soil_humdidity = telegram[3].au16reg[1];    


                }
              // for(int x = 0; x < 16; x++){
              //      Serial.print(x);
              //      Serial.print(" : ");
              //      Serial.println(au16data[x]);
              //  }

                    u32wait = millis() + 1000;       
            }
           
            break;

        case 3:
            digitalWrite(S0, LOW);
            digitalWrite(S1, LOW);          
            if (millis() > u32wait) u8state++; // wait state           
        break;  

        case 4:
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

                //Serial.print("pm2.5: ");
                //Serial.print(pm25);
                //Serial.println(" ug/m3");    
                setColor(pm25);
                led.show();      
                u8state = 0;              
        break;

        //case 5:
        //    u8state = 0;
        //break;
 
   } 

    RelayCurrentTime = millis();
   if(RelayCurrentTime - RelaypreTime > RelayintervalSwitch){
    RelaypreTime = RelayCurrentTime;
 

            u8g2.clearBuffer();
            showTime();
            u8g2.setFont(u8g2_font_unifont_t_chinese);
            u8g2.setCursor(0,fontHigh);
            if( dispCount != 2){
               u8g2.print("溫度:"); 
               if(dispCount == 0) u8g2.print((float)temperature/10);
               if(dispCount == 1) u8g2.print((float)soil_temperature/10);

            }
            else{

                u8g2.print("光照: "); 
                u8g2.print(lightlux);
                u8g2.print(" lux");
                u8g2.setCursor(0,fontHigh * 2);
                u8g2.print("繼電器: "); 
                (telegram[2].au16reg[0] == 1) ? u8g2.print("ON"):u8g2.print("OFF");

            }

                      
            u8g2.setCursor(fontWitdh * 5,fontHigh);
            if(dispCount == 0)
            {
                digitalWrite(LED_BUILTIN, HIGH); 
                u8g2.print("| 環境");
            }
            else if(dispCount == 1)           
            {
                digitalWrite(LED_BUILTIN, LOW); 
                u8g2.print("| 土壤");
            }
            else if(dispCount == 2)           
            {
                //digitalWrite(LED_BUILTIN, LOW); 
                //u8g2.print("| 光照");
            }            
                



            u8g2.setCursor(0,fontHigh * 2);
            if( dispCount != 2){
                u8g2.print("濕度:");
                if(dispCount == 0) u8g2.print((float)humdidity/10);
                if(dispCount == 1) u8g2.print((float)soil_humdidity/10);
                u8g2.print(" %");
            }

            if(dispCount != 2){
                u8g2.setCursor(fontWitdh * 5,fontHigh * 2);            
                u8g2.print("  檢測");
            }  


            u8g2.setCursor(0,fontHigh * 3);
            u8g2.print("PM2.5: ");    
            //u8g2.setCursor((fontWitdh * 3) ,fontHigh * 3);
            u8g2.print(pm25);
            u8g2.print(" ug/m3");        
            u8g2.sendBuffer();     
            //u8state = 0;    
            online == true ? (online = false) : (online = true);
            //digitalWrite(LED_BUILTIN, LOW); 
        
            dispCount++;
            if(dispCount > 2) dispCount = 0;

            if(relayCount % 8 == 0)
                (telegram[2].au16reg[0] == 1) ? (telegram[2].au16reg[0] = 0):(telegram[2].au16reg[0] = 1);

            relayCount++;
   }

    CurrentTime = millis();
   if(CurrentTime - preTime > intervalSwitch){
    preTime = CurrentTime;
         Serial.println("\nStarting connection to server...");
        if(client.connect(server,80)){

        Serial.println("Connected to server");

        client.println("GET /update?api_key="+ api_key +"&field1=" + String(pm10) + "&field2=" + String(pm25) + "&field3=" + String(pm100) + "&field4=" + String((float)temperature/10)+ + "&field5=" + String((float)humdidity/10) + " HTTP/1.1" );
        client.println("HOST: api.thingspeak.com");
        client.println("Connection: close");
        client.println();
        
        delay(100);

        //  while(client.available())
        //  {
        //    char c = client.read();
            //Serial.println("Write Time: ");
        //    Serial.print(c);
        //  }  
            Serial.println();
            client.stop();
            client.flush();
        }       
   }

    reTryConnTime = millis();
    if(reTryConnTime - reTryConnPreTime > reTryConnIntervalSwitch){
	    reTryConnPreTime = reTryConnTime;
        status = WiFi.status();
        if(status !=WL_CONNECTED )
        {
            reconnectWiFi();
        }

    } 	
}

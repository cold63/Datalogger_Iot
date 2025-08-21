// 引入所需的函式庫
#include <SPI.h>
#include "AmebaILI9341.h" // Ameba 專用的 ILI9341 驅動函式庫
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include <WiFi.h> 
#include <WiFiUdp.h> 
#include <TimeLib.h> 

// macro definitions
// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

// =======================================================================
// 硬體腳位定義
// 根據使用者需求定義 TFT LCD 的連接腳位
// =======================================================================
#define TFT_BL_PIN    9  // TFT LCD 背光開關腳位 (GPIO 9)
#define TFT_RESET_PIN 14 // TFT 重置腳位
#define TFT_DC_PIN    5  // TFT 資料/命令 (D/C) 選擇腳位
#define TFT_CS_PIN    SPI_SS // TFT 片選 (CS) 腳位，使用硬體 SPI 的 SS (Slave Select)

// 初始化 TFT 物件
// 使用上面定義的腳位來建立 AmebaILI9341 的實例
AmebaILI9341 tft = AmebaILI9341(TFT_CS_PIN, TFT_DC_PIN, TFT_RESET_PIN);

SensirionI2cSht4x sensor;

static char errorMessage[64];
static int16_t error;

unsigned long CurrentTime,preTime;
const int intervalSwitch = 1000; // 1 秒

float aTemperature = 0.0;
float aHumidity = 0.0;

char ssid[] = "StudioHQ"; // your network SSID (name)
char pass[] = "0979856319"; // your network password
int status = WL_IDLE_STATUS;
unsigned long ConnectTimeOut;
WiFiClient client;

//********** Time ********************************************
char timeServer[] = "time.stdtime.gov.tw";
const int timeZone = 8;

WiFiUDP udp;                    // UDP實例，此程式用於NTP通訊
unsigned int localPort = 2390;  // 本地UDP端口

const int NTP_PACKET_SIZE = 48;
byte packetbuffer[NTP_PACKET_SIZE];

int prevSecond = -1;
int lastUpdateMinute = -1;  // 用來追蹤上次更新的分鐘
/////Time End

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
     // 發送NTP請求
    sendNTPpacket();
    // wait to see if a reply is available
    delay(1000);
    // 檢查是否收到回應
    if (udp.parsePacket()) {    
        Serial.println("packet received");
        if (udp.read(packetbuffer,NTP_PACKET_SIZE) > 0){
            unsigned long secsSinec1900;
            //convert four bytes starting at location 40 to a long integer
            secsSinec1900 = (unsigned long)packetbuffer[40] << 24;
            secsSinec1900 |= (unsigned long)packetbuffer[41] << 16;
            secsSinec1900 |= (unsigned long)packetbuffer[42] << 8;
            secsSinec1900 |= (unsigned long)packetbuffer[43];
            // 將NTP時間轉換為UNIX時間戳（從1900年到1970年的秒數）
            // 再加上時區偏移            
            return secsSinec1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        } else {
            Serial.println("No NTP Response");
            return 0;
        }
    }

}

void reconnectWiFi()
{
    WiFi.begin(ssid,pass);
    ConnectTimeOut = millis();
    
    
    while(WiFi.status() != WL_CONNECTED){
        Serial.println();
        Serial.print("Attempting to connect to SSID:");
        Serial.println(ssid);


        if((millis() - ConnectTimeOut) > 10000)
            break;        

        delay(3000);             
    }
    status = WiFi.status();
    if (status == WL_CONNECTED){
        udp.begin(localPort);     // 啟動UDP
        Serial.println("waiting for sync"); 
        // 設置時間同步提供者為NTP獲取的時間
        setSyncProvider(getNtpTime);         
        delay(10);  
            
    }

}

// =======================================================================
// setup() 函式 - 在程式開始時僅執行一次的初始化程式碼
// =======================================================================
void setup() {
    Serial.begin(115200);
    // 設定背光腳位為輸出模式
    pinMode(TFT_BL_PIN, OUTPUT);
    // 開啟背光 (HIGH 為開啟)
    digitalWrite(TFT_BL_PIN, HIGH);

    Wire.begin();
    sensor.begin(Wire, SHT40_I2C_ADDR_44);

    sensor.softReset();
    delay(10);
    uint32_t serialNumber = 0;
    error = sensor.serialNumber(serialNumber);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute serialNumber(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("serialNumber: ");
    Serial.print(serialNumber);
    Serial.println();

    // 初始化 TFT LCD
    tft.begin();

    // 設定螢幕為橫向顯示 (320x240)
    // 0: 直向, 1: 橫向, 2: 直向反轉, 3: 橫向反轉
    tft.setRotation(1);

    // 用黑色填滿整個螢幕，清除舊畫面
    tft.fillScreen(ILI9341_BLACK);

    // 繪製靜態的 UI 介面框架與標題
    drawLayout();

    reconnectWiFi();

    GetSensorData();
     // 更新溫溼度數據
    updateDisplayData();     
}

// =======================================================================
// loop() 函式 - setup() 後會重複執行的主要程式碼
// =======================================================================
void loop() {

   
    // 每秒更新一次
  if(second() != prevSecond){ 
    prevSecond = second();
 
    // --- 更新顯示 ---
    char dataBuffer[20]; // 用於格式化字串的緩衝區

    // --- 顯示日期 ---
    tft.setFontSize(2);
    tft.setForeground(ILI9341_WHITE);
    // 先用黑色矩形清除舊的日期數據，再顯示新的
    tft.fillRectangle(10, 35, 140, 15, ILI9341_BLACK);
    tft.setCursor(25, 35);
    sprintf(dataBuffer, "%04d-%02d-%02d", year(), month(), day());
    tft.print(dataBuffer);

    // --- 顯示時間 ---
    // 先用黑色矩形清除舊的時間數據，再顯示新的
    tft.fillRectangle(170, 35, 140, 15, ILI9341_BLACK);
    tft.setCursor(185, 35);
    sprintf(dataBuffer, "%02d:%02d:%02d", hour(), minute(), second());
    tft.print(dataBuffer);
  
  }

    int currentMinute = minute();
    // 每 5 分鐘更新一次溫濕度數據
    if(currentMinute % 5 == 0 && currentMinute != lastUpdateMinute && second() == 0) {
        lastUpdateMinute = currentMinute;

        GetSensorData();
        // 更新溫溼度數據
        updateDisplayData();     
  }


}


void GetSensorData()
{
    error = sensor.measureLowestPrecision(aTemperature, aHumidity);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute measureLowestPrecision(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("aTemperature: ");
    Serial.print(aTemperature);
    Serial.print("\t");
    Serial.print("aHumidity: ");
    Serial.print(aHumidity);
    Serial.println();  
   
}
// =======================================================================
// drawLayout() - 繪製畫面的 UI 框架與靜態標題
// =======================================================================
void drawLayout() {
    // 螢幕尺寸為 320x240
    // 繪製日期和時間的框
    tft.drawRectangle(5, 5, 150, 50, ILI9341_WHITE); // 日期框
    tft.drawRectangle(165, 5, 150, 50, ILI9341_WHITE); // 時間框

    // 繪製溫度和濕度的框 (對稱且佔比較大)
    tft.drawRectangle(5, 60, 150, 175, ILI9341_WHITE); // 溫度框
    tft.drawRectangle(165, 60, 150, 175, ILI9341_WHITE);   // 濕度框

    // --- 繪製靜態標題 ---
    tft.setFontSize(2);
    tft.setBackground(ILI9341_BLACK); // 背景設為透明(與螢幕底色相同)

    // 日期標題
    tft.setForeground(ILI9341_YELLOW);
    tft.setCursor(45, 15);
    tft.print("DATE");

    // 時間標題
    tft.setForeground(ILI9341_YELLOW);
    tft.setCursor(205, 15);
    tft.print("TIME");

    // 溫度標題
    tft.setForeground(ILI9341_ORANGE);
    tft.setCursor(35, 70);
    tft.print("Temp.(C)");

    // 濕度標題
    tft.setForeground(ILI9341_CYAN);
    tft.setCursor(205, 70);
    tft.print("Humi.(%)");
}

// =======================================================================
// updateDisplayData() - 更新並顯示動態數據
// =======================================================================
void updateDisplayData() {

    // --- 更新顯示 ---
    char dataBuffer[20]; // 用於格式化字串的緩衝區

    // --- 顯示溫度 ---
    tft.setFontSize(5); // 溫濕度使用較大的字體
    // 先用黑色矩形清除舊的溫度數據，再顯示新的
    tft.fillRectangle(10, 120, 140, 80, ILI9341_BLACK);
    tft.setForeground(ILI9341_RED);
    tft.setCursor(25, 140);
    // 格式化溫度數據，保留一位小數
    dtostrf(aTemperature, 4, 1, dataBuffer);
    tft.print(dataBuffer);
	
    // --- 顯示濕度 ---
    tft.setFontSize(5); // 溫濕度使用較大的字體
    // 先用黑色矩形清除舊的濕度數據，再顯示新的
    tft.fillRectangle(170, 120, 140, 80, ILI9341_BLACK);
    tft.setForeground(ILI9341_BLUE);
    tft.setCursor(185, 140);
    // 格式化濕度數據，保留一位小數
    dtostrf(aHumidity, 4, 1, dataBuffer);
    tft.print(dataBuffer);

}
/*
執行這個範例前先安裝以下連結的程式庫
	Modbus-Master-Slave-for-Arduino
	https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino/tree/master
	
	SDCard
    https://github.com/cold63/Datalogger_Iot/blob/main/libraries/SdFat-master.zip

    範例說明:
	一次連接 1 個 RS-485 溫溼度感測器 (型號:RS485-MD-02)，Modbus Address 定義為 0x01
	讀取溫度及濕度數值並顯示在序列埠監控視窗上
    溫度及濕度各佔用 1 個 16-bit 的暫存器
    溫度暫存器位址: 0x0001
    濕度暫存器位址: 0x0002
    溫度及濕度數值需除以 10 才是正確的數值

    原 MD-02 可以一次完整讀取溫度及濕度兩個暫存器
    但為了示範模擬多個不同裝置及 query 的使用方式
    所以分別對同一個裝置發出兩個 query 來讀取溫度及濕度
*/

#include <ModbusRtu.h>
#include <SoftwareSerial.h>
#include "SPI.h" 
#include "SdFat.h"

#define S0  1
#define S1  0

#define USE_PMS5003() do{ digitalWrite(S0, LOW); digitalWrite(S1, LOW); } while(0) // PMS5003
#define USE_RS485() do{ digitalWrite(S0, HIGH); digitalWrite(S1, LOW); } while(0)  // RS-485
#define USE_GPS() do{ digitalWrite(S0, LOW); digitalWrite(S1, HIGH); } while(0)    // GPS
#define USE_EXTERNAL_UART() do{ digitalWrite(S0, HIGH); digitalWrite(S1, HIGH); } while(0) // EXTERNAL

bool hasSD = TRUE;
unsigned long SdCardu32wait;
 //SD CARD /***********BEGIN**********************/
#if SPI_DRIVER_SELECT == 2  // Must be set in SdFat/SdFatConfig.h
// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 0
//
// Chip select may be constant or RAM variable.
const uint8_t SD_CS_PIN = 18;
//
// Pin numbers in templates must be constants.
const uint8_t SOFT_MISO_PIN = 16;
const uint8_t SOFT_MOSI_PIN = 17;
const uint8_t SOFT_SCK_PIN = 19;

// SdFat software SPI template
SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> softSpi;
// Speed argument is ignored for software SPI.
#if ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(0), &softSpi)
#else  // ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(0), &softSpi)
#endif  // ENABLE_DEDICATED_SPI

#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE
//SD CARD /***********END**********************/
//SD CARD /***********BEGIN**********************/
#else  // SPI_DRIVER_SELECT
#error SPI_DRIVER_SELECT must be two in SdFat/SdFatConfig.h
#endif  // SPI_DRIVER_SELECT
//SD CARD /***********END**********************/

// data array for modbus network sharing
uint16_t au16data[16];
uint8_t u8state;
uint8_t u8Query;

SoftwareSerial mySerial(2, 3); // RX, TX

/**
 *  Modbus object declaration
 *  u8id : node id = 0 for master, = 1..247 for slave
 *  port : serial port
 *  u8txenpin : 0 for RS-232 and USB-FTDI 
 *               or any pin number > 1 for RS-485
 */
Modbus master(0, mySerial); // this is master and RS-232 or USB-FTDI via software serial

/**
 * This is an structe which contains a query to an slave device
 */
modbus_t telegram[2];

unsigned long u32wait;

uint16_t temperature,humdidity;

void setup() {
  Serial.begin(115200);  
  mySerial.begin(9600);//use the hardware serial if you want to connect to your computer via usb cable, etc.

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);

  USE_RS485(); 
   
   if (!sd.begin(SD_CONFIG)) {
    //sd.initErrorHalt();
    Serial.println("Initize SD Card error");
    hasSD = FALSE;
   }

   u32wait = millis() + 1000;
   SdCardu32wait = millis() + 1000 * 60; //每分鐘寫一次SD卡


  master.start(); // start the ModBus object.
  master.setTimeOut( 2000 ); // if there is no answer in 2000 ms, roll over
  u32wait = millis() + 1000;
  u8state = 0; 
}

void loop() {
  switch( u8state ) {
  case 0: 
    if (millis() > u32wait) u8state++; // wait state
    break;
  case 1: 
    // 同一個溫溼度感測器,只讀溫度
    telegram[0].u8id = 1; // slave address
    telegram[0].u8fct = 4; // function code (this one is registers read)
    telegram[0].u16RegAdd = 1; // start address in slave
    telegram[0].u16CoilsNo = 1; // number of elements (coils or registers) to read
    telegram[0].au16reg = au16data; // pointer to a memory array in the Arduino
    // 同一個溫溼度感測器,只讀濕度
    telegram[1].u8id = 1; // slave address
    telegram[1].u8fct = 4; // function code (this one is registers read)
    telegram[1].u16RegAdd = 2; // start address in slave
    telegram[1].u16CoilsNo = 1; // number of elements (coils or registers) to read
    telegram[1].au16reg = au16data + 1; // pointer to a memory array in the Arduino

    master.query( telegram[u8Query] ); // send query (only once)
    u8state++;
    u8Query++;

    if(u8Query > 1) u8Query = 0;

    break;
  case 2:
    master.poll(); // check incoming messages
    if (master.getState() == COM_IDLE) {
      u8state++;
      u32wait = millis() + 2000; 
        //讀取完成,將溫度及濕度數值存到變數中
        temperature = au16data[0];
        humdidity = au16data[1];

         //將 au16data 陣列內容輸出到序列埠監控視窗
         /*
          Serial.println("Modbus RTU Master - RS485 Mode");
          Serial.println("讀取 MD-02 溫溼度感測器資料");
          Serial.println("------------------------------");
         for(int x = 0; x < 16; x++){
              Serial.print(x);
              Serial.print(" : ");
              Serial.println(au16data[x]);
          }
        */
                  

          Serial.print("溫度: ");
          Serial.println((float)temperature/10);
          Serial.print("濕度: ");
          Serial.println((float)humdidity/10);
          Serial.println();
    }
    break;

    case 3:
      if(hasSD && (millis() > SdCardu32wait)) {
        SdCardu32wait = millis() + 1000 * 60; //每分鐘寫一次SD卡
        //將溫度及濕度數值寫入SD卡
        if (file.open("datalog.txt", O_APPEND| O_RDWR | O_CREAT)) {
          file.print("溫度: ");
          file.print((float)temperature/10);
          file.print(" 濕度: ");
          file.print((float)humdidity/10);
          file.println();
          file.close();
          Serial.println("已寫入SD卡");
        }
      }
      u8state = 0;
      break;
  }
}
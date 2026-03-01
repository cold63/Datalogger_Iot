/*
執行這個範例前先安裝以下連結的程式庫
	Modbus-Master-Slave-for-Arduino
	https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino/tree/master

	

Before running this example, please install the following library from the link below:
- Modbus-Master-Slave-for-Arduino
- https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino/tree/master
	
*/

#include <ModbusRtu.h>
#include <SoftwareSerial.h>

#define S0  1
#define S1  0

// 感測器倍率定義
#define WIND_SPEED_SCALE 100.0   // 風速倍率（感測器回傳值 = 實際值 × 100）
#define WIND_DIR_SCALE 100.0     // 風向倍率（感測器回傳值 = 實際值 × 100）

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
modbus_t telegram;

unsigned long u32wait;

uint16_t windspeed, winddirection;

/**
 * 將風向角度轉換為16方位名稱
 * @param degree 風向角度 (0-360)
 * @return 方位名稱字串
 */
String getWindDirection(float degree) {
  const char* directions[] = {
    "北", "北東北", "東北", "東東北",
    "東", "東東南", "東南", "南東南",
    "南", "南西南", "西南", "西西南",
    "西", "西西北", "西北", "北西北"
  };
  
  int index = (int)((degree + 11.25) / 22.5) % 16;
  return String(directions[index]);
}

void setup() {
  Serial.begin(115200);  
  mySerial.begin(9600);//use the hardware serial if you want to connect to your computer via usb cable, etc.

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);   
   

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
    telegram.u8id = 1; // slave address
    telegram.u8fct = 3; // function code (this one is registers read)
    telegram.u16RegAdd = 0; // start address in slave
    telegram.u16CoilsNo = 2; // number of elements (coils or registers) to read
    telegram.au16reg = au16data; // pointer to a memory array in the Arduino

    master.query( telegram ); // send query (only once)
    u8state++;

    break;
  case 2:
    master.poll(); // check incoming messages
    if (master.getState() == COM_IDLE) {
      u8state = 0;
      u32wait = millis() + 2000; 
        
        windspeed = au16data[0];
        winddirection = au16data[1];
        
        // 計算實際風速和風向
        float windspeed_ms = windspeed / WIND_SPEED_SCALE;      // m/s
        float winddirection_deg = winddirection / WIND_DIR_SCALE; // 度
        String wind_dir_name = getWindDirection(winddirection_deg);
        
        // 顯示原始資料（除錯用）
        for(int x = 0; x < 16; x++){
          Serial.print(x);
          Serial.print(" : ");
          Serial.println(au16data[x]);
        }
        Serial.println("------------------------");
        
        // 顯示風速風向資訊
        Serial.print("風速: ");
        Serial.print(windspeed_ms, 2);  // 顯示2位小數
        Serial.println(" m/s");
        
        Serial.print("風向: ");
        Serial.print(winddirection_deg, 1);  // 顯示1位小數
        Serial.print("° (");
        Serial.print(wind_dir_name);
        Serial.println(")");
        Serial.println();
    }
    break;
  }
}
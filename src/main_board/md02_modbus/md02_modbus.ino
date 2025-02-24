/*
執行這個範例前先安裝以下連結的程式庫
	Modbus-Master-Slave-for-Arduino
	https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino/tree/master
	
	範例說明:
	一次連接 1 個 RS-485 溫溼度感測器 (型號:RS485-MD-02)，Modbus Address 定義為 0x01
	

Before running this example, please install the following library from the link below:
- Modbus-Master-Slave-for-Arduino
- https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino/tree/master

Example Description:
- This example connects a single RS-485 temperature and humidity sensor (model: RS485-MD-02) with the Modbus Address defined as 0x01.
	
*/

#include <ModbusRtu.h>
#include <SoftwareSerial.h>

#define S0  1
#define S1  0

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
    telegram[1].au16reg = au16data + 2; // pointer to a memory array in the Arduino

    master.query( telegram[u8Query] ); // send query (only once)
    u8state++;
    u8Query++;

    if(u8Query > 1) u8Query = 0;

    break;
  case 2:
    master.poll(); // check incoming messages
    if (master.getState() == COM_IDLE) {
      u8state = 0;
      u32wait = millis() + 2000; 
        
        temperature = au16data[0];
        humdidity = au16data[2];

        
         for(int x = 0; x < 16; x++){
              Serial.print(x);
              Serial.print(" : ");
              Serial.println(au16data[x]);
          }
                  

          Serial.print("溫度: ");
          Serial.println((float)temperature/10);
          Serial.print("濕度: ");
          Serial.println((float)humdidity/10);
          Serial.println();
    }
    break;
  }
}
/*
執行這個範例前先安裝以下連結的程式庫
	Modbus-Master-Slave-for-Arduino
	https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino/tree/master
	
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
#include <WiFi.h>
#include <PubSubClient.h>

#define S0  1
#define S1  0

#define USE_PMS5003() do{ digitalWrite(S0, LOW); digitalWrite(S1, LOW); } while(0) // PMS5003
#define USE_RS485() do{ digitalWrite(S0, HIGH); digitalWrite(S1, LOW); } while(0)  // RS-485
#define USE_GPS() do{ digitalWrite(S0, LOW); digitalWrite(S1, HIGH); } while(0)    // GPS
#define USE_EXTERNAL_UART() do{ digitalWrite(S0, HIGH); digitalWrite(S1, HIGH); } while(0) // EXTERNAL

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
unsigned long lastMQTTSendTime = 0;  // MQTT 發送計時器
const unsigned long MQTT_SEND_INTERVAL = 3000;  // MQTT 發送間隔 (3秒)

uint16_t temperature,humdidity;

char ssid[] = "your_ssid";     // your network SSID (name)
char pass[] = "your_password";  // your network password
int status  = WL_IDLE_STATUS;    // the Wifi radio's status
long lastTime = 0;

WiFiClient wifiClient;
unsigned long ConnectTimeOut;  // WiFi 連線逾時計時器
PubSubClient mqttClient(wifiClient);

char username[] = "";  // device access token(存取權杖)
char password[] = "";                      // no need password
char mqttServer[]     = "broker.hivemq.com";
char clientId[20];
char publishTopic[]   = "md02Service/v1";   
//char subscribeTopic[] = "md02/su1";


// --- WiFi 連線 ---
void reconnectWiFi()
{
    
    ConnectTimeOut = millis();  // 記錄開始連線時間
    
    while(WiFi.status() != WL_CONNECTED){
        Serial.println();
        Serial.print("Attempting to connect to SSID:");
        Serial.println(ssid);
        WiFi.disconnect(); // 先主動斷線，消除殘留 session

        // 連線逾時檢查 (10 秒)
        if((millis() - ConnectTimeOut) > 10000)
            break;        

        WiFi.begin(ssid, pass);  // 嘗試連線
        delay(3000);             // 等待連線結果
        Serial.println(); 
    }
    
}

// --- MQTT 連線 ---
boolean reconnectMQTT() {
    if (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(clientId, username, password)) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again later");
        }
    }
    return mqttClient.connected();
}

void reconnect()
{

    // Loop until we're reconnected
    while (!(mqttClient.connected())) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (mqttClient.connect(clientId, username, password)) {
            Serial.println("connected");
            //mqttClient.subscribe(subscribeTopic);
            //mqttClient.subscribe(subscribeTopicRPC);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
  


}

void sendMQTT(char *val)
{
 if (!mqttClient.connected()) {
      Serial.println("Attempting MQTT connection");
      mqttClient.connect(clientId, username, password);
  }

    if (mqttClient.connected()) {
        mqttClient.publish(publishTopic, val);
    } 

}

void setup() {
  Serial.begin(115200);  
  mySerial.begin(9600);//use the hardware serial if you want to connect to your computer via usb cable, etc.

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);

  USE_RS485();

  /* 設定查詢 0: 讀取溫度暫存器 ------------------------------------------
   * Modbus 功能碼 4 (0x04): 讀取輸入暫存器 (Read Input Registers)
   * 從站位址: 1
   * 起始位址: 1 (溫度暫存器)
   * 讀取數量: 1 個暫存器
   */
  telegram[0].u8id = 1;           // 從站位址
  telegram[0].u8fct = 4;          // 功能碼: 讀取輸入暫存器
  telegram[0].u16RegAdd = 1;      // 暫存器起始位址 (溫度)
  telegram[0].u16CoilsNo = 1;     // 讀取暫存器數量
  telegram[0].au16reg = au16data; // 資料存放指標 (存入 au16data[0])

  /* 設定查詢 1: 讀取濕度暫存器 ------------------------------------------
   * 從站位址: 1
   * 起始位址: 2 (濕度暫存器)
   * 讀取數量: 1 個暫存器
   */
  telegram[1].u8id = 1;               // 從站位址
  telegram[1].u8fct = 4;              // 功能碼: 讀取輸入暫存器
  telegram[1].u16RegAdd = 2;          // 暫存器起始位址 (濕度)
  telegram[1].u16CoilsNo = 1;         // 讀取暫存器數量
  telegram[1].au16reg = au16data + 1; // 資料存放指標 (存入 au16data[1])

  reconnectWiFi();
  randomSeed(millis());
  sprintf(clientId, "d%lu", random(1000, 30000));
  Serial.print("Client ID: ");
  Serial.println(clientId);
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(10);
  
  master.start(); // start the ModBus object.
  master.setTimeOut( 2000 ); // if there is no answer in 2000 ms, roll over
  u32wait = millis() + 1000;
  u8state = 0; 
}

void loop() {

  if (!(mqttClient.connected())) {
    reconnect();
  }

  switch( u8state ) {
  case 0: 
    if (millis() > u32wait) u8state++; // wait state
    break;
  case 1: 


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
        Serial.println((float)temperature / 10);
        Serial.print("濕度: ");
        Serial.println((float)humdidity / 10);
        Serial.println();
    }
    break;
  }

  

  // 每 3 秒發送一次 MQTT
  if (millis() - lastMQTTSendTime >= MQTT_SEND_INTERVAL) {
    lastMQTTSendTime = millis();
    
    String JsnonString = "{\"temperature\":";
    JsnonString += (float)temperature / 10;
    JsnonString += ",\"humdidity\":";
    JsnonString += (float)humdidity / 10;
    JsnonString += "}";
    sendMQTT((char *)JsnonString.c_str());
  }

  mqttClient.loop();
}
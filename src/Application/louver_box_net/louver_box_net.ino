
#include <ModbusRtu.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <PubSubClient.h>

#define S0  1
#define S1  0

// data array for modbus network sharing
uint16_t au16data[16];
uint8_t u8state;
uint8_t u8Query;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
long lastReconnectAttempt = 0;

char ssid[] = "your_ssid";     // your network SSID (name)
char pass[] = "your_password";  // your network password
int status  = WL_IDLE_STATUS;    // the Wifi radio's status

char username[] = "";  // device access token(存取權杖)
char password[] = "";                      // no need password

char mqttServer[]     = "broker.emqx.io";
char clientId[32]; 
char publishTopic[]   = "louverout1/v1";   
char subscribeTopic[] = "louverin1/sub1";
bool successSubscribe = false; // 是否訂閱成功

unsigned long CurrentTime,preTime;
const int intervalSwitch = 1000 *20; // 20 秒

unsigned long reTryConnTime,reTryConnPreTime;    
const int reTryConnIntervalSwitch = 1000 * 10; // 10 秒
unsigned long ConnectTimeOut;

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
modbus_t telegram[3];

unsigned long u32wait;

uint16_t temperature,humdidity;
uint32_t light;

boolean reconnect() {
    
    if (mqttClient.connect(clientId, username, password)) {
        Serial.println("connected");
        successSubscribe = mqttClient.subscribe(subscribeTopic);
    }
    return mqttClient.connected();
}

void sendMQTT(const char *val)
{
 if (!mqttClient.connected()) {
      Serial.println("\nAttempting MQTT connection");
      mqttClient.connect(clientId, username, password);
  }

    if (mqttClient.connected()) {
        mqttClient.publish(publishTopic, val);
    } 

}

void reconnectWiFi()
{
    WiFi.begin(ssid,pass);
    ConnectTimeOut = millis();
    
    
    while(WiFi.status() != WL_CONNECTED){
        Serial.print("Attempting to connect to SSID:");
        Serial.println(ssid);


        if((millis() - ConnectTimeOut) > 10000)
            break;        

        delay(3000);             
    }
    
}

void setup() {
  Serial.begin(115200);  
  mySerial.begin(9600);//use the hardware serial if you want to connect to your computer via usb cable, etc.

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);   

  reconnectWiFi();
   // wait for WiFi connection   
  Serial.print("\nConnected to WiFi");
  for(int i = 0; i < 5; i++) {
      delay(1000);
      Serial.print(".");
  }
  Serial.println();
  randomSeed(millis()); // 使用當前時間作為亂數種子
  sprintf(clientId, "d%lu", random(1000, 30000));  // 生成隨機的客戶端 ID
  Serial.print("Client ID: ");
  Serial.println(clientId);
  mqttClient.setServer(mqttServer, 1883); // 設定 MQTT 伺服器和埠號  
  mqttClient.setKeepAlive(360); // 設定保持連線時間為 360 秒
  mqttClient.setSocketTimeout(10); // 設定 Socket 超時時間為 10 秒
  delay(5000);

  master.start(); // 啟動 Modbus 
  master.setTimeOut( 2000 ); // 設定 Modbus 超時時間為 2000 毫秒
  u32wait = millis() + 1000; // 初始化等待時間
  u8state =u8Query= 0; 
}

void loop() {

    if (!(mqttClient.connected()) ) { // 如果 MQTT 客戶端未連線
        successSubscribe = false;  // 如果距離上次嘗試重新連線超過 5 秒
        long now = millis();
        if ((now - lastReconnectAttempt) > 5000) {
            lastReconnectAttempt = now; // 更新上次嘗試重新連線的時間
            // Attempt to reconnect
            if (reconnect()) {
                lastReconnectAttempt = 0; // 重新連線成功，重設上次嘗試時間
            }
        }
    } else {
        // Client connected
        mqttClient.loop(); // 讓 MQTT 客戶端處理訊息收發
        if(!successSubscribe) {
            successSubscribe = mqttClient.subscribe(subscribeTopic);
            if(successSubscribe) {
                Serial.println("Subscribed to topic: " + String(subscribeTopic));
            } else {
                Serial.println("Failed to subscribe to topic: " + String(subscribeTopic));
            }
        }
    }

  switch( u8state ) {
  case 0: 
    if ((long)(millis() - u32wait) >= 0) u8state++; // wait state
    break;
  case 1: 
    telegram[0].u8id = 1; // 從站位址 (ID 1)
    telegram[0].u8fct = 3; // 功能碼 (讀取保持暫存器)
    telegram[0].u16RegAdd = 0; // 暫存器起始位址 0
    telegram[0].u16CoilsNo = 1; // 讀取 1 個暫存器 (溫度)
    telegram[0].au16reg = au16data; // 指向儲存資料的陣列

    telegram[1].u8id = 1; // 從站位址 (ID 1)
    telegram[1].u8fct = 3; // 功能碼 (讀取保持暫存器)
    telegram[1].u16RegAdd = 1; // 暫存器起始位址 1
    telegram[1].u16CoilsNo = 1; // 讀取 1 個暫存器 (濕度)
    telegram[1].au16reg = au16data + 1; // 指向儲存資料的陣列   

    telegram[2].u8id = 1; // 從站位址 (ID 1)
    telegram[2].u8fct = 3; // 功能碼 (讀取保持暫存器)
    telegram[2].u16RegAdd = 3; // 暫存器起始位址 3
    telegram[2].u16CoilsNo = 2; // 讀取 2 個暫存器 (光照，可能是 32 位元值)
    telegram[2].au16reg = au16data + 2; // 指向儲存資料的陣列     

    master.query( telegram[u8Query] ); // 發送 Modbus 查詢
    u8state++;
    u8Query++;

    if(u8Query > 2) u8Query = 0;
    break;
  case 2:
    master.poll(); // check incoming messages
    if (master.getState() == COM_IDLE) {
      u8state = 0;
      u32wait = millis() + 2000; 
        
        temperature = au16data[0];
        humdidity = au16data[1];
        light = ((uint32_t)au16data[4] << 16) | (uint32_t)au16data[3];

        //Debug 
        // for(int x = 0; x < 16; x++){
        //      Serial.print(x);
        //      Serial.print(" : ");
        //      Serial.println(au16data[x]);
        //  }
        //Debug --------
                 
         Serial.println();
         Serial.print("溫度: ");
         Serial.println((float)temperature/10);
         Serial.print("濕度: ");
         Serial.println((float)humdidity/10);
         Serial.print("光照: ");
         Serial.println(light);
         Serial.println();

    }
    break;
  }

    CurrentTime = millis();
    if(CurrentTime - preTime > intervalSwitch){
         preTime = CurrentTime;
         String payload = "{";
          payload += "\"temperature\":" + String((float)temperature/10) + ",";
          payload += "\"humdidity\":" + String((float)humdidity/10) + ",";
          payload += "\"light\":" + String(light);
          payload += "}";
          Serial.println("Sending MQTT payload: " + payload);
          sendMQTT(payload.c_str());
    }
  // 定時檢查 Wi-Fi 連線並重新連線
  reTryConnTime = millis();
  if(reTryConnTime - reTryConnPreTime > reTryConnIntervalSwitch){
      reTryConnPreTime = reTryConnTime;
      status = WiFi.status();
      if(status !=WL_CONNECTED )
      {
          reconnectWiFi();// 重新連線到 Wi-Fi
      }

  }

}
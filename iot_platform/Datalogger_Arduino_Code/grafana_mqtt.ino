
#include <ModbusRtu.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define USE_ERROR_TEMPLATE  


unsigned long lastMQTTSendTime = 0;  // MQTT 發送計時器
const unsigned long MQTT_SEND_INTERVAL = 5000;  // MQTT 發送間隔 (5秒)

uint16_t temperature,humidity;

char ssid[] = "your_ssid";     // your network SSID (name)
char pass[] = "your_password";  // your network password
int status  = WL_IDLE_STATUS;    // the Wifi radio's status
long lastTime = 0;

WiFiClient wifiClient;
unsigned long ConnectTimeOut;  // WiFi 連線逾時計時器
PubSubClient mqttClient(wifiClient);

char username[] = "your_account";  // 填入IoT平台帳號
char password[] = "your_password";// 填入IoT平台密碼
char mqttServer[]     = "broker.makdev.net";
char clientId[20];
char publishTopic[]   = "<your_account>/room1/env";   //修改帳號
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




  reconnectWiFi();
  randomSeed(millis());
  sprintf(clientId, "d%lu", random(1000, 30000));
  Serial.print("Client ID: ");
  Serial.println(clientId);
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(10);

}

void loop() {

  if (!(mqttClient.connected())) {
    reconnect();
  }


  // 每 5 秒發送一次 MQTT
  if (millis() - lastMQTTSendTime >= MQTT_SEND_INTERVAL) {
    lastMQTTSendTime = millis();
 
    temperature = 230 + (random(0, 16) ); // 36.5 + 0.0 ~ 1.5 
    humidity = 600 + (random(0, 16) ); // 60 + 0.0 ~ 1.5 
#ifdef USE_ERROR_TEMPLATE
// ==========================================
// 模擬異常高溫情況，持續 10 分鐘，每 5 分鐘有 5 分鐘的異常高溫
    // ==========================================
    if ((millis() % 600000) > 300000) {
      // 隨機產生 36.5°C ~ 38.0°C 的異常高溫，讓 Grafana 曲線看起來更真實
      temperature = 365 + (random(0, 16) ); // 36.5 + 0.0 ~ 1.5 
      Serial.println("[模擬測試] 目前處於 5 分鐘高溫異常期！");
    }
#endif

    String JsnonString = "{\"temperature\":";
    JsnonString += (float)temperature / 10;
    JsnonString += ",\"humidity\":";
    JsnonString += (float)humidity / 10;
    JsnonString += "}";
    Serial.print("MQTT Publish: ");
    Serial.println(JsnonString);
    sendMQTT((char *)JsnonString.c_str());
  }

  mqttClient.loop();

}
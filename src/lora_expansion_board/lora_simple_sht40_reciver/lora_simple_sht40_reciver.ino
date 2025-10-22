#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>

// LoRa 引腳定義
#define LORA_NSS 10
#define LORA_RST 9
#define LORA_DIO0 5

// WiFi 參數
char ssid[] = "your_ssid";
char pass[] = "your_password";
int status = WL_IDLE_STATUS;
unsigned long ConnectTimeOut;

// MQTT 參數
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

char username[] = "";
char password[] = "";
char mqttServer[] = "broker.emqx.io";
char clientId[20] = "";
char publishTopic[] = "lora_simple/sht40";
char subscribeTopic[] = "host/command";

// MQTT 傳送間隔控制
unsigned long lastMqttSendTime = 0;
const unsigned long MQTT_SEND_INTERVAL = 10000; // 10 秒（減少 WiFi/MQTT 活動）
String latestJsonData = ""; // 儲存最新的感測器資料


// 感測器資料結構（使用 packed 避免記憶體對齊問題）
typedef struct {
    uint32_t device_id;      // 裝置唯一 ID (4 bytes)
    float temperature;       // 溫度 (4 bytes)
    float humidity;          // 濕度 (4 bytes)
    uint16_t checksum;       // CRC-16 校驗碼 (2 bytes)
} __attribute__((packed)) SensorData;  // 總共 14 bytes


unsigned long reTryConnTime,reTryConnPreTime;    
const int reTryConnIntervalSwitch = 1000 * 10; // 10 秒

void reconnectWiFi()
{
    WiFi.begin(ssid, pass);
    ConnectTimeOut = millis();
    
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println();
        Serial.print("Attempting to connect to SSID:");
        Serial.println(ssid);

        // 使用時間差計算避免 millis() overflow 問題
        if ((millis() - ConnectTimeOut) > 10000)
            break;        

        delay(3000);             
    }
}

// 計算 CRC-16 校驗碼（CRC-16-CCITT）
uint16_t calculate_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

void reconnect() {
    
    // Loop until we're reconnected
    while (!(mqttClient.connected())) {
        Serial.print("\r\nAttempting MQTT connection...");
        // Attempt to connect
        if (mqttClient.connect(clientId)) {
            Serial.println("connected");

        } else {
            Serial.println("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 3 seconds");
            //Wait 3 seconds before retrying
            delay(3000);
        }
    }
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

// 驗證感測器資料的有效性
bool validateSensorData(const SensorData* data) {
  // 檢查溫度範圍 (-40°C ~ 80°C)
  if (data->temperature < -40.0 || data->temperature > 80.0) {
    return false;
  }
  
  // 檢查濕度範圍 (0% ~ 100%)
  if (data->humidity < 0.0 || data->humidity > 100.0) {
    return false;
  }
  
  return true;
}

// 格式化輸出感測器資料（減少 Serial 輸出）
String printSensorData(const SensorData* data, int rssi, float snr) {
  // 簡化輸出，減少 Serial 時間
  Serial.print("T:");
  Serial.print(data->temperature, 1);
  Serial.print("C H:");
  Serial.print(data->humidity, 1);
  Serial.print("% RSSI:");
  Serial.print(rssi);
  Serial.print(" SNR:");
  Serial.println(snr, 1);

  // 建立 JSON 字串（預估最大長度約 150 bytes）
  String jsonString = "";
  jsonString.reserve(200); // 預先分配記憶體，避免多次重新配置
  
  jsonString = "{";
  jsonString += "\"device_id\":\"0x" + String(data->device_id, HEX) + "\",";
  jsonString += "\"temperature\":" + String(data->temperature, 2) + ",";
  jsonString += "\"humidity\":" + String(data->humidity, 2) + ",";
  jsonString += "\"rssi\":" + String(rssi) + ",";
  jsonString += "\"snr\":" + String(snr, 2) + ",";
  jsonString += "\"timestamp\":" + String(millis());
  jsonString += "}";
  
  return jsonString;  
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  WiFi.disconnect();
  delay(100);
  // 初始化 LoRa
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { // 頻率需與傳送端一致
    Serial.println("LoRa 初始化失敗！");
    while (1);
  }
  LoRa.setSpreadingFactor(7);     // 設置擴頻因子
  LoRa.setSignalBandwidth(125E3); // 設置信號帶寬 
  LoRa.setCodingRate4(5);         // 設置編碼率  
  LoRa.setSyncWord(0x12);          // 同步訊號字，需與傳送端一致
  LoRa.setTxPower(10); 
  
  Serial.println("LoRa 接收端已啟動");

  reconnectWiFi();

    randomSeed(millis()); // 使用當前時間作為亂數種子
    sprintf(clientId, "d%lu", random(1000, 30000));  // 生成隨機的客戶端 ID
    Serial.print("Client ID: ");
    Serial.println(clientId);
    wifiClient.setBlockingMode();
    mqttClient.setServer(mqttServer, 1883); // 設定 MQTT 伺服器和埠號  

}


void loop() {
 
    if (!(mqttClient.connected())) {
        reconnect();
    }

  // 使用輪詢方式檢查 LoRa 封包
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    // 檢查封包大小是否正確
    if (packetSize == sizeof(SensorData)) {
      SensorData receivedData;
      
      // 讀取二進制資料到結構體
      if (LoRa.readBytes((uint8_t*)&receivedData, sizeof(SensorData)) == sizeof(SensorData)) {
        
        // 計算 CRC 校驗碼（不包含 checksum 欄位本身）
        uint16_t calculated_crc = calculate_crc16((uint8_t*)&receivedData, sizeof(SensorData) - sizeof(receivedData.checksum));
        
        // 驗證 CRC 校驗碼
        if (calculated_crc == receivedData.checksum) {
          
          // 驗證感測器資料的有效性
          if (validateSensorData(&receivedData)) {
            
            // 獲取訊號品質資訊
            int rssi = LoRa.packetRssi();
            float snr = LoRa.packetSnr();
            
            // 輸出感測器資料並建立 JSON
            String jsonData = printSensorData(&receivedData, rssi, snr);
            latestJsonData = jsonData; // 更新最新資料
            Serial.print("JSON: ");
            Serial.println(jsonData);

          } else {
            Serial.println("資料超範圍");
          }
          
        } else {
          Serial.println("CRC錯誤");
        }
        
      } else {
        Serial.println("讀取失敗");
      }
      
    } else {
      Serial.println("封包大小錯誤");
      // 清空緩衝區
      while (LoRa.available()) {
        LoRa.read();
      }
    }
  }


  
  static unsigned long lastMqttProcess = 0;
  unsigned long currentTime = millis();
  
  // 限制 MQTT 處理頻率：每 300ms 才執行一次
  // 使用時間差計算避免 millis() overflow 問題
  if ((currentTime - lastMqttProcess) >= 300) {
    lastMqttProcess = currentTime;
    
    
    // 每 10 秒發送一次 MQTT（如果有新資料）
    // 使用時間差計算避免 millis() overflow 問題
    if ((currentTime - lastMqttSendTime) >= MQTT_SEND_INTERVAL) {
      if (latestJsonData.length() > 0 && mqttClient.connected()) {
        // 檢查 JSON 字串長度，避免緩衝區溢位
        if (latestJsonData.length() < 256) {
          char jsonBuffer[256];
          latestJsonData.toCharArray(jsonBuffer, 256);
          sendMQTT(jsonBuffer);
          Serial.println("MQTT已發送");
        } else {
          Serial.println("JSON 字串過長，跳過發送");
        }
        latestJsonData = ""; // 清空已發送的資料
      }
      lastMqttSendTime = currentTime;
    }
  }


  mqttClient.loop();
  

   reTryConnTime = millis();
  // 使用時間差計算避免 millis() overflow 問題
  if ((reTryConnTime - reTryConnPreTime) >= reTryConnIntervalSwitch) {
      reTryConnPreTime = reTryConnTime;
      status = WiFi.status();
      if (status != WL_CONNECTED) {
          reconnectWiFi();
      }
  }
 
}
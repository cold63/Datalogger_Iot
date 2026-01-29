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
┌─────────────┐     Queue      ┌─────────────┐
│ ModbusTask  │ ─────────────> │  MqttTask   │
│ (讀取感測器) │  SensorData_t  │ (發布 MQTT)  │
└─────────────┘                └─────────────┘
       │                              │
       │                              │
       v                              v
   RS-485 感測器              MQTT Broker
*/

#include <ModbusRtu.h>
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// 定義溫濕度資料結構
typedef struct {
    uint16_t temperature;
    uint16_t humidity;
} SensorData_t;

// FreeRTOS Queue 用於 Modbus 與 MQTT 之間傳遞資料
QueueHandle_t sensorDataQueue;

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

uint16_t temperature, humidity;

char ssid[] = "your_ssid";     // your network SSID (name)
char pass[] = "your_password";  // your network password
int status  = WL_IDLE_STATUS;    // the Wifi radio's status
long lastTime = 0;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
volatile bool wifiConnected = false;

char username[] = "";  // device access token(存取權杖)
char password[] = "";                      // no need password
char mqttServer[]     = "broker.hivemq.com";
char clientId[20];
char publishTopic[]   = "md02Service/v1";   
//char subscribeTopic[] = "md02/su1";


/**
 * @brief WiFi 連線管理任務
 * 
 * 此任務負責監控 WiFi 連線狀態，並在斷線時自動重新連線。
 * 使用 wifiConnected 旗標通知其他任務目前的連線狀態。
 * 
 * @param pvParameters FreeRTOS 任務參數 (未使用)
 */
void wifiTask(void *pvParameters) {
    static unsigned long lastAttempt = 0;           // 上次嘗試連線的時間
    const unsigned long retryInterval = 10000;      // 重試間隔：10 秒
    
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            // WiFi 未連線，檢查是否達到重試間隔
            if (millis() - lastAttempt > retryInterval) {
                Serial.print("WiFi disconnected, try reconnect: ");
                Serial.println(ssid);
                WiFi.disconnect();                              // 先主動斷線，清除殘留 session
                vTaskDelay(500 / portTICK_PERIOD_MS);           // 等待斷線完成
                WiFi.begin(ssid, pass);                         // 嘗試重新連線
                lastAttempt = millis();                         // 更新嘗試時間
            }
            wifiConnected = false;                              // 更新連線狀態旗標
        } else {
            wifiConnected = true;                               // WiFi 已連線
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);                  // 每秒檢查一次
    }
}

/**
 * @brief Modbus RTU 通訊任務
 * 
 * 此任務使用狀態機輪詢 RS-485 溫濕度感測器 (MD-02)，
 * 讀取溫度和濕度數值後，透過 Queue 傳送給 MqttTask。
 * 
 * 狀態機流程：
 *   State 0: 等待狀態，等待 2 秒後進入下一狀態
 *   State 1: 發送 Modbus 查詢請求
 *   State 2: 輪詢等待回應，讀取完成後將資料送入 Queue
 * 
 * @param pvParameters FreeRTOS 任務參數 (未使用)
 */
void ModbusTask(void *pvParameters) {
    for (;;) {

        switch( u8state ) {
        case 0: 
            // 等待狀態：等待指定時間後進入查詢狀態
            if (millis() > u32wait) u8state++;
            break;
            
        case 1: 
            // 發送查詢：依序發送 telegram[0] (溫度) 和 telegram[1] (濕度)
            master.query( telegram[u8Query] );
            u8state++;
            u8Query++;
            if(u8Query > 1) u8Query = 0;    // 循環切換查詢
            break;
            
        case 2:
            // 輪詢回應：檢查是否收到從站回覆
            master.poll();
            if (master.getState() == COM_IDLE) {
                // 通訊完成，重置狀態機
                u8state = 0;
                u32wait = millis() + 2000;      // 設定下次查詢等待時間 (2 秒)
                
                // 讀取完成，將溫度及濕度數值存到變數中
                temperature = au16data[0];      // 溫度 (需除以 10)
                humidity = au16data[1];         // 濕度 (需除以 10)

                // 封裝資料並透過 Queue 發送給 MqttTask
                SensorData_t sensorData;
                sensorData.temperature = temperature;
                sensorData.humidity = humidity;
                xQueueSend(sensorDataQueue, &sensorData, 0);    // 非阻塞發送

                // 輸出到序列埠監控視窗
                Serial.print("溫度: ");
                Serial.println((float)temperature / 10);
                Serial.print("濕度: ");
                Serial.println((float)humidity / 10);
                Serial.println();
            }
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);   // 100ms 延遲，避免佔用過多 CPU
    }
}

/**
 * @brief MQTT 發布任務
 * 
 * 此任務負責：
 *   1. 維護 MQTT Broker 連線
 *   2. 從 Queue 接收 ModbusTask 傳來的感測器資料
 *   3. 每 3 秒發布一次最新的溫濕度資料到 MQTT Broker
 * 
 * Queue 接收策略：
 *   使用 while 迴圈清空 Queue，只保留最新資料，避免 Queue 溢出
 * 
 * @param pvParameters FreeRTOS 任務參數 (未使用)
 */
void MqttTask(void *pvParameters) {
    SensorData_t receivedData;              // 暫存從 Queue 接收的資料
    SensorData_t latestData = {0, 0};       // 保存最新資料，用於發布
    bool hasData = false;                   // 旗標：是否已收到過有效資料
    
    for (;;) {
        // 檢查 WiFi 連線狀態
        if (!wifiConnected) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);  // WiFi 未連線，等待 1 秒後重試
            continue;
        }

        // 維護 MQTT 連線
        if (!mqttClient.connected()) {
            Serial.print("Attempting MQTT connection...");
            if (mqttClient.connect(clientId, username, password)) {
                Serial.println("connected");
            } else {
                Serial.print("failed, rc=");
                Serial.print(mqttClient.state());
                Serial.println(" try again later");
                vTaskDelay(5000 / portTICK_PERIOD_MS);  // 連線失敗，等待 5 秒後重試
                continue;
            }
        }

        // 處理 MQTT 心跳與回調
        mqttClient.loop();
        
        // 從 Queue 接收最新資料 (非阻塞，清空 Queue 取得最新值)
        while (xQueueReceive(sensorDataQueue, &receivedData, 0) == pdTRUE) {
            latestData = receivedData;      // 持續更新為最新資料
            hasData = true;                 // 標記已收到有效資料
        }
        
        // 每 MQTT_SEND_INTERVAL (3 秒) 發送一次 MQTT
        if (hasData && (millis() - lastMQTTSendTime >= MQTT_SEND_INTERVAL)) {
            lastMQTTSendTime = millis();    // 更新發送時間戳
            
            // 組裝 JSON 格式字串
            String jsonString = "{\"temperature\":";
            jsonString += (float)latestData.temperature / 10;   // 溫度需除以 10
            jsonString += ",\"humidity\":";
            jsonString += (float)latestData.humidity / 10;      // 濕度需除以 10
            jsonString += "}";
            
            // 發布到 MQTT Broker
            if (mqttClient.connected()) {
                mqttClient.publish(publishTopic, jsonString.c_str());
                Serial.print("MQTT sent: ");
                Serial.println(jsonString);
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);   // 100ms 延遲，避免佔用過多 CPU
    }
}

/**
 * @brief 系統初始化
 * 
 * 初始化序列埠、Modbus、MQTT、FreeRTOS Queue 及任務
 */
void setup() {
  // 初始化序列埠
  Serial.begin(115200);                     // 除錯用序列埠
  mySerial.begin(9600);                     // Modbus RS-485 通訊用軟體序列埠

  // 設定 UART 多工器控制腳位
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  USE_RS485();                              // 切換到 RS-485 模式

  /* 設定查詢 0: 讀取溫度暫存器 ------------------------------------------
   * Modbus 功能碼 4 (0x04): 讀取輸入暫存器 (Read Input Registers)
   * 從站位址: 8
   * 起始位址: 1 (溫度暫存器)
   * 讀取數量: 1 個暫存器
   */
  telegram[0].u8id = 1;           // 從站位址
  telegram[0].u8fct = 4;          // 功能碼: 讀取輸入暫存器
  telegram[0].u16RegAdd = 1;      // 暫存器起始位址 (溫度)
  telegram[0].u16CoilsNo = 1;     // 讀取暫存器數量
  telegram[0].au16reg = au16data; // 資料存放指標 (存入 au16data[0])

  /* 設定查詢 1: 讀取濕度暫存器 ------------------------------------------
   * 從站位址: 8
   * 起始位址: 2 (濕度暫存器)
   * 讀取數量: 1 個暫存器
   */
  telegram[1].u8id = 1;               // 從站位址
  telegram[1].u8fct = 4;              // 功能碼: 讀取輸入暫存器
  telegram[1].u16RegAdd = 2;          // 暫存器起始位址 (濕度)
  telegram[1].u16CoilsNo = 1;         // 讀取暫存器數量
  telegram[1].au16reg = au16data + 1; // 資料存放指標 (存入 au16data[1])

  //reconnectWiFi();
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

  // 建立 FreeRTOS Queue
  // 用於 ModbusTask -> MqttTask 之間傳遞感測器資料
  // 容量：5 筆 SensorData_t，避免短暫延遲造成資料遺失
  sensorDataQueue = xQueueCreate(5, sizeof(SensorData_t));
  if (sensorDataQueue == NULL) {
      Serial.println("Failed to create queue!");
  }

  // 建立 FreeRTOS 任務
  // 任務參數：函數指標, 名稱, 堆疊大小, 參數, 優先級, 任務控制代碼
  xTaskCreate(wifiTask, "WiFiTask", 4096, NULL, 1, NULL);       // WiFi 管理 (優先級 1)
  xTaskCreate(ModbusTask, "ModbusTask", 2048, NULL, 2, NULL);   // Modbus 通訊 (優先級 2)
  xTaskCreate(MqttTask, "MqttTask", 4096, NULL, 2, NULL);       // MQTT 發布 (優先級 2)
}

/**
 * @brief Arduino 主迴圈
 * 
 * 所有工作已交由 FreeRTOS 任務處理，主迴圈僅需讓出 CPU 時間
 */
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);    // 讓 FreeRTOS 排程器控制執行
}
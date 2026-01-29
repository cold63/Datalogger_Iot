/*
 * ============================================================================
 * 專案名稱: MD02 Modbus RS-485 溫溼度感測器讀取程式
 * 目標平台: RTL8720DF (Realtek Ameba)
 * ============================================================================
 * 
 * 執行這個範例前先安裝以下連結的程式庫:
 *   Modbus-Master-Slave-for-Arduino
 *   https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino/tree/master
 * 
 * 功能說明:
 *   1. 透過 RS-485 介面連接 MD-02 溫溼度感測器
 *   2. 使用 Modbus RTU 協定讀取溫度與濕度數據
 *   3. 透過 WiFi 將數據上傳至 ThingSpeak 雲端平台
 * 
 * 硬體規格:
 *   - 感測器型號: RS485-MD-02
 *   - Modbus 從站位址: 0x08
 *   - 溫度暫存器位址: 0x0001 (1 個 16-bit 暫存器)
 *   - 濕度暫存器位址: 0x0002 (1 個 16-bit 暫存器)
 *   - 數值處理: 讀取值需除以 10 得到實際溫濕度
 * 
 * 通訊架構:
 *   RTL8720DF <--RS485--> MD-02 感測器
 *   RTL8720DF <--WiFi--> ThingSpeak 雲端
 * 
 * 備註:
 *   原 MD-02 可一次讀取溫度及濕度兩個暫存器，
 *   但本範例為示範多 query 使用方式，分別發送兩個查詢。
 * ============================================================================
 */

/* ============================================================================
 * 程式庫引入區
 * ============================================================================ */
#include <ModbusRtu.h>      // Modbus RTU 主從通訊程式庫
#include <SoftwareSerial.h> // 軟體序列埠程式庫
#include <WiFi.h>           // WiFi 連線程式庫
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
/* ============================================================================
 * UART 多工器選擇腳位定義
 * 說明: 使用 S0, S1 兩個腳位控制 UART 多工器，切換不同的通訊裝置
 * ============================================================================ */
#define S0  1  // 多工器選擇腳位 0
#define S1  0  // 多工器選擇腳位 1

/* UART 多工器裝置選擇巨集定義
 * S1  S0  | 選擇裝置
 * --------|----------
 *  0   0  | PMS5003 空氣品質感測器
 *  0   1  | RS-485 (Modbus 裝置)
 *  1   0  | GPS 模組
 *  1   1  | 外部 UART 裝置
 */
#define USE_PMS5003() do{ digitalWrite(S0, LOW); digitalWrite(S1, LOW); } while(0)        // 選擇 PMS5003
#define USE_RS485() do{ digitalWrite(S0, HIGH); digitalWrite(S1, LOW); } while(0)         // 選擇 RS-485
#define USE_GPS() do{ digitalWrite(S0, LOW); digitalWrite(S1, HIGH); } while(0)           // 選擇 GPS
#define USE_EXTERNAL_UART() do{ digitalWrite(S0, HIGH); digitalWrite(S1, HIGH); } while(0) // 選擇外部 UART

/* ============================================================================
 * WiFi 連線設定
 * ============================================================================ */
char ssid[] = "your_ssid";      // WiFi 網路名稱 (SSID)
char pass[] = "your_password"; // WiFi 網路密碼
int status = WL_IDLE_STATUS;   // WiFi 連線狀態變數
volatile bool wifiConnected = false;

/* ============================================================================
 * ThingSpeak 雲端平台設定
 * ============================================================================ */
char server[] = "api.thingspeak.com";      // ThingSpeak API 伺服器位址
const String api_key = "your_api_key"; // ThingSpeak 頻道寫入 API 金鑰

/* ============================================================================
 * 網路連線相關變數
 * ============================================================================ */
unsigned long ConnectTimeOut;  // WiFi 連線逾時計時器
WiFiClient client;             // WiFi 客戶端物件
//WiFiSSLClient client;        // SSL 加密連線客戶端 (備用)

/* ============================================================================
 * Modbus 通訊相關變數
 * ============================================================================ */
uint16_t au16data[16];  // Modbus 資料陣列，用於儲存從 slave 讀取的暫存器數據
uint8_t u8state;        // 狀態機目前狀態 (0: 等待, 1: 發送查詢, 2: 輪詢回應)
uint8_t u8Query;        // 目前執行的查詢編號 (0: 溫度, 1: 濕度)

/* ============================================================================
 * 軟體序列埠設定
 * ============================================================================ */
SoftwareSerial mySerial(2, 3); // 軟體序列埠: RX=Pin2, TX=Pin3

/* ============================================================================
 * Modbus 主站物件宣告
 * 參數說明:
 *   - u8id: 節點 ID，0 表示主站，1~247 表示從站
 *   - port: 使用的序列埠
 *   - u8txenpin: RS-232/USB 設為 0，RS-485 設為 >1 的腳位編號
 * ============================================================================ */
Modbus master(0, mySerial); // 建立 Modbus 主站，使用軟體序列埠

/* ============================================================================
 * Modbus 查詢結構陣列
 * 說明: 定義兩個查詢結構，分別用於讀取溫度和濕度
 * ============================================================================ */
modbus_t telegram[2];

/* ============================================================================
 * 其他全域變數
 * ============================================================================ */
unsigned long u32wait;          // Modbus 查詢間隔等待計時器
uint16_t temperature, humdidity; // 溫度與濕度原始數值 (需除以 10 得到實際值)

// FreeRTOS 佇列
#define QUEUE_SIZE 10
QueueHandle_t Queue;

// 溫溼度數據結構體 (用於 Queue 傳輸)
typedef struct {
    uint16_t temperature;
    uint16_t humidity;
} SensorData_t;

void ModbusTask(void *pvParameters) {
  /* --------------------------------------------------------------------------
   * Modbus 狀態機
   * 狀態說明:
   *   - 狀態 0: 等待狀態，等待指定時間後進入查詢狀態
   *   - 狀態 1: 發送 Modbus 查詢請求
   *   - 狀態 2: 輪詢檢查回應，處理接收到的數據
   * -------------------------------------------------------------------------- */
  
  // FreeRTOS 任務必須包含無限循環
  for (;;) {
    switch( u8state ) {
    
    /* 狀態 0: 等待狀態 */
    case 0: 
      if (millis() > u32wait) u8state++; // 等待時間到，進入查詢狀態
      break;
    
    /* 狀態 1: 設定並發送 Modbus 查詢 */
    case 1: 

      master.query( telegram[u8Query] ); // 發送目前的查詢請求
      u8state++;                         // 進入輪詢狀態
      u8Query++;                         // 切換到下一個查詢

      // 查詢編號循環 (0 -> 1 -> 0 -> ...)
      if(u8Query > 1) u8Query = 0;

      break;
    
    /* 狀態 2: 輪詢並處理回應 */
    case 2:
      master.poll(); // 輪詢檢查是否有回應訊息
      
      // 當通訊完成 (狀態為 IDLE) 時處理數據
      if (master.getState() == COM_IDLE) {
        u8state = 0;                    // 重置狀態機
        u32wait = millis() + 30000;     // 設定下次查詢等待時間 (30 秒後)
        
        /* 將讀取到的原始數據存入變數 */
        temperature = au16data[0];      // 溫度原始值
        humdidity = au16data[1];        // 濕度原始值

        /* 輸出溫濕度數值到序列埠監控視窗
         * 注意: 原始值需除以 10 得到實際溫濕度
         */
        Serial.print("溫度: ");
        Serial.println((float)temperature/10);
        Serial.print("濕度: ");
        Serial.println((float)humdidity/10);
        Serial.println();

        /* 將數據透過 Queue 傳送給 SendThingTask */
        SensorData_t sensorData;
        sensorData.temperature = temperature;
        sensorData.humidity = humdidity;
        
        // 嘗試將數據放入 Queue (不等待，若滿則丟棄)
        if (xQueueSend(Queue, &sensorData, 0) != pdPASS) {
            Serial.println("Queue full, data discarded");
        }
      }
      break;
    }
    
    // Modbus 輪詢間隔 (10ms)，狀態機內部控制 2 秒查詢間隔
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void wifiTask(void *pvParameters) {
    static unsigned long lastAttempt = 0;
    const unsigned long retryInterval = 10000; // 10秒重試一次
    
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            if (millis() - lastAttempt > retryInterval) {
                Serial.print("WiFi disconnected, try reconnect: ");
                Serial.println(ssid);
                WiFi.disconnect(); // 先主動斷線，消除殘留 session
                vTaskDelay(500 / portTICK_PERIOD_MS); // 斷線後稍等
                WiFi.begin(ssid, pass);
                lastAttempt = millis();
            }
            wifiConnected = false;
        } else {
            wifiConnected = true;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void SendThingTask(void *pvParameters) {
    SensorData_t receivedData;
    
    for (;;) {
        // 等待從 Queue 接收數據 (等待 60 秒)
        if (xQueueReceive(Queue, &receivedData, (60 * 1000) / portTICK_PERIOD_MS) == pdPASS) {
            
            // 檢查 WiFi 是否已連線
            if (!wifiConnected) {
                Serial.println("WiFi not connected, skip upload");
                continue;
            }
            
            // 建立 HTTP 連線到 ThingSpeak 伺服器
            if (client.connect(server, 80)) {
                Serial.println("Connected to ThingSpeak server");

                /* 發送 HTTP GET 請求，將溫濕度數據寫入 ThingSpeak
                 * field1: 溫度 (原始值除以 10)
                 * field2: 濕度 (原始值除以 10)
                 */
                float temp = (float)receivedData.temperature / 10.0;
                float hum = (float)receivedData.humidity / 10.0;
                
                Serial.print("Uploading - Temp: ");
                Serial.print(temp);
                Serial.print(", Humidity: ");
                Serial.println(hum);

                client.println("GET /update?api_key=" + api_key + 
                               "&field1=" + String(temp) + 
                               "&field2=" + String(hum) + " HTTP/1.1");
                client.println("HOST: api.thingspeak.com");
                client.println("Connection: close");
                client.println();

                vTaskDelay(50 / portTICK_PERIOD_MS);  // 等待伺服器回應

                client.stop();   // 關閉連線
                client.flush();  // 清空緩衝區
                
                Serial.println("Upload completed");
            } else {
                Serial.println("Failed to connect to ThingSpeak server");
            }
        } else {
            // Queue 接收超時 (60秒沒有新數據)
            Serial.println("No sensor data received in 60 seconds");
        }
        
        // 確保上傳間隔至少 60 秒 (ThingSpeak 免費版限制)
        vTaskDelay((60 * 1000) / portTICK_PERIOD_MS);
    }
}

/* ============================================================================
 * 函式: setup()
 * 功能: 系統初始化設定
 * 說明:
 *   1. 初始化序列埠 (除錯用 115200 bps, Modbus 用 9600 bps)
 *   2. 設定 UART 多工器腳位並選擇 RS-485 模式
 *   3. 連接 WiFi 網路
 *   4. 啟動 Modbus 主站並設定通訊參數
 * ============================================================================ */
void setup() {
  Serial.begin(115200);     // 初始化除錯序列埠 (115200 bps)
  mySerial.begin(9600);     // 初始化 Modbus 軟體序列埠 (9600 bps)

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

  // 設定 UART 多工器選擇腳位為輸出模式
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);

  USE_RS485();  // 切換多工器至 RS-485 模式

  // 建立佇列 (存放 SensorData_t 結構體)
  Queue = xQueueCreate(QUEUE_SIZE, sizeof(SensorData_t));
  if (Queue == NULL)
  {
      Serial.println("Queue create failed!");
      while (1)
          ;
  }

  // 建立任務
  xTaskCreate(wifiTask, "WiFiTask", 2048, NULL, 3, NULL);
  xTaskCreate(ModbusTask, "MODBUSTask", 4096, NULL, 2, NULL);
  xTaskCreate(SendThingTask, "SendThingTask", 4096, NULL, 2, NULL);
  master.start();            // 啟動 Modbus 主站

  master.setTimeOut(2000);   // 設定通訊逾時時間為 2000 ms
  u32wait = millis() + 1000; // 設定初始等待時間 (1 秒後開始查詢)
  u8state = 0;               // 初始化狀態機為等待狀態
}

/* ============================================================================
 * 函式: loop()
 * 功能: 主程式迴圈
 * ============================================================================ */
void loop() {
 vTaskDelay(1000 / portTICK_PERIOD_MS); // 讓 FreeRTOS 控制主流程
}
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SensirionI2cSht4x.h>
#include <LoRa.h>
// macro definitions
// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

// 讀取間隔（毫秒）
#define READ_INTERVAL 5000  // 每 5 秒讀取一次

// 裝置唯一 ID
#define DEVICE_ID 0x12345678

// LoRa 引腳定義
#define LORA_NSS 10
#define LORA_RST 9
#define LORA_DIO0 5

// 感測器資料結構（使用 packed 避免記憶體對齊問題）
typedef struct {
    uint32_t device_id;      // 裝置唯一 ID (4 bytes)
    float temperature;       // 溫度 (4 bytes)
    float humidity;          // 濕度 (4 bytes)
    uint16_t checksum;       // CRC-16 校驗碼 (2 bytes)
} __attribute__((packed)) SensorData;  // 總共 14 bytes

// SHT40 感測器物件
SensirionI2cSht4x sensor;

// 錯誤處理變數
static char errorMessage[64];
static int16_t error;

// 上次讀取時間
unsigned long lastReadTime = 0;

// 讀取計數器
uint32_t readCount = 0;

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

// 初始化 SHT40 感測器
bool initSHT40() {
  
  sensor.begin(Wire, SHT40_I2C_ADDR_44);
  delay(100);
  
  sensor.softReset();
  delay(100);
  
  uint32_t serialNumber = 0;
  error = sensor.serialNumber(serialNumber);
  if (error != NO_ERROR) {
    Serial.print("Error trying to execute serialNumber(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return false;
  }
  
  Serial.println("SHT40 感測器初始化成功");
  Serial.print("序號: ");
  Serial.println(serialNumber);
  
  return true;
}

// 讀取 SHT40 感測器資料
bool readSHT40(float* temperature, float* humidity) {
  delay(20);
  
  error = sensor.measureLowestPrecision(*temperature, *humidity);
  if (error != NO_ERROR) {
    Serial.println("讀取 SHT40 感測器失敗！");
    Serial.print("Error trying to execute measureLowestPrecision(): ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
    return false;
  }
  
  return true;
}

// 顯示感測器資料
void displaySensorData(float temperature, float humidity) {
  Serial.println("=== 感測器資料 ===");
  Serial.print("讀取次數: ");
  Serial.println(readCount);
  
  Serial.print("溫度: ");
  Serial.print(temperature, 2);
  Serial.println("°C");
  
  Serial.print("濕度: ");
  Serial.print(humidity, 2);
  Serial.println("%RH");
  
  Serial.println("==================");
  Serial.println();
}

// 透過 LoRa 發送感測器資料
bool sendLoRaData(float temperature, float humidity) {
  SensorData data;
  
  // 填充資料結構
  data.device_id = DEVICE_ID;
  data.temperature = temperature;
  data.humidity = humidity;
  
  // 計算 CRC 校驗碼
  data.checksum = calculate_crc16((uint8_t*)&data, sizeof(SensorData) - sizeof(data.checksum));
  
  // 開始傳送
  LoRa.beginPacket();
  size_t written = LoRa.write((uint8_t*)&data, sizeof(SensorData));
  bool success = LoRa.endPacket();
  
  if (success && written == sizeof(SensorData)) {
    Serial.println("LoRa 傳送成功");
    Serial.print("  封包大小: ");
    Serial.print(sizeof(SensorData));
    Serial.print(" bytes, CRC: 0x");
    Serial.println(data.checksum, HEX);
    return true;
  } else {
    Serial.println("LoRa 傳送失敗");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
 
  // 初始化 LoRa
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { // 頻率設為 433MHz
    Serial.println("LoRa 初始化失敗！");
    while (1);
  }
  
  LoRa.setSpreadingFactor(7);     // 設置擴頻因子
  LoRa.setSignalBandwidth(125E3); // 設置信號帶寬 
  LoRa.setCodingRate4(5);         // 設置編碼率  
  LoRa.setSyncWord(0x12);          // 同步訊號字，需與傳送端一致
  LoRa.setTxPower(10); 
  Serial.println("LoRa 接收端已啟動");
    
  // 初始化 I2C
  Serial.println("正在初始化 I2C...");
  Wire.begin();
  delay(500);
  
  // 初始化 SHT40 感測器
  if (!initSHT40()) {
    Serial.println("系統啟動失敗！請修正問題後重新啟動。");
    while (1) {
      delay(1000);
    }
  }
  
}

void loop() {
  unsigned long currentTime = millis();
  
  // 檢查是否到達讀取時間
  if (currentTime - lastReadTime >= READ_INTERVAL) {
    lastReadTime = currentTime;
    readCount++;
    
    float temperature, humidity;
    
    Serial.println("--- 開始新的讀取 ---");
    
    // 讀取感測器資料
    if (readSHT40(&temperature, &humidity)) {
      
      // 檢查資料有效性
      if (temperature >= -40.0 && temperature <= 80.0 &&
          humidity >= 0.0 && humidity <= 100.0) {
        
        // 顯示資料
        displaySensorData(temperature, humidity);
        
        // 透過 LoRa 發送資料
        sendLoRaData(temperature, humidity);
        
      } else {
        Serial.println("感測器資料超出有效範圍");
        Serial.print("溫度: ");
        Serial.print(temperature);
        Serial.print("°C, 濕度: ");
        Serial.print(humidity);
        Serial.println("%RH");
      }
      
    } else {
      Serial.println("無法讀取感測器");

    }
  }
}

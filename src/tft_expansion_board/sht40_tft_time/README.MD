# TFT 溫濕度時間顯示器 程式設計文件

## 1. 功能說明

本程式旨在建立一個多功能的資訊顯示器，使用 ILI9341 TFT 螢幕來即時呈現以下資訊：

* **溫濕度**: 從 SHT40 感測器讀取當前的溫度 (°C) 和相對濕度 (%)。
* **日期與時間**: 透過 WiFi 連接到網路時間協定 (NTP) 伺服器來獲取並同步標準時間，然後顯示目前的年/月/日與時/分/秒。

程式會將螢幕橫向擺放 (320x240)，並將上述資訊繪製在帶有方框的獨立區域內，其中溫濕度會以較大的字體顯示，以求清晰易讀。

## 2. 必要函式庫提醒

為了讓此程式能正確編譯與運作，您需要確保已安裝以下函式庫：

* **核心函式庫 (通常已內建或隨開發板核心提供)**:
  
  * `SPI.h`: 用於 SPI 通訊。
  * `Wire.h`: 用於 I2C 通訊 (連接 SHT40 感測器)。
  * `AmebaILI9341.h`: Ameba 開發板專用的 ILI9341 TFT 螢幕驅動庫。
  * `WiFi.h`: 用於 WiFi 連線。

* **需手動安裝的函式庫**:
  
  * `TimeLib.h`: **(極為重要)** 此程式庫用於時間的計算與管理。
  * `SensirionI2cSht4x.h`: SHT4x 系列溫濕度感測器的驅動庫。

## 3. 程式碼結構詳解

### 3.1 標頭檔與全域變數

程式開頭首先引入所有必要的函式庫，並定義了 TFT 螢幕的硬體腳位。

```cpp
// 引入所需的函式庫
#include <SPI.h>
#include "AmebaILI9341.h"
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// 硬體腳位定義
#define TFT_BL_PIN    9
#define TFT_RESET_PIN 14
#define TFT_DC_PIN    5
#define TFT_CS_PIN    SPI_SS

// 初始化 TFT 和 SHT40 感測器的物件
AmebaILI9341 tft = AmebaILI9341(TFT_CS_PIN, TFT_DC_PIN, TFT_RESET_PIN);
SensirionI2cSht4x sensor;

// WiFi 與 NTP 相關設定
char ssid[] = "your_ssid"; // 請替換成您的 WiFi 名稱
char pass[] = "your_password"; // 請替換成您的 WiFi 密碼
char timeServer[] = "time.stdtime.gov.tw"; // 台灣標準時間伺服器
```

* **說明**: 這裡建立了 `tft` 和 `sensor` 物件，它們是後續操作硬體的基礎。同時，您必須修改 `ssid` 和 `pass` 變數以符合您所在的網路環境。

### 3.2 初始化 `setup()`

`setup()` 函式在開發板啟動時僅執行一次，負責完成所有硬體的初始化設定。

```cpp
void setup() {
    // 啟動序列埠，用於除錯
    Serial.begin(115200);

    // 設定並開啟 TFT 背光
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // 初始化 I2C 和 SHT40 感測器
    Wire.begin();
    sensor.begin(Wire, SHT40_I2C_ADDR_44);
    sensor.softReset(); // 軟重置感測器

    // 初始化 TFT 螢幕
    tft.begin();
    tft.setRotation(1); // 設定為橫向顯示
    tft.fillScreen(ILI9341_BLACK); // 清空螢幕為黑色

    // 繪製靜態的 UI 介面
    drawLayout();

    // 連接 WiFi 並同步 NTP 時間
    reconnectWiFi();

    // 首次讀取感測器數據並更新到螢幕
    GetSensorData();
    updateDisplayData();
}
```

* **執行流程**: 開啟背光 -> 初始化感測器 -> 初始化螢幕 -> 繪製背景框 -> 連網對時 -> 顯示初始數據。

### 3.3 主迴圈 `loop()`

`loop()` 函式會不斷重複執行，負責動態更新螢幕上的數據。

```cpp
void loop() {
    // 每秒更新一次時間和日期
    if(second() != prevSecond){
        prevSecond = second();
        // ... (更新日期與時間的程式碼)
    }

    // 每 5 分鐘的 0 秒時，更新一次溫濕度
    int currentMinute = minute();
    if(currentMinute % 5 == 0 && currentMinute != lastUpdateMinute && second() == 0) {
        lastUpdateMinute = currentMinute;
        GetSensorData();
        updateDisplayData();
    }
}
```

* **更新邏輯**:
  1. **時間/日期**: 透過 `second() != prevSecond` 判斷是否新的一秒到來，如果是，就只更新時間與日期部分，避免不必要的螢幕閃爍。
  2. **溫濕度**: 為了降低功耗與網路請求，溫濕度設定為每 5 分鐘更新一次。

### 3.4 輔助函式

* `drawLayout()`: 負責繪製螢幕上所有靜態的元素，包括四個大方框以及 "DATE", "TIME", "Temp.(C)", "Humi.(%)" 等標題文字。此函式在 `setup()` 中被呼叫一次即可。
* `updateDisplayData()`: 專門用來顯示溫濕度數值。它會使用較大的字體，並在顯示新數據前，先用一個黑色的矩形 `fillRectangle()` 清除舊數據的區域，最後再印上新的數值。
* `GetSensorData()`: 呼叫 `sensor.measureLowestPrecision()` 來從 SHT40 獲取溫濕度值，並存入全域變數 `aTemperature` 和 `aHumidity` 中。
* `getNtpTime()` & `reconnectWiFi()`: 負責處理 WiFi 連線與 NTP 網路時間同步的複雜邏輯。

## 4. 核心邏輯分析

* **畫面佈局**: 程式的核心是 `drawLayout()` 函式。它透過 `tft.drawRectangle()` 在精確計算的座標上繪製出四個方框，將顯示區域劃分開來。標題和數據則使用 `tft.setCursor(x, y)` 定位後，再用 `tft.print()` 顯示。
* **非同步更新**: `loop()` 函式的設計體現了效率思維。它不是每一輪都刷新整個螢幕，而是根據時間變化來決定只更新需要變動的部分 (秒數變了就只更新時間，到了特定分鐘才更新溫濕度)，這大大減少了螢幕的閃爍感，也更有效率。
* **數據格式化**:
  * 日期與時間使用 `sprintf()` 格式化成 `YYYY-MM-DD` 和 `HH:MM:SS` 的格式。
  * 溫濕度是浮點數，使用 `dtostrf()` 函式將其轉換為字串，並控制只顯示一位小數。

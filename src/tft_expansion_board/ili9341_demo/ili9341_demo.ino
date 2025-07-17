#include <SPI.h>
#include "AmebaILI9341.h"

// --- 請根據你的實際接線修改以下腳位 ---
#define TFT_RESET       14
#define TFT_DC          5
#define TFT_CS          SPI_SS
#define SPI_BUS         SPI
#define ILI9341_SPI_FREQUENCY 20000000

// 初始化 TFT 物件
AmebaILI9341 tft = AmebaILI9341(TFT_CS, TFT_DC, TFT_RESET, SPI_BUS);

// --- UI 佈局參數 (已為橫向模式調整) ---
#define BOX_WIDTH     93  // 每個資訊框的寬度
#define BOX_HEIGHT    85  // 每個資訊框的高度
#define H_SPACING     10  // 框的水平間距
#define V_SPACING     10  // 框的垂直間距
#define LEFT_MARGIN   10  // 左邊距
#define TOP_MARGIN    40  // 頂部標題區域高度

// ----------------------------------------------------
//  setup() - 初始化與繪製靜態 UI
// ----------------------------------------------------
void setup() {
    Serial.begin(115200);
	
    pinMode(9, OUTPUT);
	// 開啟 TFT 背光 LED
    digitalWrite(9, HIGH);
	
    SPI_BUS.setDefaultFrequency(ILI9341_SPI_FREQUENCY);

    // 初始化 TFT 螢幕
    tft.begin();

    // ******** 主要變更：設定螢幕為橫向 ********
    // 0 = 直向, 1 = 橫向, 2 = 反向直向, 3 = 反向橫向
    tft.setRotation(1);

    // 繪製固定的 UI 框架
    drawUiLayout();
}

// ----------------------------------------------------
//  loop() - 循環更新動態數據 (此函式無變更)
// ----------------------------------------------------
void loop() {
    // 更新螢幕上的所有感測器數值
    updateAllData();

    // 等待 5 秒後再更新下一輪數據
    delay(5000);
}

// ----------------------------------------------------
//  drawUiLayout() - 繪製不會變動的 UI 元素
// ----------------------------------------------------
void drawUiLayout() {
    // 1. 設定整體背景色
    tft.fillScreen(ILI9341_BLACK);

    // 2. 繪製主標題 (調整游標位置以在 320px 寬度上置中)
    tft.setCursor(40, 15);
    tft.setFontSize(2); // 稍微縮小字體以適應標題欄
    tft.setForeground(ILI9341_CYAN);
    tft.print("Environmental Monitor");

    // 3. ******** 變更：繪製 3x2 的資訊框與其標籤 ********
    // 第一列
    drawInfoBox(0, 0, "PM1.0");
    drawInfoBox(0, 1, "PM2.5");
    drawInfoBox(0, 2, "PM10");
    // 第二列
    drawInfoBox(1, 0, "Temp");
    drawInfoBox(1, 1, "Humi");
    drawInfoBox(1, 2, "Date");
}

// ----------------------------------------------------
//  drawInfoBox() - 繪製單一資訊框的框架與標籤 (此函式無變更)
// ----------------------------------------------------
void drawInfoBox(int row, int col, const char* label) {
    // 計算框的左上角座標
    int16_t x = LEFT_MARGIN + col * (BOX_WIDTH + H_SPACING);
    int16_t y = TOP_MARGIN + row * (BOX_HEIGHT + V_SPACING);

    // 繪製外框
    tft.drawRectangle(x, y, BOX_WIDTH, BOX_HEIGHT, ILI9341_DARKGREY);

    // 繪製資訊標籤 (e.g., "PM2.5")
    tft.setCursor(x + 10, y + 10);
    tft.setFontSize(2);
    tft.setForeground(ILI9341_WHITE);
    tft.print(label);
}

// ----------------------------------------------------
//  updateAllData() - 更新所有會變動的數值
// ----------------------------------------------------
void updateAllData() {
    // ****** 在這裡填入你讀取感測器的真實程式碼 ******
    // 以下使用隨機數模擬感測器數據 (數據本身無變更)
    float pm1_0 = random(5, 20) / 10.0 + 5.0;
    float pm2_5 = random(10, 150) / 10.0 + 10.0;
    float pm10  = random(20, 200) / 10.0 + 20.0;
    float temp  = random(5, 40) / 10.0 + 25.0;
    float hum   = random(0, 30) / 10.0 + 60.0;
    int year = 2025;
    int month = 7;
    int day = 4; // 更新日期

    char buffer[20];

    // --- ******** 變更：更新 3x2 資訊框的數值 ******** ---
    // PM1.0
    dtostrf(pm1_0, 4, 1, buffer);
    updateValueInBox(0, 0, buffer, "ug/m3");

    // PM2.5
    dtostrf(pm2_5, 4, 1, buffer);
    updateValueInBox(0, 1, buffer, "ug/m3");

    // PM10
    dtostrf(pm10, 4, 1, buffer);
    updateValueInBox(0, 2, buffer, "ug/m3");

    // Temperature
    dtostrf(temp, 4, 1, buffer);
    updateValueInBox(1, 0, buffer, " C");

    // Humidity
    dtostrf(hum, 4, 1, buffer);
    updateValueInBox(1, 1, buffer, " %");
    
    // Date
    sprintf(buffer, "%02d/%02d",  month, day);
    updateValueInBox(1, 2, buffer, ""); // 日期沒有單位
}

// ----------------------------------------------------
//  updateValueInBox() - 在指定的框內更新數值
// ----------------------------------------------------
void updateValueInBox(int row, int col, const char* value, const char* unit) {
    // 計算框的左上角座標
    int16_t x = LEFT_MARGIN + col * (BOX_WIDTH + H_SPACING);
    int16_t y = TOP_MARGIN + row * (BOX_HEIGHT + V_SPACING);

    // 定義數值和單位的顯示 Y 座標
    int16_t value_y = y + 40; 
    int16_t unit_y = y + 65;

    // 1. 清除舊數值與單位區域
    tft.fillRectangle(x + 5, value_y - 2, BOX_WIDTH - 10, 45, ILI9341_BLACK);

    // 2. 繪製新數值
    tft.setCursor(x + 10, value_y);
    tft.setFontSize(2);
    tft.setForeground(ILI9341_YELLOW);
    tft.print(value);
    
    // 3. 繪製單位
    tft.setCursor(x + 10, unit_y);
    tft.setFontSize(1); // 單位使用較小的字體
    tft.setForeground(ILI9341_LIGHTGREY);
    tft.print(unit);
}
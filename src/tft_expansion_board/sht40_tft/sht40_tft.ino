#include <SPI.h>
#include "AmebaILI9341.h"
#include <math.h> // 用於圖表模擬數據

// ------------------ 硬體腳位定義 ------------------
#define TFT_BL      9   // 背光控制腳位 (HIGH: 開啟, LOW: 關閉)
#define TFT_RESET   14  // TFT 重置腳位
#define TFT_DC      5   // TFT 資料/命令選擇腳位
#define TFT_CS      SPI_SS // TFT 片選腳位 (使用硬體 SPI SS)

// 建立 TFT 物件
AmebaILI9341 tft = AmebaILI9341(TFT_CS, TFT_DC, TFT_RESET);

// ------------------ 畫面佈局定義 ------------------
const int screenWidth = 320;
const int screenHeight = 240;

// 圖表區域
const int chartX = 30; // 左邊留出空間給刻度
const int chartY = 70;
const int chartWidth = 260; // 左右留出空間給刻度
const int chartHeight = 150; // 縮小一點高度給X軸標籤

// ------------------ 圖表歷史數據 ------------------
float tempHistory[chartWidth];
float humHistory[chartWidth];
int dataIndex = 0;

// ------------------ 副程式原型宣告 ------------------
void drawLayout();
void drawChartAxis();
void updateInfo();
void updateChart(float temp, float hum);
void resetChart();

// ------------------ 主要設定函式 ------------------
void setup() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);

    drawLayout();
    drawChartAxis(); // 繪製圖表座標軸

    // 初始化歷史數據
    resetChart();
}

// ------------------ 主要迴圈函式 ------------------
void loop() {
    updateInfo();
    delay(100); // 加快更新速度以觀察圖表刷新
}

// ------------------ 繪製靜態畫面佈局 ------------------
void drawLayout() {
    tft.drawRectangle(0, 0, screenWidth, screenHeight, ILI9341_WHITE);

    tft.setFontSize(2);
    // Temp Box
    tft.drawRectangle(10, 10, 95, 50, ILI9341_CYAN);
    tft.setForeground(ILI9341_YELLOW);
    tft.setCursor(20, 15);
    tft.print("Temp");

    // Humi Box
    tft.drawRectangle(115, 10, 95, 50, ILI9341_CYAN);
    tft.setForeground(ILI9341_GREEN);
    tft.setCursor(125, 15);
    tft.print("Humi");

    // Date/Time Box - 稍微加寬以容納文字
    tft.drawRectangle(220, 10, 99, 50, ILI9341_CYAN);
}

// ------------------ 繪製圖表座標軸和刻度 ------------------
void drawChartAxis() {
    // 繪製圖表外框
    tft.drawRectangle(chartX, chartY, chartWidth, chartHeight, ILI9341_WHITE);
    tft.setFontSize(1);

    // 左側 (Humi) 刻度
    tft.setForeground(ILI9341_GREEN);
    for (int i = 0; i <= 4; i++) {
        int y = chartY + (chartHeight * i / 4);
        if (i > 0 && i < 4) { // Don't draw over the main box line
             tft.drawLine(chartX - 3, y, chartX, y, ILI9341_GREEN);
        }
        tft.setCursor(chartX - 28, y - 4);
        tft.print(100 - i * 25);
        tft.print("%");
    }

    // 右側 (Temp) 刻度
    tft.setForeground(ILI9341_YELLOW);
    for (int i = 0; i <= 5; i++) {
        int y = chartY + (chartHeight * i / 5);
        if (i > 0 && i < 5) { // Don't draw over the main box line
            tft.drawLine(chartX + chartWidth, y, chartX + chartWidth + 3, y, ILI9341_YELLOW);
        }
        tft.setCursor(chartX + chartWidth + 8, y - 4);
        tft.print(50 - i * 10);
        tft.print("C");
    }

    // X軸 (Time) 刻度
    tft.setForeground(ILI9341_WHITE);
    int time_total_s = chartWidth / 10; // 假設 10px = 1s
    int step_count = 5;
    int time_step_s = time_total_s / step_count;
    int pixel_step = chartWidth / step_count;

    for (int i = 0; i <= step_count; i++) {
        int x = chartX + i * pixel_step;
        tft.drawLine(x, chartY + chartHeight, x, chartY + chartHeight + 3, ILI9341_WHITE);
        tft.setCursor(x - 4, chartY + chartHeight + 5);
        tft.print(i * time_step_s);
    }
     tft.setCursor(chartX + chartWidth - 10, chartY + chartHeight + 5);
     tft.print("s");
}

// ------------------ 更新所有動態資訊 ------------------
void updateInfo() {
    float temperature = 27.5 + (sin(millis() / 8000.0) * 25.5); // 2-53度
    float humidity = 50.0 + (cos(millis() / 5000.0) * 50.0); // 0-100%

    char buffer[16];
    tft.setBackground(ILI9341_BLACK);

    // 更新溫度和濕度 (字體大小 2)
    tft.setFontSize(2);
    dtostrf(temperature, 4, 1, buffer);
    tft.setForeground(ILI9341_YELLOW);
    tft.fillRectangle(20, 40, 80, 18, ILI9341_BLACK);
    tft.setCursor(20, 40);
    tft.print(buffer);

    dtostrf(humidity, 4, 1, buffer);
    tft.setForeground(ILI9341_GREEN);
    tft.fillRectangle(125, 40, 80, 18, ILI9341_BLACK);
    tft.setCursor(125, 40);
    tft.print(buffer);

    // 更新日期和時間 (字體大小 2)
    tft.setForeground(ILI9341_WHITE);
    // Y/M/D 
    sprintf(buffer, "24/08/14"); // 模擬日期 YY/MM/DD
    tft.fillRectangle(222, 15, 96, 18, ILI9341_BLACK);
    tft.setCursor(222, 15);
    tft.print(buffer);
    // H:M 
    sprintf(buffer, "%02d:%02d", (millis() / 60000) % 24, (millis() / 1000) % 60);
    tft.fillRectangle(238, 40, 60, 18, ILI9341_BLACK);
    tft.setCursor(238, 40);
    tft.print(buffer);

    updateChart(temperature, humidity);
}

// ------------------ 重置圖表區域 ------------------
void resetChart() {
    // 用黑色填滿圖表內部區域
    tft.fillRectangle(chartX + 1, chartY + 1, chartWidth - 2, chartHeight - 2, ILI9341_BLACK);
    // 重置歷史數據
    for (int i = 0; i < chartWidth; i++) {
        tempHistory[i] = -1;
        humHistory[i] = -1;
    }
    // 指針歸零
    dataIndex = 0;
}

// ------------------ 更新圖表繪製 ------------------
void updateChart(float temp, float hum) {
    // 如果圖表畫滿了，則重置
    if (dataIndex >= chartWidth) {
        resetChart();
    }

    tempHistory[dataIndex] = temp;
    humHistory[dataIndex] = hum;

    int prevIndex = (dataIndex == 0) ? 0 : dataIndex - 1;
    float prevTemp = tempHistory[prevIndex];
    float prevHum = humHistory[prevIndex];

    int currentX = chartX + dataIndex;
    int prevX = chartX + prevIndex;

    if (tempHistory[0] != -1) { // 確保是從第一個點開始畫
        // Map values to Y coordinates
        int tempY = chartY + chartHeight - map(constrain(temp, 0, 50), 0, 50, 1, chartHeight - 2);
        int prevTempY = chartY + chartHeight - map(constrain(prevTemp, 0, 50), 0, 50, 1, chartHeight - 2);
        int humY = chartY + chartHeight - map(constrain(hum, 0, 100), 0, 100, 1, chartHeight - 2);
        int prevHumY = chartY + chartHeight - map(constrain(prevHum, 0, 100), 0, 100, 1, chartHeight - 2);

        // Draw lines
        tft.drawLine(prevX, prevTempY, currentX, tempY, ILI9341_YELLOW);
        tft.drawLine(prevX, prevHumY, currentX, humY, ILI9341_GREEN);
    }

    dataIndex++;
}
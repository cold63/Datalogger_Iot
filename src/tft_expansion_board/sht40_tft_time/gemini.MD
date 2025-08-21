# 專案:建立 ili9341 連續顯示 數值的畫面

## 軟體:

- 這是 Arduino 的 TFT 240x320 的控制程式。

- 有 溫度、濕度、年月日 、時間等資訊需要顯示。

- 請使用橫向顯示。

- 請先閱讀自行定義副程式及範例程式。

- 所有資訊均顯示上面並設計框框將每一個資訊框起來。溫度、濕度的顯示比例佔比要大，而且要對稱

- 程式請加入繁體中文註解。

- 檔案名稱為 TFT_Demo_820.ino

## 硬體:

- GPIO 9 是 TFT LCD 的背光開關。

- GPIO 9 為 HIGH 是背光開啟。

- GPIO 9 為 LOW 是背光關閉。

- 硬體已接  TFT_RESET  為  14、 TFT_DC  為    5、TFT_CS 為   SPI_SS 、 SPI_BUS  為  SPI ，請使用 #define 定義

## 自行定義副程式:

- [ameba-arduino-d/Arduino_package/hardware/libraries/SPI/src/AmebaILI9341.h at dev · Ameba-AIoT/ameba-arduino-d · GitHub](https://github.com/Ameba-AIoT/ameba-arduino-d/blob/dev/Arduino_package/hardware/libraries/SPI/src/AmebaILI9341.h)

## 範例:

- [ameba-arduino-d/Arduino_package/hardware/libraries/SPI/examples/ILI9341_TFT_LCD_basic/ILI9341_TFT_LCD_basic.ino at dev · Ameba-AIoT/ameba-arduino-d · GitHub](https://github.com/Ameba-AIoT/ameba-arduino-d/blob/dev/Arduino_package/hardware/libraries/SPI/examples/ILI9341_TFT_LCD_basic/ILI9341_TFT_LCD_basic.ino)

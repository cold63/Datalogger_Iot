/**
 * @file udp_server.ino
 * @brief W5500 UDP Echo Server Example
 * 
 * 使用 bit-banging SPI 初始化 W5500，
 * 在 port 5000 監聽 UDP 封包並回傳。
 */

#include <Arduino.h>
#include "w5500_driver.h"

// 網路配置
wiz_NetInfo g_net_info = {
    .mac = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED},
    .ip  = {192, 168, 1, 177},
    .sn  = {255, 255, 255, 0},
    .gw  = {192, 168, 1, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_STATIC
};

#define UDP_PORT 5000
#define DATA_BUF_SIZE 2048
uint8_t data_buf[DATA_BUF_SIZE];

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("\n\n--- W5500 UDP Server Example ---");

    // [1] 初始化驅動 (包含 Reset 與 Callbacks 註冊)
    w5500_driver_init(NULL, NULL);
    
    // [2] 驗證 SPI
    uint8_t ver = getVERSIONR();
    Serial.print("W5500 Version: 0x");
    Serial.println(ver, HEX);
    if (ver != 0x04) {
        Serial.println("Error: SPI Communication failed!");
        while(1);
    }

    // [3] 設定網路資訊
    wizchip_setnetinfo(&g_net_info);
    Serial.println("Network configured.");

    // [4] 開啟 UDP Socket
    if (socket(0, Sn_MR_UDP, UDP_PORT, 0) == 0) {
        Serial.print("UDP Server listening on port ");
        Serial.println(UDP_PORT);
    } else {
        Serial.println("Failed to open socket.");
    }
}

void loop() {
    // 檢查是否有資料
    uint16_t size = getSn_RX_RSR(0);
    if (size > 0) {
        uint8_t dest_ip[4];
        uint16_t dest_port;
        
        // 接收封包
        int32_t len = recvfrom(0, data_buf, size, dest_ip, &dest_port);
        
        if (len > 0) {
            Serial.print("Received ");
            Serial.print(len);
            Serial.print(" bytes from ");
            for(int i=0; i<4; i++) {
                Serial.print(dest_ip[i]);
                if(i<3) Serial.print(".");
            }
            Serial.print(":");
            Serial.println(dest_port);

            // Echo back
            sendto(0, data_buf, len, dest_ip, dest_port);
        }
    }
}

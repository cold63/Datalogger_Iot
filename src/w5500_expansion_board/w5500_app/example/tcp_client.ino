/**
 * @file tcp_client.ino
 * @brief W5500 TCP Client Example
 * 
 * 主動連線至遠端 TCP 伺服器並發送訊息。
 */

#include <Arduino.h>
#include "w5500_driver.h"

// 本地網路配置
wiz_NetInfo g_net_info = {
    .mac = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE},
    .ip  = {192, 168, 1, 178},
    .sn  = {255, 255, 255, 0},
    .gw  = {192, 168, 1, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_STATIC
};

// 目標伺服器 IP (請修改為您的 PC IP)
uint8_t target_ip[4] = {192, 168, 1, 104}; 
uint16_t target_port = 5000;

#define LOCAL_PORT 6000
uint8_t data_buf[1024];

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("\n\n--- W5500 TCP Client Example ---");

    // [1] 初始化驅動
    w5500_driver_init(NULL, NULL);
    
    // [2] 設定網路資訊
    wizchip_setnetinfo(&g_net_info);
}

void loop() {
    uint8_t sn = 1; // 使用 Socket 1
    uint8_t status = getSn_SR(sn);
    static uint32_t last_send = 0;

    switch (status) {
        case SOCK_CLOSED:
            // 每 5 秒嘗試重新連線
            if (millis() - last_send > 5000) {
                last_send = millis();
                if (socket(sn, Sn_MR_TCP, LOCAL_PORT, 0) == sn) {
                    Serial.println("TCP Socket opened.");
                }
            }
            break;

        case SOCK_INIT:
            // 嘗試連線到伺服器
            Serial.print("Connecting to Server ");
            for(int i=0; i<4; i++) {
                Serial.print(target_ip[i]);
                if(i<3) Serial.print(".");
            }
            Serial.print(":");
            Serial.println(target_port);
            
            if (connect(sn, target_ip, target_port) == SOCK_OK) {
                Serial.println("Connection request sent.");
            }
            break;

        case SOCK_ESTABLISHED:
            // 已建立連線，發送測試訊息
            {
                if (millis() - last_send > 5000) {
                    last_send = millis();
                    const char* msg = "Hello from W5500 TCP Client!";
                    send(sn, (uint8_t*)msg, strlen(msg));
                    Serial.println("Message sent.");
                }

                // 接收回傳資料
                uint16_t size = getSn_RX_RSR(sn);
                if (size > 0) {
                    if (size > sizeof(data_buf)-1) size = sizeof(data_buf)-1;
                    int32_t len = recv(sn, data_buf, size);
                    if (len > 0) {
                        data_buf[len] = '\0';
                        Serial.print("Received from Server: ");
                        Serial.println((char*)data_buf);
                    }
                }
            }
            break;

        case SOCK_CLOSE_WAIT:
            disconnect(sn);
            break;

        default:
            if (status != SOCK_LISTEN && status != SOCK_SYNSENT && status != SOCK_SYNRECV) {
                close(sn);
            }
            break;
    }
}

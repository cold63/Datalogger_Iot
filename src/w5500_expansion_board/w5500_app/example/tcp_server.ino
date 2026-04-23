/**
 * @file tcp_server.ino
 * @brief W5500 TCP Echo Server Example
 * 
 * 監聽 TCP Port 5000，接受客戶端連線並回傳所收到的資料。
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

#define TCP_PORT 5000
#define DATA_BUF_SIZE 2048
uint8_t data_buf[DATA_BUF_SIZE];

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("\n\n--- W5500 TCP Server Example ---");

    // [1] 初始化驅動
    w5500_driver_init(NULL, NULL);
    
    // [2] 設定網路資訊
    wizchip_setnetinfo(&g_net_info);
    Serial.println("Network configured.");
}

void loop() {
    uint8_t sn = 0; // 使用 Socket 0
    uint8_t status = getSn_SR(sn);

    switch (status) {
        case SOCK_CLOSED:
            // 開啟 Socket
            if (socket(sn, Sn_MR_TCP, TCP_PORT, 0) == sn) {
                Serial.println("TCP Socket opened, listening...");
            }
            break;

        case SOCK_INIT:
            // 進入 Listen 狀態
            listen(sn);
            break;

        case SOCK_ESTABLISHED:
            // 已建立連線
            {
                static bool connected = false;
                if (!connected) {
                    uint8_t remote_ip[4];
                    uint16_t remote_port;
                    getSn_DIPR(sn, remote_ip);
                    remote_port = getSn_DPORT(sn);
                    
                    Serial.print("Client connected from ");
                    for(int i=0; i<4; i++) {
                        Serial.print(remote_ip[i]);
                        if(i<3) Serial.print(".");
                    }
                    Serial.print(":");
                    Serial.println(remote_port);
                    connected = true;
                }

                // 檢查是否有資料
                uint16_t size = getSn_RX_RSR(sn);
                if (size > 0) {
                    if (size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
                    int32_t len = recv(sn, data_buf, size);
                    if (len > 0) {
                        Serial.print("Received ");
                        Serial.print(len);
                        Serial.println(" bytes, echoing back...");
                        send(sn, data_buf, len);
                    }
                }
            }
            break;

        case SOCK_CLOSE_WAIT:
            // 客戶端要求斷開
            Serial.println("Client disconnecting...");
            disconnect(sn);
            break;

        case SOCK_FIN_WAIT:
        case SOCK_CLOSING:
        case SOCK_TIME_WAIT:
            // 等待關閉完成
            close(sn);
            break;
    }
}

/**
 * @file udp_client.ino
 * @brief W5500 UDP Client Example
 * 
 * 定期發送 UDP 封包到伺服器並接收回覆。
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

// 目標伺服器 (請修改為您的 PC IP)
uint8_t target_ip[4] = {192, 168, 1, 104}; 
uint16_t target_port = 5000;

#define LOCAL_PORT 6000

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("\n\n--- W5500 UDP Client Example ---");

    // [1] 初始化驅動
    w5500_driver_init(NULL, NULL);
    
    // [2] 設定網路資訊
    wizchip_setnetinfo(&g_net_info);
    
    // [3] 開啟本地 Socket
    if (socket(1, Sn_MR_UDP, LOCAL_PORT, 0) == 1) {
        Serial.print("UDP Client port ");
        Serial.print(LOCAL_PORT);
        Serial.println(" opened.");
    } else {
        Serial.println("Failed to open socket.");
    }
}

void loop() {
    static uint32_t last_send = 0;
    
    // 每 5 秒發送一次訊息
    if (millis() - last_send > 5000) {
        last_send = millis();
        
        const char* msg = "Hello from RTL8720DF!";
        uint16_t len = strlen(msg);
        
        Serial.print("Sending to ");
        for(int i=0; i<4; i++) {
            Serial.print(target_ip[i]);
            if(i<3) Serial.print(".");
        }
        Serial.print(":");
        Serial.println(target_port);

        int32_t ret = sendto(1, (uint8_t*)msg, len, target_ip, target_port);
        
        if (ret < 0) {
            Serial.print("Send failed error: ");
            Serial.println(ret);
        } else {
            Serial.println("Message sent successfully.");
        }
    }

    // 檢查回覆
    uint16_t size = getSn_RX_RSR(1);
    if (size > 0) {
        uint8_t remote_ip[4];
        uint16_t remote_port;
        uint8_t rx_buf[128];
        
        int32_t len = recvfrom(1, rx_buf, sizeof(rx_buf)-1, remote_ip, &remote_port);
        if (len > 0) {
            rx_buf[len] = '\0';
            Serial.print("Received reply from Server: ");
            Serial.println((char*)rx_buf);
        }
    }
}

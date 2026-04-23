/**
 * @file MQTT_dhcp.ino
 * @brief 使用 RTL8720D + W5500 實作 MQTT 發布與訂閱範例
 * 包含 DHCP 自動取得 IP、DNS 解析 Broker 網址、以及狀態機連線機制。
 */

#include <Arduino.h>
#include "w5500_driver.h"
#include "lib/ioLibrary/Internet/DHCP/dhcp.h"
#include "lib/ioLibrary/Internet/DNS/dns.h"

// --- MQTT 函式庫配置 ---
// 由於 Arduino IDE 不會自動連結子目錄中的 .c 檔案，因此直接 include .c 實作檔案
extern "C" {
#include "lib/ioLibrary/Internet/MQTT/mqtt_interface.h"
#include "lib/ioLibrary/Internet/MQTT/MQTTClient.h"
}

#include "lib/ioLibrary/Internet/MQTT/mqtt_interface.c"
#include "lib/ioLibrary/Internet/MQTT/MQTTClient.c"
#include "lib/ioLibrary/Internet/MQTT/MQTTPacket/src/MQTTPacket.c"
#include "lib/ioLibrary/Internet/MQTT/MQTTPacket/src/MQTTConnectClient.c"
#include "lib/ioLibrary/Internet/MQTT/MQTTPacket/src/MQTTDeserializePublish.c"
#include "lib/ioLibrary/Internet/MQTT/MQTTPacket/src/MQTTSerializePublish.c"
#include "lib/ioLibrary/Internet/MQTT/MQTTPacket/src/MQTTSubscribeClient.c"
#include "lib/ioLibrary/Internet/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.c"

// --- 硬體資源定義 (Socket 號碼) ---
#define DHCP_SOCKET     7
#define DNS_SOCKET      6
#define MQTT_SOCKET     0

// --- MQTT Broker 連線設定 ---
const char* mqtt_broker = "broker.hivemq.com";
uint16_t mqtt_port = 1883;
const char* client_id = "RTL8720D_W5500_Client";
const char* pub_topic = "w5500/test/status";
const char* sub_topic = "w5500/test/control";

// --- 記憶體緩衝區定義 ---
uint8_t dhcp_buffer[548];
uint8_t dns_buffer[MAX_DNS_BUF_SIZE];
unsigned char mqtt_send_buf[256];
unsigned char mqtt_read_buf[256];

// --- 網路卡預設資訊 ---
wiz_NetInfo g_net_info = {
    .mac = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED},
    .ip  = {0, 0, 0, 0},
    .sn  = {255, 255, 255, 0},
    .gw  = {0, 0, 0, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_DHCP
};

// --- 系統狀態機枚舉 ---
enum SystemState {
    STATE_INIT_NET,     // 初始化網路卡
    STATE_DHCP,         // 取得動態 IP
    STATE_DNS,          // 解析 Broker 網址
    STATE_MQTT_CONNECT, // 建立 TCP 與 MQTT 連線
    STATE_MQTT_IDLE,    // 正常運作模式 (維持連線/收發訊息)
    STATE_FAIL          // 錯誤處理
};

SystemState sys_state = STATE_INIT_NET;
uint8_t broker_ip[4] = {0, 0, 0, 0};
uint32_t last_tick = 0;
uint32_t last_pub_time = 0;

Network n;     // MQTT 網路層物件
MQTTClient c;  // MQTT 客戶端物件

// --- MQTT 接收訊息回呼函式 (Callback) ---
// 當訂閱的主題收到訊息時，會進入此函式
void messageArrived(MessageData* md) {
    MQTTMessage* message = md->message;
    char topic[64];
    int len = (md->topicName->lenstring.len < 63) ? md->topicName->lenstring.len : 63;
    memcpy(topic, md->topicName->lenstring.data, len);
    topic[len] = '\0';

    char payload[128];
    len = (message->payloadlen < 127) ? message->payloadlen : 127;
    memcpy(payload, message->payload, len);
    payload[len] = '\0';

    Serial.print("[MQTT] Received on "); Serial.print(topic);
    Serial.print(": "); Serial.println(payload);
    
    // 範例：簡單的指令解析
    if (strstr(payload, "LED_ON")) {
        Serial.println("[APP] Command: Turn LED ON");
    } else if (strstr(payload, "LED_OFF")) {
        Serial.println("[APP] Command: Turn LED OFF");
    }
}

// --- 簡易 DNS 查詢實作 (避免完整 DNS 庫過大) ---
uint16_t simple_dns_query(uint8_t sn, uint8_t* dns_server, const char* host, uint8_t* out_ip) {
    uint8_t buf[256];
    uint16_t query_len = 0;
    uint16_t id = 0x1234;
    // 構造 DNS Query Header
    buf[query_len++] = (uint8_t)(id >> 8); buf[query_len++] = (uint8_t)(id & 0xFF);
    buf[query_len++] = 0x01; buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x01; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x00; 
    // 構造 Host Name 段落
    const char* p = host;
    while (*p) {
        const char* next_dot = strchr(p, '.');
        uint8_t segment_len = (uint8_t)((next_dot) ? (next_dot - p) : strlen(p));
        buf[query_len++] = segment_len;
        memcpy(&buf[query_len], p, (size_t)segment_len);
        query_len += segment_len;
        p += (next_dot) ? (segment_len + 1) : segment_len;
    }
    buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x01; // Type A
    buf[query_len++] = 0x00; buf[query_len++] = 0x01; // Class IN
    
    if (socket(sn, Sn_MR_UDP, 53535, 0) != sn) return 0;
    sendto(sn, buf, query_len, dns_server, 53);
    
    uint32_t start_ms = (uint32_t)millis();
    while ((uint32_t)millis() - start_ms < 3000) {
        if (getSn_RX_RSR(sn) > 0) {
            uint8_t host_ip[4];
            uint16_t host_port;
            int32_t len = recvfrom(sn, buf, sizeof(buf), host_ip, &host_port);
            if (len > 12) { 
                uint16_t pos = query_len; 
                while (pos + 12 <= len) {
                    // 尋找回傳封包中的 IP 位址段 (簡單過濾)
                    if (buf[pos+2] == 0x00 && buf[pos+3] == 0x01 && buf[pos+10] == 0x00 && buf[pos+11] == 0x04) { 
                        out_ip[0] = buf[pos+12]; out_ip[1] = buf[pos+13]; out_ip[2] = buf[pos+14]; out_ip[3] = buf[pos+15];
                        close(sn); return 1;
                    }
                    pos++;
                }
            }
        }
        delay(10);
    }
    close(sn); return 0;
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("\n--- W5500 MQTT Publish Example ---");
    
    // 初始化 W5500 驅動程式 (分配各 Socket 的緩衝區大小)
    uint8_t tx_size[8] = {2,2,2,2,2,2,2,2};
    uint8_t rx_size[8] = {2,2,2,2,2,2,2,2};
    w5500_driver_init(tx_size, rx_size);
    
    // 初始化 DHCP 功能
    DHCP_init(DHCP_SOCKET, dhcp_buffer);
}

void loop() {
    static uint32_t last_dhcp_tick = 0;
    static uint32_t last_link_check = 0;

    // DHCP 每秒計時處理 (內部計數用)
    if ((uint32_t)millis() - last_dhcp_tick >= 1000) { DHCP_time_handler(); last_dhcp_tick = (uint32_t)millis(); }
    
    // MQTT 毫秒計時處理 (用於超時判斷)
    if ((uint32_t)millis() - last_tick >= 1) { MilliTimer_Handler(); last_tick = (uint32_t)millis(); }

    // 實體層網路狀態檢查 (每 2 秒一次)
    if ((uint32_t)millis() - last_link_check >= 2000) {
        if (wizphy_getphylink() == PHY_LINK_OFF) {
            if (sys_state != STATE_INIT_NET && sys_state != STATE_DHCP) {
                Serial.println("[NET] Physical link down! Restarting network...");
                sys_state = STATE_INIT_NET; // 斷線後重回 INIT 狀態
            }
        }
        last_link_check = (uint32_t)millis();
    }

    // 系統狀態機核心邏輯
    switch (sys_state) {
        case STATE_INIT_NET:
            Serial.println("[NET] Initializing Network...");
            wizchip_setnetinfo(&g_net_info);
            sys_state = STATE_DHCP;
            break;

        case STATE_DHCP:
            switch (DHCP_run()) {
                case DHCP_IP_ASSIGN:
                case DHCP_IP_CHANGED:
                case DHCP_IP_LEASED:
                    // 取得分配到的網路參數
                    getIPfromDHCP(g_net_info.ip); getGWfromDHCP(g_net_info.gw); 
                    getSNfromDHCP(g_net_info.sn); getDNSfromDHCP(g_net_info.dns);
                    wizchip_setnetinfo(&g_net_info);
                    
                    Serial.print("[DHCP] IP Assigned: ");
                    for(int i=0; i<4; i++) { Serial.print(g_net_info.ip[i]); if(i<3) Serial.print("."); }
                    Serial.println();
                    sys_state = STATE_DNS;
                    break;
                case DHCP_FAILED: Serial.println("[DHCP] Failed!"); sys_state = STATE_FAIL; break;
                default: break;
            }
            break;

        case STATE_DNS:
            // 透過 DNS 解析 MQTT Broker 的網域名稱
            Serial.print("[DNS] Querying: "); Serial.println(mqtt_broker);
            if (simple_dns_query(DNS_SOCKET, g_net_info.dns, mqtt_broker, broker_ip)) {
                Serial.print("[DNS] Found IP: ");
                for(int i=0; i<4; i++) { Serial.print(broker_ip[i]); if(i<3) Serial.print("."); }
                Serial.println();
                sys_state = STATE_MQTT_CONNECT;
            } else { Serial.println("[DNS] Failed!"); delay(2000); }
            break;

        case STATE_MQTT_CONNECT:
            Serial.print("[MQTT] Connecting to "); 
            for(int i=0; i<4; i++) { Serial.print(broker_ip[i]); if(i<3) Serial.print("."); }
            Serial.println("...");

            // 1. 初始化 TCP 網路層
            NewNetwork(&n, MQTT_SOCKET);
            if (ConnectNetwork(&n, broker_ip, (uint16_t)mqtt_port) == (int)SOCK_OK) {
                
                // 2. 初始化 MQTT Client 結構
                memset(mqtt_send_buf, 0, sizeof(mqtt_send_buf));
                memset(mqtt_read_buf, 0, sizeof(mqtt_read_buf));
                MQTTClientInit(&c, &n, 5000, mqtt_send_buf, sizeof(mqtt_send_buf), mqtt_read_buf, sizeof(mqtt_read_buf));
                
                // 強制清空回呼指標陣列，防止 Hard Fault
                for (int i = 0; i < MAX_MESSAGE_HANDLERS; ++i) {
                    c.messageHandlers[i].topicFilter = 0;
                    c.messageHandlers[i].fp = 0; 
                }
                c.defaultMessageHandler = 0;
                
                // 3. 發送 MQTT Connect 封包
                MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
                data.MQTTVersion = 3; 
                data.clientID.cstring = (char*)client_id; 
                data.keepAliveInterval = 60; 
                data.cleansession = 1;
                
                if (MQTTConnect(&c, &data) == (int)SUCCESSS) { 
                    Serial.println("[MQTT] Connected!"); 
                    
                    // 4. 連線後立即訂閱主題
                    if (MQTTSubscribe(&c, sub_topic, QOS0, messageArrived) == (int)SUCCESSS) {
                        Serial.print("[MQTT] Subscribed to: "); Serial.println(sub_topic);
                        sys_state = STATE_MQTT_IDLE; 
                    } else {
                        Serial.println("[MQTT] Subscribe Failed!");
                        sys_state = STATE_MQTT_IDLE; 
                    }
                }
                else { 
                    Serial.println("[MQTT] Connection Failed!"); 
                    n.disconnect(&n); 
                    delay(5000); 
                }
            } else { 
                Serial.println("[TCP] Connection Failed!"); 
                delay(5000); 
            }
            break;

        case STATE_MQTT_IDLE:
            // 檢查 MQTT 是否仍處於連線狀態
            if (!c.isconnected) { sys_state = STATE_MQTT_CONNECT; break; }

            // 每 10 秒發布一次狀態訊息
            if ((uint32_t)millis() - last_pub_time >= 10000) {
                char payload[64]; sprintf(payload, "{\"uptime\": %lu}", (uint32_t)millis() / 1000);
                MQTTMessage msg; 
                msg.qos = QOS0; msg.retained = 0; msg.dup = 0; 
                msg.payload = (void*)payload; msg.payloadlen = strlen(payload);
                
                Serial.print("[MQTT] Pub: "); Serial.println(payload);
                MQTTPublish(&c, (char*)pub_topic, &msg); 
                last_pub_time = (uint32_t)millis();
            }

            // 維持背景心跳與訊息處理 (Non-blocking)
            MQTTYield(&c, 100);
            break;

        case STATE_FAIL: 
            Serial.println("[SYS] Halted."); 
            delay(10000); 
            sys_state = STATE_INIT_NET; // 錯誤後嘗試重新開始
            break;
    }
}


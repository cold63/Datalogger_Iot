/**
 * @file w5500_try1.ino
 * @brief 使用 RTL8720D + W5500 實作 MQTT 發布與訂閱範例
 * 包含 DHCP 自動取得 IP、DNS 解析 Broker 網址、以及狀態機連線機制。
 */

#include <Arduino.h>
#include "w5500_driver.h"
#include "lib/ioLibrary/Internet/DHCP/dhcp.h"
#include "lib/ioLibrary/Internet/DNS/dns.h"

// --- MbedTLS Headers (AmebaD SDK 內建) ---
#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"

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
uint16_t mqtt_port = 8883; // MQTTS 預設 Port
const char* client_id = "RTL8720D_W5500_Client";
const char* pub_topic = "w5500/test/status";
const char* sub_topic = "w5500/test/control";

// --- 記憶體緩衝區定義 ---
uint8_t dhcp_buffer[548];
uint8_t dns_buffer[MAX_DNS_BUF_SIZE];
unsigned char mqtt_send_buf[512]; // 增加 Buffer 大小以應對 TLS
unsigned char mqtt_read_buf[512];

// --- 網路卡資訊配置 (DHCP 模式) ---
wiz_NetInfo g_net_info = {
    .mac = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED},
    .ip  = {0, 0, 0, 0},
    .sn  = {255, 255, 255, 0},
    .gw  = {0, 0, 0, 0},
    .dns = {8, 8, 8, 8},
    .dhcp = NETINFO_DHCP           // 設定為 DHCP 模式
};

// --- 系統狀態機枚舉 ---
enum SystemState {
    STATE_INIT_NET,     // 初始化網路卡
    STATE_DHCP,         // 取得動態 IP
    STATE_DNS,          // 解析 Broker 網址
    STATE_TCP_CONNECT,  // 建立 TCP 底層連線
    STATE_TLS_INIT,     // 初始化 MbedTLS
    STATE_TLS_HANDSHAKE,// TLS 握手
    STATE_MQTT_CONNECT, // 建立 MQTT 協定連線
    STATE_MQTT_IDLE,    // 正常運作模式
    STATE_FAIL          // 錯誤處理
};

SystemState sys_state = STATE_INIT_NET;
uint8_t broker_ip[4] = {0, 0, 0, 0};
uint32_t last_tick = 0;
uint32_t last_pub_time = 0;

Network n;     // MQTT 網路層物件
MQTTClient c;  // MQTT 客戶端物件

// --- MbedTLS 結構與 Socket 橋接 ---
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;
mbedtls_ctr_drbg_context ctr_drbg;
uint8_t mqtt_sock_num = MQTT_SOCKET;

// MbedTLS 的 W5500 發送橋接
static int w5500_ssl_send(void *ctx, const unsigned char *buf, size_t len) {
    uint8_t sn = *((uint8_t *)ctx);
    int32_t ret = send(sn, (uint8_t *)buf, len);
    if (ret < 0) return MBEDTLS_ERR_NET_SEND_FAILED;
    return ret;
}

// MbedTLS 的 W5500 接收橋接
static int w5500_ssl_recv(void *ctx, unsigned char *buf, size_t len) {
    uint8_t sn = *((uint8_t *)ctx);
    uint16_t size = getSn_RX_RSR(sn);
    if (size == 0) return MBEDTLS_ERR_SSL_WANT_READ;
    int32_t ret = recv(sn, buf, len > size ? size : len);
    if (ret < 0) return MBEDTLS_ERR_NET_RECV_FAILED;
    return ret;
}

// Paho MQTT 與 MbedTLS 的讀寫橋接
int mqtt_ssl_read(Network* n, unsigned char* buffer, int len, long timeout_ms) {
    mbedtls_ssl_set_bio(&ssl, &mqtt_sock_num, w5500_ssl_send, w5500_ssl_recv, NULL);
    uint32_t start = millis();
    int read_len = 0;
    while (read_len < len && (millis() - start < (uint32_t)timeout_ms)) {
        int ret = mbedtls_ssl_read(&ssl, buffer + read_len, len - read_len);
        if (ret > 0) read_len += ret;
        else if (ret != MBEDTLS_ERR_SSL_WANT_READ) break;
        delay(1);
    }
    return read_len;
}

int mqtt_ssl_write(Network* n, unsigned char* buffer, int len, long timeout_ms) {
    return mbedtls_ssl_write(&ssl, buffer, len);
}

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

    // 如果不是固定 IP，則執行 DHCP 計時器
    if (g_net_info.dhcp != NETINFO_STATIC) {
        if ((uint32_t)millis() - last_dhcp_tick >= 1000) { DHCP_time_handler(); last_dhcp_tick = (uint32_t)millis(); }
    }
    
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
            // 如果是固定 IP，直接跳過 DHCP 進入 DNS 階段
            if (g_net_info.dhcp == NETINFO_STATIC) {
                Serial.println("[NET] Static IP Mode Active");
                sys_state = STATE_DNS;
            } else {
                sys_state = STATE_DHCP;
            }
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
                sys_state = STATE_TCP_CONNECT; // 改為先建立 TCP
            } else { Serial.println("[DNS] Failed!"); delay(2000); }
            break;

        case STATE_TCP_CONNECT:
            Serial.println("[TCP] Connecting to 8883...");
            close(MQTT_SOCKET);
            if (socket(MQTT_SOCKET, Sn_MR_TCP, 54321, 0) == MQTT_SOCKET) {
                if (connect(MQTT_SOCKET, broker_ip, mqtt_port) == SOCK_OK) {
                    Serial.println("[TCP] Connected.");
                    sys_state = STATE_TLS_INIT;
                } else { Serial.println("[TCP] Failed."); delay(5000); }
            }
            break;

        case STATE_TLS_INIT: {
            Serial.println("[TLS] Initializing...");
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_ctr_drbg_init(&ctr_drbg);
            
            auto custom_entropy = [](void* data, unsigned char* output, size_t len) -> int {
                for(size_t i=0; i<len; i++) output[i] = (unsigned char)rand();
                return 0;
            };

            if (mbedtls_ctr_drbg_seed(&ctr_drbg, custom_entropy, NULL, (const unsigned char *)"mqtts", 5) != 0) {
                Serial.println("[TLS] Seed failed."); sys_state = STATE_FAIL; break;
            }

            mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE); // 不驗證憑證
            mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
            mbedtls_ssl_setup(&ssl, &conf);
            mbedtls_ssl_set_hostname(&ssl, mqtt_broker);
            mbedtls_ssl_set_bio(&ssl, &mqtt_sock_num, w5500_ssl_send, w5500_ssl_recv, NULL);
            
            Serial.println("[TLS] Starting Handshake (10-30s)...");
            sys_state = STATE_TLS_HANDSHAKE;
            break;
        }

        case STATE_TLS_HANDSHAKE: {
            int ret = mbedtls_ssl_handshake(&ssl);
            if (ret == 0) {
                Serial.println("[TLS] Handshake Success!");
                sys_state = STATE_MQTT_CONNECT;
            } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                Serial.print("[TLS] Handshake Failed: "); Serial.println(ret);
                sys_state = STATE_FAIL;
            }
            break;
        }

        case STATE_MQTT_CONNECT: {
            Serial.println("[MQTT] Connecting over SSL...");
            // 重新導向 Paho MQTT 的讀寫介面到 SSL 橋接
            n.my_socket = MQTT_SOCKET;
            n.mqttread = mqtt_ssl_read;
            n.mqttwrite = mqtt_ssl_write;

            // 初始化 MQTT Client 結構
            memset(mqtt_send_buf, 0, sizeof(mqtt_send_buf));
            memset(mqtt_read_buf, 0, sizeof(mqtt_read_buf));
            MQTTClientInit(&c, &n, 10000, mqtt_send_buf, sizeof(mqtt_send_buf), mqtt_read_buf, sizeof(mqtt_read_buf));
            
            for (int i = 0; i < MAX_MESSAGE_HANDLERS; ++i) {
                c.messageHandlers[i].topicFilter = 0;
                c.messageHandlers[i].fp = 0; 
            }
            c.defaultMessageHandler = 0;
            
            MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
            data.MQTTVersion = 3; 
            data.clientID.cstring = (char*)client_id; 
            data.keepAliveInterval = 60; 
            data.cleansession = 1;
            
            if (MQTTConnect(&c, &data) == (int)SUCCESSS) { 
                Serial.println("[MQTT] Connected!"); 
                if (MQTTSubscribe(&c, sub_topic, QOS0, messageArrived) == (int)SUCCESSS) {
                    Serial.print("[MQTT] Subscribed to: "); Serial.println(sub_topic);
                    sys_state = STATE_MQTT_IDLE; 
                } else { sys_state = STATE_MQTT_IDLE; }
            } else { 
                Serial.println("[MQTT] Connection Failed!"); 
                close(MQTT_SOCKET); mbedtls_ssl_free(&ssl); delay(5000); 
                sys_state = STATE_INIT_NET;
            }
            break;
        }

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


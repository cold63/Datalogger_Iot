/**
 * @file https_client_fixip.ino
 * @brief W5500 HTTPS Client Example with Static IP on RTL8720DF
 * 
 * 這是 W5500 透過 MbedTLS 庫實現的 HTTPS Client 範例，使用固定 IP 配置。
 */

#include <Arduino.h>
#include "w5500_driver.h"
#include "lib/ioLibrary/Internet/DHCP/dhcp.h"
#include "lib/ioLibrary/Internet/DNS/dns.h"

// MbedTLS Headers (AmebaD SDK 內建)
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

// --- Configuration ---
#define DNS_SOCKET      6
#define HTTPS_SOCKET    0

const char* target_host = "www.google.com";
uint16_t target_port = 443;

uint8_t dns_buffer[MAX_DNS_BUF_SIZE];

// --- Static IP Configuration ---
wiz_NetInfo g_net_info = {
    .mac  = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED},
    .ip   = {192, 168, 1, 123},      // <--- 請修改為您的固定 IP
    .sn   = {255, 255, 255, 0},      // <--- 子網路遮罩
    .gw   = {192, 168, 1, 1},        // <--- 閘道器 IP
    .dns  = {8, 8, 8, 8},            // <--- DNS 伺服器 IP (Google DNS)
    .dhcp = NETINFO_STATIC           // 使用固定 IP 模式
};

uint8_t current_resolved_ip[4];

// --- MbedTLS Structure ---
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;
mbedtls_ctr_drbg_context ctr_drbg;

// --- MbedTLS I/O Bridge for W5500 ---
static int w5500_mbedtls_send(void *ctx, const unsigned char *buf, size_t len) {
    uint8_t sn = *((uint8_t *)ctx);
    int32_t ret = send(sn, (uint8_t *)buf, len);
    if (ret < 0) return MBEDTLS_ERR_NET_SEND_FAILED;
    return ret;
}

static int w5500_mbedtls_recv(void *ctx, unsigned char *buf, size_t len) {
    uint8_t sn = *((uint8_t *)ctx);
    uint16_t size = getSn_RX_RSR(sn);
    if (size == 0) return MBEDTLS_ERR_SSL_WANT_READ;
    
    int32_t ret = recv(sn, buf, len > size ? size : len);
    if (ret < 0) return MBEDTLS_ERR_NET_RECV_FAILED;
    return ret;
}

// --- Lightweight Simple DNS Implementation ---
uint16_t simple_dns_query(uint8_t sn, uint8_t* dns_server, const char* host, uint8_t* out_ip) {
    uint8_t buf[256];
    uint16_t query_len = 0;
    uint16_t id = 0x1234;
    buf[query_len++] = id >> 8; buf[query_len++] = id & 0xFF;
    buf[query_len++] = 0x01; buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x01; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x00; 
    
    const char* p = host;
    while (*p) {
        const char* next_dot = strchr(p, '.');
        uint8_t segment_len = (next_dot) ? (next_dot - p) : strlen(p);
        buf[query_len++] = segment_len;
        memcpy(&buf[query_len], p, segment_len);
        query_len += segment_len;
        p += (next_dot) ? (segment_len + 1) : segment_len;
    }
    buf[query_len++] = 0x00; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x01; 
    buf[query_len++] = 0x00; buf[query_len++] = 0x01; 

    if (socket(sn, Sn_MR_UDP, 53535, 0) != sn) return 0;
    sendto(sn, buf, query_len, dns_server, 53);
    
    uint32_t start_ms = millis();
    while (millis() - start_ms < 3000) {
        uint16_t recv_len = getSn_RX_RSR(sn);
        if (recv_len > 0) {
            uint8_t host_ip[4];
            uint16_t host_port;
            int32_t len = recvfrom(sn, buf, sizeof(buf), host_ip, &host_port);
            if (len > 12) { 
                uint16_t pos = query_len; 
                while (pos + 12 <= len) {
                    if (buf[pos+2] == 0x00 && buf[pos+3] == 0x01 && 
                        buf[pos+10] == 0x00 && buf[pos+11] == 0x04) { 
                        out_ip[0] = buf[pos+12];
                        out_ip[1] = buf[pos+13];
                        out_ip[2] = buf[pos+14];
                        out_ip[3] = buf[pos+15];
                        close(sn);
                        return 1;
                    }
                    pos++;
                }
            }
        }
        delay(10);
    }
    close(sn);
    return 0;
}

enum SystemState {
    STATE_INIT_NET,
    STATE_DNS,
    STATE_TCP_CONNECT,
    STATE_TLS_INIT,
    STATE_TLS_HANDSHAKE,
    STATE_HTTPS_SEND,
    STATE_HTTPS_RECEIVE,
    STATE_DONE,
    STATE_FAIL
} sys_state = STATE_INIT_NET;

uint8_t https_sock_num = HTTPS_SOCKET;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("\n--- W5500 HTTPS Client (Static IP) ---");

    w5500_driver_init(NULL, NULL);
}

void loop() {
    static uint32_t last_timer = 0;
    if (millis() - last_timer >= 1000) {
        last_timer = millis();
        // Static 模式下不需要 DHCP_time_handler，但 DNS_time_handler 仍需運行
        DNS_time_handler();
    }

    switch (sys_state) {
        case STATE_INIT_NET:
            Serial.println("[NET] Configuring Static IP...");
            wizchip_setnetinfo(&g_net_info);
            
            // 立即檢查設定是否生效
            wiz_NetInfo check;
            wizchip_getnetinfo(&check);
            Serial.print("IP: "); for(int i=0; i<4; i++) { Serial.print(check.ip[i]); if(i<3) Serial.print("."); } Serial.println();
            
            sys_state = STATE_DNS;
            break;

        case STATE_DNS: {
            Serial.print("[DNS] Querying: "); Serial.println(target_host);
            if (simple_dns_query(DNS_SOCKET, g_net_info.dns, target_host, current_resolved_ip)) {
                Serial.println("[DNS] Success.");
                sys_state = STATE_TCP_CONNECT;
            } else {
                Serial.println("[DNS] Failed.");
                sys_state = STATE_FAIL;
            }
            break;
        }

        case STATE_TCP_CONNECT:
            close(HTTPS_SOCKET);
            if (socket(HTTPS_SOCKET, Sn_MR_TCP, 54321, 0) == HTTPS_SOCKET) {
                if (connect(HTTPS_SOCKET, current_resolved_ip, target_port) == SOCK_OK) {
                    Serial.println("[TCP] Connected to Port 443.");
                    sys_state = STATE_TLS_INIT;
                }
            }
            break;

        case STATE_TLS_INIT: {
            Serial.println("[TLS] Initializing MbedTLS...");
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_ctr_drbg_init(&ctr_drbg);

            auto custom_entropy = [](void* data, unsigned char* output, size_t len) -> int {
                for(size_t i=0; i<len; i++) output[i] = (unsigned char)rand();
                return 0;
            };

            if (mbedtls_ctr_drbg_seed(&ctr_drbg, custom_entropy, NULL, (const unsigned char *)"w5500", 5) != 0) {
                Serial.println("[TLS] Seed failed.");
                sys_state = STATE_FAIL;
                break;
            }

            mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
            mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE); 
            mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
            
            mbedtls_ssl_setup(&ssl, &conf);
            mbedtls_ssl_set_hostname(&ssl, target_host);
            mbedtls_ssl_set_bio(&ssl, &https_sock_num, w5500_mbedtls_send, w5500_mbedtls_recv, NULL);
            
            Serial.println("[TLS] Init done. Starting Handshake...");
            sys_state = STATE_TLS_HANDSHAKE;
            break;
        }

        case STATE_TLS_HANDSHAKE: {
            int ret = mbedtls_ssl_handshake(&ssl);
            if (ret == 0) {
                Serial.println("[TLS] Handshake Success!");
                sys_state = STATE_HTTPS_SEND;
            } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                Serial.print("[TLS] Handshake Failed: "); Serial.println(ret);
                sys_state = STATE_FAIL;
            }
            break;
        }

        case STATE_HTTPS_SEND: {
            char req[256];
            snprintf(req, sizeof(req), 
                "GET / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "Connection: close\r\n"
                "\r\n", target_host);
            mbedtls_ssl_write(&ssl, (const unsigned char *)req, strlen(req));
            Serial.println("[HTTPS] Request sent.");
            sys_state = STATE_HTTPS_RECEIVE;
            break;
        }

        case STATE_HTTPS_RECEIVE: {
            unsigned char buf[512];
            int ret = mbedtls_ssl_read(&ssl, buf, sizeof(buf) - 1);
            if (ret > 0) {
                buf[ret] = '\0';
                Serial.print((char*)buf);
            } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
                Serial.println("\n[HTTPS] Connection closed.");
                sys_state = STATE_DONE;
            }
            break;
        }

        case STATE_DONE:
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            close(HTTPS_SOCKET);
            Serial.println("--- END ---");
            while(1) delay(1000);
            break;

        case STATE_FAIL:
            Serial.println("[SYS] Error occurred.");
            while(1) delay(1000);
            break;
    }
}

# MbedTLS 中使用 OpenSSL 產生的金鑰與憑證

在 MbedTLS 中使用 OpenSSL 產生的金鑰（例如客戶端憑證 `client.crt` 與私鑰 `client.key`，以及伺服器的根憑證 `ca.crt`），主要分為以下幾個步驟：

### 1. 將 PEM 檔案轉換為 C 語言字串
MbedTLS 通常直接讀取 PEM 格式的字串（包含 `-----BEGIN CERTIFICATE-----` 等標籤）。你需要將這些檔案內容放入代碼中，並在結尾加上 `\0` 結束字元。

```cpp
// 根憑證 (CA Certificate) - 用於驗證伺服器
const char* CA_CERT = 
    "-----BEGIN CERTIFICATE-----\n"
    "ABC... (憑證內容) ...XYZ\n"
    "-----END CERTIFICATE-----";

// 客戶端憑證 (Client Certificate) - 如果伺服器要求雙向認證 (Mutual TLS)
const char* CLIENT_CERT = 
    "-----BEGIN CERTIFICATE-----\n"
    "...內容...\n"
    "-----END CERTIFICATE-----";

// 客戶端私鑰 (Client Private Key)
const char* CLIENT_KEY = 
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "...內容...\n"
    "-----END RSA PRIVATE KEY-----";
```

### 2. 需要新增的 MbedTLS 結構
你需要額外的 `mbedtls_x509_crt`（用於存儲憑證鏈）和 `mbedtls_pk_context`（用於存儲私鑰）。

```cpp
mbedtls_x509_crt cacert;    // 根憑證
mbedtls_x509_crt clicert;   // 客戶端憑證
mbedtls_pk_context pkey;    // 客戶端私鑰
```

### 3. 在 STATE_TLS_INIT 階段載入金鑰
你需要修改初始化部分，將憑證與金鑰解析並配置到 SSL 配置中：

```cpp
// 1. 初始化結構
mbedtls_x509_crt_init(&cacert);
mbedtls_x509_crt_init(&clicert);
mbedtls_pk_init(&pkey);

// 2. 解析根憑證 (用於驗證伺服器)
int ret = mbedtls_x509_crt_parse(&cacert, (const unsigned char *)CA_CERT, strlen(CA_CERT) + 1);
if (ret != 0) { /* 錯誤處理 */ }

// 3. 解析客戶端憑證與私鑰 (如果是雙向認證場景)
ret = mbedtls_x509_crt_parse(&clicert, (const unsigned char *)CLIENT_CERT, strlen(CLIENT_CERT) + 1);
ret = mbedtls_pk_parse_key(&pkey, (const unsigned char *)CLIENT_KEY, strlen(CLIENT_KEY) + 1, NULL, 0);

// 4. 將憑證配置進 SSL Config
// 設定驗證模式為 MBEDTLS_SSL_VERIFY_REQUIRED (原本是 NONE)
mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);

// 如果是雙向認證，提供自己的憑證
mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey);
```

### 4. 記憶體釋放
在完成通信或關閉連線時，記得釋放這些新增的記憶體結構：

```cpp
mbedtls_x509_crt_free(&cacert);
mbedtls_x509_crt_free(&clicert);
mbedtls_pk_free(&pkey);
```

### 注意事項
1. **內存消耗**：RTL8720DF 的內存雖然比一般 Arduino 多，但 TLS 握手與加載多個憑證（尤其是 RSA 2048/4096）會消耗大量 RAM。如果發生 `MBEDTLS_ERR_SSL_ALLOC_FAILED`，請考慮使用 **ECC (Elliptic Curve)** 憑證代替 RSA。
2. **格式**：OpenSSL 產生的 `.pem` 或 `.crt` 內容必須與上述 `const char*` 格式一致，每一行結尾建議加上 `\n`。
3. **時間同步**：如果啟用了 `MBEDTLS_SSL_VERIFY_REQUIRED`（驗證憑證有效性），你的系統必須有準確的時間（透過 SNTP），否則憑證會因為「不在有效期內」而驗證失敗。

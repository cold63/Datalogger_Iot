/**
 * @file w5500_lib.cpp
 * @brief W5500 Library Compilation Wrapper
 * 
 * 此文件用於將 W5500 的 .c 源文件編譯到 Arduino 項目中
 */

// 必須在包含任何 wizchip 頭文件之前定義芯片型號
#ifndef _WIZCHIP_
#define _WIZCHIP_ 5500
#endif

#ifndef _WIZCHIP_SOCK_NUM_
#define _WIZCHIP_SOCK_NUM_ 8
#endif

// 編譯 W5500 ioLibrary 的 C 源文件
#include "lib/ioLibrary/Ethernet/wizchip_conf.c"
#include "lib/ioLibrary/Ethernet/W5500/w5500.c"
#include "lib/ioLibrary/Ethernet/socket.c"

// DHCP Support
#include "lib/ioLibrary/Internet/DHCP/dhcp.c"

// DNS Support
#include "lib/ioLibrary/Internet/DNS/dns.c"


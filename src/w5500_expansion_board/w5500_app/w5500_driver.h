/**
 * @file w5500_driver.h
 * @brief W5500 Driver Header (Refactored based on PAA3905 Bit-banging SPI)
 */

#ifndef _W5500_DRIVER_H_
#define _W5500_DRIVER_H_

#include <Arduino.h>
#include <stdint.h>

/**
 * @brief Chip definition for ioLibrary
 */
#ifndef _WIZCHIP_
#define _WIZCHIP_                  5500
#endif

#ifndef _WIZCHIP_SOCK_NUM_
#define _WIZCHIP_SOCK_NUM_         8
#endif

#include "lib/ioLibrary/Ethernet/wizchip_conf.h"
#include "lib/ioLibrary/Ethernet/socket.h"
#include "lib/ioLibrary/Ethernet/W5500/w5500.h"
#include "lib/ioLibrary/Internet/DHCP/dhcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Pin Definitions (RTL8720DF PCB)
 */
#define W5500_PIN_SCLK    19
#define W5500_PIN_MISO    16
#define W5500_PIN_MOSI    17
#define W5500_PIN_NCS     18
#define W5500_PIN_RST     11

/**
 * @brief Bit-banging Macros (Matching PAA3905 style)
 */
#define wCS0    digitalWrite(W5500_PIN_NCS, LOW)
#define wCS1    digitalWrite(W5500_PIN_NCS, HIGH)
#define wSCK0   digitalWrite(W5500_PIN_SCLK, LOW)
#define wSCK1   digitalWrite(W5500_PIN_SCLK, HIGH)
#define wMOSI0  digitalWrite(W5500_PIN_MOSI, LOW)
#define wMOSI1  digitalWrite(W5500_PIN_MOSI, HIGH)
#define wMISO   digitalRead(W5500_PIN_MISO)

/**
 * @brief Driver Functions
 */
void w5500_driver_init(uint8_t* tx_size, uint8_t* rx_size);

// HAL Callbacks for WIZnet Library
void ameba_w5500_cs_select(void);
void ameba_w5500_cs_deselect(void);
uint8_t ameba_w5500_spi_read_byte(void);
void ameba_w5500_spi_write_byte(uint8_t wb);
void ameba_w5500_spi_read_burst(uint8_t* pBuf, uint16_t len);
void ameba_w5500_spi_write_burst(uint8_t* pBuf, uint16_t len);
void ameba_w5500_critical_enter(void);
void ameba_w5500_critical_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _W5500_DRIVER_H_ */

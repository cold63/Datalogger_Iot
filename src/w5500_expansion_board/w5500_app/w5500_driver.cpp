/**
 * @file w5500_driver.cpp
 * @brief W5500 Driver Implementation (Bit-banging SPI based on PAA3905)
 */

#include "w5500_driver.h"

// ===== Bit-banging SPI Implementation (PAA3905 style) =====

#define dly1u delayMicroseconds(1)

// Transfer 8 bits (Full Duplex)
static uint8_t tr8b(uint8_t data) {
    uint8_t received = 0;
    uint8_t bitmap = 0x80;
    for(int i=0; i<8; i++) {
        if (bitmap & data) wMOSI1; else wMOSI0;
        dly1u; wSCK1;
        if (wMISO) received |= bitmap;
        dly1u; wSCK0;
        bitmap >>= 1;
    }
    return received;
}

// ===== WIZnet HAL Implementation =====

void ameba_w5500_cs_select(void) {
    wCS0;
    delayMicroseconds(5); // Stabilize CS
}

void ameba_w5500_cs_deselect(void) {
    delayMicroseconds(5);
    wCS1;
}

uint8_t ameba_w5500_spi_read_byte(void) {
    return tr8b(0x00);
}

void ameba_w5500_spi_write_byte(uint8_t wb) {
    tr8b(wb);
}

void ameba_w5500_spi_read_burst(uint8_t* pBuf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        pBuf[i] = tr8b(0x00);
    }
}

void ameba_w5500_spi_write_burst(uint8_t* pBuf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        tr8b(pBuf[i]);
    }
}

void ameba_w5500_critical_enter(void) {
    noInterrupts();
}

void ameba_w5500_critical_exit(void) {
    interrupts();
}

// ===== Initialization =====

void w5500_driver_init(uint8_t* tx_size, uint8_t* rx_size) {
    // Setup Pins
    pinMode(W5500_PIN_NCS, OUTPUT);
    wCS1;
    pinMode(W5500_PIN_SCLK, OUTPUT);
    wSCK0;
    pinMode(W5500_PIN_MOSI, OUTPUT);
    wMOSI0;
    pinMode(W5500_PIN_MISO, INPUT);
    
    pinMode(W5500_PIN_RST, OUTPUT);
    digitalWrite(W5500_PIN_RST, LOW);
    delay(10);
    digitalWrite(W5500_PIN_RST, HIGH);
    delay(200);

    // Register Callbacks
    reg_wizchip_cris_cbfunc(ameba_w5500_critical_enter, ameba_w5500_critical_exit);
    reg_wizchip_cs_cbfunc(ameba_w5500_cs_select, ameba_w5500_cs_deselect);
    reg_wizchip_spi_cbfunc(ameba_w5500_spi_read_byte, ameba_w5500_spi_write_byte);
    reg_wizchip_spiburst_cbfunc(ameba_w5500_spi_read_burst, ameba_w5500_spi_write_burst);

    // Initial Configuration
    uint8_t tmp_m[8] = {2,2,2,2,2,2,2,2};
    if(!tx_size) tx_size = tmp_m;
    if(!rx_size) rx_size = tmp_m;
    
    wizchip_init(tx_size, rx_size);
    
    // Ensure Ping Response is enabled and RESET software logic
    setMR(0x80); // Software Reset
    delay(10);
    while(getMR() & 0x80);
    
    setMR(0x00); // Set to normal mode (Bit 4 PB = 0 means Respond to Ping)
}

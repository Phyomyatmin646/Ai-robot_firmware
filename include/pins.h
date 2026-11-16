#pragma once

#include <Arduino.h>

/**
 * ============================================================================
 * ESP32-S3 N16R8 + GPIO Expansion Board V2775 Pinout Definition
 * ============================================================================
 * 
 * CRITICAL HARDWARE RESTRICTIONS (ESP32-S3 Octal PSRAM / N16R8):
 * - GPIO 33, 34, 35, 36, 37 are strictly RESERVED for Octal PSRAM (OPI).
 *   NEVER use them for peripherals.
 * - GPIO 19 (USB D-) and GPIO 20 (USB D+) are used for Native USB.
 * - GPIO 43 (U0TXD) and GPIO 44 (U0RXD) are used for USB-to-UART Serial Monitor.
 * - GPIO 0, 3, 45, 46 are boot strapping pins and avoided here.
 */

// ============================================================================
// 1. I2S Audio Input: INMP441 MEMS Microphone
// ============================================================================
#define PIN_I2S_MIC_SCK       15    // I2S Bit Clock (BCLK)
#define PIN_I2S_MIC_WS        16    // I2S Word Select / Left-Right Clock (LRCLK)
#define PIN_I2S_MIC_SD        17    // I2S Serial Data In (SD)

// ============================================================================
// 2. I2S Audio Output: MAX98357A Class-D Amplifier
// ============================================================================
#define PIN_I2S_SPK_BCLK      5     // I2S Bit Clock (BCLK)
#define PIN_I2S_SPK_LRC       6     // I2S Word Select (LRC / WS)
#define PIN_I2S_SPK_DIN       7     // I2S Serial Data Out (DIN)

// ============================================================================
// 3. SPI Display: ILI9341 2.8" TFT (240x320)
// ============================================================================
#define PIN_TFT_MOSI          11    // Master Out Slave In (SDI)
#define PIN_TFT_MISO          13    // Master In Slave Out (SDO)
#define PIN_TFT_SCK           12    // SPI Clock (SCL / SCK)
#define PIN_TFT_CS            10    // Chip Select (CS)
#define PIN_TFT_DC            9     // Data / Command (DC / RS)
#define PIN_TFT_RST           14    // Reset (RST)
#define PIN_TFT_BL            4     // Backlight / LED PWM Brightness Control

// ============================================================================
// 4. Touchscreen Controller: XPT2046 (UNVERIFIED)
// Status: UNVERIFIED. Set to true only if physical display module includes XPT2046.
// ============================================================================
#define FEATURE_TOUCH_XPT2046_PRESENT 0
#define PIN_TOUCH_CS          8     // Touch Chip Select (T_CS)
#define PIN_TOUCH_IRQ         18    // Touch Interrupt (T_IRQ)

// ============================================================================
// 5. PN532 RFID Module (High-Speed UART Mode)
// Set PN532 DIP Switches: [SEL0=0 (L), SEL1=0 (L)] for UART mode.
// ============================================================================
#define PIN_PN532_RX          1     // ESP32 RX <- PN532 TXD
#define PIN_PN532_TX          2     // ESP32 TX -> PN532 RXD
#define PN532_UART_NUM        1     // HardwareSerial port 1

// ============================================================================
// 6. One-Channel Capacitive Touch Sensor (Push-to-Talk)
// TTP223 or equivalent digital touch sensor module
// ============================================================================
#define PIN_TOUCH_SENSOR      48    // Digital Input (Active HIGH on touch)

// ============================================================================
// 7. Physical Push Buttons (Volume Up / Volume Down)
// Active LOW with internal pull-up enabled
// ============================================================================
#define PIN_BTN_VOL_UP        41    // Push Button: Volume Up
#define PIN_BTN_VOL_DOWN      42    // Push Button: Volume Down

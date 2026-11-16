# Hardware Pin Mapping Specification

## 1. Overview & Board Identification

- **MCU Core:** ESP32-S3 N16R8 (Xtensa dual-core 32-bit LX7, up to 240 MHz).
- **Internal Storage:** 16 MB Octal SPI Flash.
- **Internal PSRAM:** 8 MB Octal SPI PSRAM (OPI).
- **Expansion Board:** ESP32-S3 GPIO Extension Board V2775.

---

## 2. Critical GPIO Restrictions & Preservation Matrix

| GPIO | Purpose in N16R8 Hardware | Allowed for Peripherals? | Reason / Restriction |
|---|---|---|---|
| **GPIO 33, 34, 35, 36, 37** | Octal PSRAM (OPI bus) | **STRICTLY FORBIDDEN** | Dedicated to high-speed 8MB PSRAM lines. Connecting peripherals will corrupt RAM and trigger kernel panic. |
| **GPIO 19, 20** | Native USB D- / D+ | **RESERVED** | Native USB CDC / JTAG serial interface for development and upload. |
| **GPIO 43, 44** | UART0 TX / RX | **RESERVED** | Connected to on-board USB-UART bridge for Serial Monitor at 115200 baud. |
| **GPIO 0** | Boot Strapping | Cautious (Pull-up) | Low at reset enters bootloader mode; used by BOOT button. |
| **GPIO 3** | JTAG Strapping | Reserved | Strapping pin. |
| **GPIO 45, 46** | VDD_SPI & Boot Strapping | Reserved | Flash voltage and ROM logging strapping. |

---

## 3. Peripheral Pin Assignment Table

| Peripheral Module | Signal / Function | ESP32-S3 Pin | Expansion Board Location | Electrical Type | Safe Operating Voltage |
|---|---|---|---|---|---|
| **INMP441 Microphone** | SCK (Bit Clock) | **GPIO 15** | Left Header (IO15) | Digital Output from ESP32 | 3.3V |
| | WS (Word Select) | **GPIO 16** | Left Header (IO16) | Digital Output from ESP32 | 3.3V |
| | SD (Serial Data) | **GPIO 17** | Left Header (IO17) | Digital Input to ESP32 | 3.3V |
| | L/R (Channel) | **GND** | Left GND | Left Channel (GND) | GND |
| | VDD / GND | **3.3V / GND** | Left Power Rail | Power supply | 3.3V only (DO NOT connect to 5V) |
| **MAX98357A Amplifier** | BCLK (Bit Clock) | **GPIO 5** | Left Header (IO5) | Digital Output from ESP32 | 3.3V logic |
| | LRC (Word Select) | **GPIO 6** | Left Header (IO6) | Digital Output from ESP32 | 3.3V logic |
| | DIN (Data In) | **GPIO 7** | Left Header (IO7) | Digital Output from ESP32 | 3.3V logic |
| | GAIN | **Unconnected** (9dB) or **VIN** (6dB) | Left Power Rail | Sets 9dB (floating) or 6dB (VIN) sensitivity | 3.3V/5V/GND |
| | VIN / GND | **5V / GND** | Left 5V Power Rail | High current amp rail | 5.0V (from 5V regulator) |
| **ILI9341 2.8" SPI Display** | MOSI (SDI) | **GPIO 11** | Left Header (IO11) | SPI Output from ESP32 | 3.3V logic |
| | MISO (SDO) | **GPIO 13** | Left Header (IO13) | SPI Input to ESP32 | 3.3V logic |
| | SCK (CLK) | **GPIO 12** | Left Header (IO12) | SPI Output from ESP32 | 3.3V logic |
| | CS (Chip Select) | **GPIO 10** | Left Header (IO10) | Active LOW | 3.3V logic |
| | DC (Data/Cmd) | **GPIO 9** | Left Header (IO9) | Digital Output | 3.3V logic |
| | RESET | **GPIO 14** | Left Header (IO14) | Active LOW | 3.3V logic |
| | LED (Backlight) | **GPIO 4** | Left Header (IO4) | PWM (LEDC Channel 0) | 3.3V logic (Max 20mA) |
| | VCC / GND | **3.3V / GND** | Left Power Rail | Module Power | 3.3V (or 5V if onboard LDO exists) |
| **Touch Screen (XPT2046)** | T_CS (Chip Select)| **GPIO 8** | Left Header (IO8) | Active LOW | 3.3V logic *(UNVERIFIED)* |
| | T_IRQ (Interrupt) | **GPIO 18** | Left Header (IO18) | Active LOW | 3.3V logic *(UNVERIFIED)* |
| | T_CLK, T_DIN, T_DO| **GPIO 12, 11, 13** | Left Header | Shared SPI bus | 3.3V logic *(UNVERIFIED)* |
| **PN532 RFID Module** | TXD (to ESP32 RX)| **GPIO 1** | Right Header (IO1) | UART RX (UART1) | 3.3V logic |
| | RXD (from ESP32 TX)| **GPIO 2** | Right Header (IO2) | UART TX (UART1) | 3.3V logic |
| | VCC / GND | **3.3V or 5V / GND**| Right Power Rail | Module Power | 3.3V or 5V (Check DIP switch) |
| **Touch Sensor (PTT)** | OUT (Digital) | **GPIO 48** | Right Header (IO48) | Digital Input | 3.3V logic |
| | VCC / GND | **3.3V / GND** | Right Power Rail | Module Power | 3.3V |
| **Push Button: Vol Up** | Pin 1 to GPIO, Pin 2 to GND | **GPIO 41** | Right Header (IO41) | Active LOW (Internal Pull-Up) | 3.3V |
| **Push Button: Vol Down**| Pin 1 to GPIO, Pin 2 to GND | **GPIO 42** | Right Header (IO42) | Active LOW (Internal Pull-Up) | 3.3V |

---

## 4. Verification Status of Modules

- **ESP32-S3 N16R8 Pinout:** CONFIRMED via physical PCB inspection of V2775.
- **INMP441 Microphone Pinout:** CONFIRMED standard I2S.
- **MAX98357A Amplifier Pinout:** CONFIRMED standard I2S.
- **ILI9341 SPI Pinout:** CONFIRMED 4-wire hardware SPI.
- **PN532 UART Mode:** CONFIRMED standard High-Speed UART (DIP switch 00).
- **Volume Buttons & Digital Touch:** CONFIRMED.
- **XPT2046 Touch Controller:** **UNVERIFIED**. Marked unverified in firmware configuration until physical display module is verified with an XPT2046 IC on board.

# Hardware Wiring Guide

## 1. Safety Checklist Before Powering On

1. **Power Supply Check:** Set the adjustable adapter to **7.5V or 9V DC**. Check polarity with a multimeter: Center positive (+), outer sleeve negative (-).
2. **PSRAM Protection:** Ensure **GPIO 35, 36, and 37** have **NO wires attached**.
3. **Common Ground:** All modules must share a solid common GND through the expansion board.
4. **Logic Level:** Never apply 5V directly to INMP441, TTP223, or ILI9341 signal pins.

---

## 2. Step-by-Step Module Wiring

### 2.1 INMP441 I2S MEMS Microphone
| Microphone Pin | V2775 Board Header Pin | Wire Color / Recommendation |
|---|---|---|
| **VDD** | `3V3` (Red pin row on Left Header) | Red |
| **GND** | `GND` (Black pin row on Left Header) | Black |
| **SD** | `IO17` (Yellow pin row) | Blue |
| **WS** | `IO16` (Yellow pin row) | Green |
| **SCK** | `IO15` (Yellow pin row) | Yellow |
| **L/R** | `GND` (Sets Left Channel) | Black / Jumper to GND |

---

### 2.2 MAX98357A I2S Class-D Amplifier & 8Ω 2W Speaker
| Amplifier Pin | V2775 Board Header Pin | Wire Color / Recommendation |
|---|---|---|
| **VIN** | `5V` (Red pin row on Left Header or bottom 5V rail) | Red (Use 5V for high audio headroom) |
| **GND** | `GND` (Black pin row) | Black |
| **BCLK** | `IO5` (Yellow pin row) | Yellow |
| **LRC** | `IO6` (Yellow pin row) | Green |
| **DIN** | `IO7` (Yellow pin row) | Orange |
| **GAIN** | Leave Floating (9dB) or connect to VIN (6dB) | Datasheet Gain: Float=9dB, VIN=6dB, GND=12dB. 9dB/6dB recommended for headroom |
| **SD** | Leave floating (or pull-up to 3.3V) | Default active, right/left mix |
| **SPEAKER (+)** | Speaker red/positive terminal | Twisted pair |
| **SPEAKER (-)** | Speaker black/negative terminal | Twisted pair |

---

### 2.3 ILI9341 2.8" SPI Display (240x320)
| Display Pin | V2775 Board Header Pin | Description |
|---|---|---|
| **VCC** | `3V3` or `5V` (Check display module LDO) | Power supply |
| **GND** | `GND` | Ground |
| **CS** | `IO10` | Display SPI Chip Select |
| **RESET** | `IO14` | Display Hardware Reset |
| **DC / RS** | `IO9` | Data / Command Select |
| **MOSI / SDI** | `IO11` | SPI Master Out Slave In |
| **SCK / CLK** | `IO12` | SPI Clock |
| **LED / BL** | `IO4` | Backlight PWM control |
| **MISO / SDO** | `IO13` | SPI Master In Slave Out |

---

### 2.4 Touchscreen Controller (XPT2046) - UNVERIFIED
*Only wire if display has 4-wire resistive touch controller pins:*
| Touch Pin | V2775 Board Header Pin | Description |
|---|---|---|
| **T_CLK** | `IO12` | Shared SPI Clock |
| **T_CS** | `IO8` | Dedicated Touch Chip Select |
| **T_DIN** | `IO11` | Shared SPI MOSI |
| **T_DO** | `IO13` | Shared SPI MISO |
| **T_IRQ** | `IO18` | Touch pen interrupt |

---

### 2.5 PN532 RFID Module (UART Mode)
*First configure PN532 hardware switches: DIP switch `SEL0 = 0 (L)`, `SEL1 = 0 (L)`.*
| PN532 Pin | V2775 Board Header Pin | Description |
|---|---|---|
| **VCC** | `5V` or `3V3` | Module Power |
| **GND** | `GND` | Common Ground |
| **TXD** | `IO1` | Connects to ESP32 UART1 RX |
| **RXD** | `IO2` | Connects to ESP32 UART1 TX |

---

### 2.6 One-Channel Touch Sensor Module (Head Touch Interaction)
| Sensor Pin | ESP32-S3 Pin | Description |
|---|---|---|
| **VCC** | `3V3` | Power Supply |
| **GND** | `GND` | Ground |
| **SIG / OUT** | `IO38` | Digital Signal (High on touch) - Moved from IO48 due to Onboard WS2812 RGB LED |

---

### 2.7 Volume Buttons (Push Buttons)
| Button | Terminal 1 | Terminal 2 | Logic |
|---|---|---|---|
| **Volume Up** | `IO41` | `GND` | Active LOW (Firmware enables internal pull-up) |
| **Volume Down** | `IO42` | `GND` | Active LOW (Firmware enables internal pull-up) |

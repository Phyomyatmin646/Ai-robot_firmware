# 🤖 ESP32-S3 AI Robot Firmware (N16R8 + V2775)

Production-grade, memory-optimized, multi-threaded C++ firmware for the **ESP32-S3 AI Robot Desk Companion** featuring:
- **Processor:** ESP32-S3 Dual-Core Xtensa LX7 @ 240MHz
- **Memory:** 16MB Octal SPI Flash + 8MB Octal SPI PSRAM (OPI)
- **Carrier Board:** ESP32-S3 GPIO Extension Board V2775
- **Audio System:** INMP441 I2S MEMS Microphone (16kHz PCM capture) + MAX98357A I2S Class-D Amplifier + 8Ω 2W Speaker (24kHz Gemini Live playback)
- **Display & Touch:** ILI9341 2.8" SPI TFT (240×320) with 30 FPS cute animated robot avatar in PSRAM + TTP223 Push-to-Talk touch sensor
- **Card Reader:** PN532 High-Speed UART RFID module for interactive badges
- **Cloud Gateway:** Native RFC 6455 bidirectional WebSocket (`wss://107.23.154.150.sslip.io/ws/esp32`) streaming raw binary PCM frames with zero JSON wrapping overhead

---

## 1. Prerequisites & Development Environment

### 1.1 Required Software (Windows)
- **Windows 10 / 11 (64-bit)**
- **Antigravity IDE** or **Visual Studio Code**
- **Python 3.10+** (with PATH configured)
- **PlatformIO Core CLI 6.2.0+**
  - Binary location: `C:\Users\USER\.platformio\penv\Scripts\platformio.exe`

### 1.2 Windows PowerShell PATH Setup
To invoke `pio` directly in any PowerShell terminal:
```powershell
$env:Path += ";C:\Users\USER\.platformio\penv\Scripts"
pio --version
```

---

## 2. Hardware Pinout & Wiring Summary

> [!CAUTION]
> **Octal PSRAM Reserved Pins:** GPIO 33, 34, 35, 36, and 37 are strictly reserved for the 8MB Octal PSRAM. Never attach wires or peripherals to these pins!

| Subsystem | Peripheral | ESP32-S3 GPIO | V2775 Header Location | Notes |
|---|---|---|---|---|
| **Audio In** | INMP441 SCK (BCLK) | **GPIO 15** | Left Header (IO15) | I2S0 Bit Clock |
| | INMP441 WS (LRCLK) | **GPIO 16** | Left Header (IO16) | I2S0 Word Select |
| | INMP441 SD (Data Out) | **GPIO 17** | Left Header (IO17) | I2S0 Serial Data In |
| | INMP441 L/R | **GND** | Left GND | Left Channel (Mono) |
| **Audio Out** | MAX98357A BCLK | **GPIO 5** | Left Header (IO5) | I2S1 Bit Clock |
| | MAX98357A LRC (WS) | **GPIO 6** | Left Header (IO6) | I2S1 Word Select |
| | MAX98357A DIN | **GPIO 7** | Left Header (IO7) | I2S1 Data In |
| | MAX98357A GAIN | **GND** | Left GND | Sets 9 dB Gain (Protects 2W Speaker) |
| **Display** | ILI9341 MOSI | **GPIO 11** | Left Header (IO11) | SPI Data |
| | ILI9341 SCK | **GPIO 12** | Left Header (IO12) | SPI Clock (40MHz) |
| | ILI9341 MISO | **GPIO 13** | Left Header (IO13) | SPI Read |
| | ILI9341 CS | **GPIO 10** | Left Header (IO10) | Chip Select |
| | ILI9341 DC | **GPIO 9** | Left Header (IO9) | Data / Command |
| | ILI9341 RST | **GPIO 14** | Left Header (IO14) | Reset |
| | ILI9341 BL (Backlight) | **GPIO 4** | Left Header (IO4) | PWM Brightness (LEDC) |
| **RFID** | PN532 RXD | **GPIO 1** | Right Header (IO1) | UART1 RX (from PN532 TX) |
| | PN532 TXD | **GPIO 2** | Right Header (IO2) | UART1 TX (to PN532 RX) |
| **Sensors** | TTP223 Touch (Head) | **GPIO 38** | Left Header (IO38) | Digital Touch (Active HIGH) |
| **Buttons** | Volume Up Button | **GPIO 41** | Right Header (IO41) | Active LOW (Internal Pull-Up) |
| | Volume Down Button | **GPIO 42** | Right Header (IO42) | Active LOW (Internal Pull-Up) |

---

## 3. Build, Flash & Serial Monitor Commands

### 3.1 Build Firmware
Run in the `Firmware` root directory:
```powershell
& 'C:\Users\USER\.platformio\penv\Scripts\platformio.exe' run
```

### 3.2 Upload Firmware to ESP32-S3
Plug the ESP32-S3 via USB-C into your computer. Check Device Manager for the COM port (e.g. `COM3`), then run:
```powershell
& 'C:\Users\USER\.platformio\penv\Scripts\platformio.exe' run --target upload --upload-port COM3
```

### 3.3 Open Serial Monitor
```powershell
& 'C:\Users\USER\.platformio\penv\Scripts\platformio.exe' device monitor -b 115200
```

---

## 4. Device Registration & Provisioning Instructions

### Step 1: Register on Web Dashboard
1. Log in to your Web Dashboard at `https://107.23.154.150.sslip.io`.
2. Register your robot via the REST API or Dashboard UI:
   `POST /api/v1/devices/register`
3. Save the returned **Device ID** and **API Key** (API Key begins with `rbt_`).

### Step 2: Provision the ESP32
Open the Serial Monitor at 115200 baud and send:
```text
PROV:WIFI:MyHomeWiFi:MyPassword123
PROV:AUTH:6732948271049281a10b90c1:rbt_a8f902b1c8e7d654
PROV:STATUS
PROV:REBOOT
```

*Alternative (Wi-Fi SoftAP Portal):*
Long-press the Push-to-Talk touch sensor for 4 seconds. The robot screen will prompt you to connect to the `ESP32-Robot-Setup` Wi-Fi network and open `http://192.168.4.1` in your browser.

---

## 5. Project Structure

```
Firmware/
├── platformio.ini              # 16MB Flash, 8MB Octal PSRAM PlatformIO config
├── partitions_16MB.csv         # Custom 16MB partition map (6MB app, 6MB OTA)
├── include/
│   ├── config.h                # System timing, audio rates & NVS keys
│   ├── pins.h                  # Authoritative hardware pin definitions
│   ├── app_events.h            # FreeRTOS event types & state enums
│   ├── firmware_version.h      # Semantic versioning constants
│   └── secrets.example.h       # Credentials template
├── src/
│   ├── main.cpp                # Multi-task FreeRTOS scheduler & Serial CLI
│   ├── hardware/
│   │   ├── audio_input.cpp     # INMP441 I2S DMA microphone driver (16kHz PCM)
│   │   ├── audio_output.cpp    # MAX98357A I2S amplifier with digital volume limiter
│   │   ├── display.cpp         # LovyanGFX ILI9341 display & backlight driver
│   │   ├── touch.cpp           # TTP223 capacitive touch & long-press detector
│   │   ├── buttons.cpp         # Vol+/Vol- buttons with hardware debouncing
│   │   └── rfid.cpp            # PN532 UART RFID reader with cooldown logic
│   ├── network/
│   │   ├── wifi_manager.cpp    # Wi-Fi auto-reconnect & Captive Setup Portal
│   │   └── websocket_client.cpp# RFC 6455 WSS client with binary audio streaming
│   ├── services/
│   │   ├── config_manager.cpp  # NVS Preferences persistence & secret masking
│   │   ├── event_manager.cpp   # FreeRTOS inter-task event queue
│   │   ├── device_manager.cpp  # Robot state machine & voice stream coordinator
│   │   └── heartbeat.cpp       # 15s telemetry reporting service
│   └── ui/
│       └── ui_manager.cpp      # Cute animated 30 FPS robot face in Octal PSRAM
└── docs/                       # Comprehensive technical documentation
```

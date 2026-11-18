# Troubleshooting Guide

## 1. Boot, Flash & PSRAM Issues

### 1.1 PSRAM Not Detected
- **Symptom:** Serial Monitor reports `PSRAM: NOT DETECTED`.
- **Cause 1:** `platformio.ini` missing `board_build.arduino.memory_type = qio_opi` or `-DBOARD_HAS_PSRAM`.
- **Cause 2:** GPIO 33, 34, 35, 36, or 37 connected to external wires. These pins are internally connected to Octal PSRAM.
- **Fix:** Remove any jumper wires on GPIO 35, 36, 37. Ensure `board_build.flash_mode = qio` and `memory_type = qio_opi` are set.

### 1.2 Boot Loop / Brownout Detector Reset
- **Symptom:** Serial log repeatedly shows: `Brownout detector was triggered`.
- **Cause:** Power adapter cannot supply sudden Wi-Fi RF TX or amplifier current spikes.
- **Fix:**
  1. Use the adjustable DC adapter set to 7.5V–9V DC with at least 2A capacity.
  2. Ensure the two 100µF capacitors on the V2775 board are intact.
  3. Avoid powering the amplifier and speaker purely from a PC USB 2.0 port (500mA limit).

---

## 2. Wi-Fi & WebSocket Connection Issues

### 2.1 Handshake Rejected (HTTP 4001 or 4003)
- **Code 4001 (Unauthorized):** Device ID or API Key is missing.
  - *Fix:* Ensure device is registered via Web Dashboard and provisioned into NVS via Serial CLI.
- **Code 4003 (Forbidden):** Device credentials are invalid.
  - *Fix:* Re-register device or re-enter exact Device ID and API Key. Note that API key starts with `rbt_`.

### 2.2 TLS / SSL Handshake Failure
- **Symptom:** `[WS] Connection failed: SSL Handshake Failed`.
- **Cause:** System time not synchronized or root CA certificate rejected.
- **Fix:** The firmware synchronizes time via SNTP on Wi-Fi connection prior to establishing TLS. If self-signed certificates are in development, ensure root CA bundle is configured.

---

## 3. Audio Pipeline Issues

### 3.1 Audio Distortion / Crackling on Speaker
- **Cause:** Digital volume scaling clipping or I2S sample rate mismatch.
- **Fix:**
  - Verify MAX98357A `GAIN` pin is connected to GND (9 dB gain).
  - Backend Gemini Live streams at **24,000 Hz**. Ensure `AudioOutput` dynamically adjusts its sample rate upon receiving `audio:response:start`.

### 3.2 Microphone Capturing Only Silence / Noise
- **Cause:** INMP441 `L/R` pin floating or incorrect WS/SCK clock configuration.
- **Fix:**
  - Connect INMP441 `L/R` pin firmly to GND (selects Left Channel).
  - Ensure VDD is connected strictly to **3.3V** (INMP441 is not 5V tolerant).
  - In ESP-IDF I2S configuration, ensure `i2s_channel_fmt_t` is set to `I2S_CHANNEL_FMT_ONLY_LEFT`.

---

## 4. Display & Touch Issues

### 4.1 Display Shows White Screen
- **Cause:** SPI clock speed too high, incorrect CS/DC pins, or backlight pin disconnected.
- **Fix:**
  - Verify GPIO 4 (Backlight) is driven HIGH (or PWM initialized).
  - Verify SPI pins: MOSI = GPIO 11, SCK = GPIO 12, CS = GPIO 10, DC = GPIO 9, RST = GPIO 14.
  - Set SPI frequency to 27 MHz or 40 MHz max.

### 4.2 Touch Screen Not Responding
- **Cause:** Touch controller is unverified or lacks dedicated XPT2046 hardware.
- **Fix:**
  - Use the capacitive head-touch sensor (GPIO 38) as the primary tactile interaction method.
  - Check whether the physical 2.8" TFT board includes the XPT2046 IC chip before enabling resistive touch SPI.

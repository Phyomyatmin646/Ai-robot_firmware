# System Architecture & Firmware Design

## 1. High-Level Architecture Overview

The ESP32-S3 AI Robot Firmware is engineered as an event-driven, multithreaded embedded system leveraging FreeRTOS on the **ESP32-S3 dual-core Xtensa LX7 processor**.

```
+-------------------------------------------------------------------------+
|                                CORE 0                                   |
|  [Network & Communications]                                             |
|   - WiFiManager (Reconnection & Event handling)                         |
|   - WebSocketClient (RFC 6455 WSS client with TLS 1.3)                  |
|   - HeartbeatService (15s cadence, system telemetry)                    |
|   - DeviceManager & ConfigManager (NVS Preferences persistence)         |
+------------------------------------+------------------------------------+
                                     | Event Queue / FreeRTOS Queues
+------------------------------------+------------------------------------+
|                                CORE 1                                   |
|  [Hardware Control & Audio Pipeline]                                    |
|   - AudioInputTask (INMP441 I2S DMA -> 16kHz PCM stream to WS)          |
|   - AudioOutputTask (WS 24kHz PCM -> RingBuffer -> MAX98357A I2S DMA)   |
|   - UITask (LovyanGFX / TFT driver, cute robot avatar state machine)     |
|   - HardwareTask (Buttons Vol+/Vol-, TTP223 touch sensor, PN532 RFID)   |
+-------------------------------------------------------------------------+
```

---

## 2. Memory Architecture & 8MB Octal PSRAM Utilization

The ESP32-S3 N16R8 possesses:
- **Internal SRAM:** 512 KB (Fast SRAM for DMA buffers and time-critical ISRs).
- **External Octal SPI PSRAM (OPI):** 8 MB (Accessible with zero wait states when enabled with `board_build.arduino.memory_type = qio_opi`).
- **Internal Octal Flash:** 16 MB.

### PSRAM Allocation Strategy
1. **Audio Playback Ring Buffer:**
   - 24kHz 16-bit mono audio consumes 48,000 bytes/sec.
   - A 192 KB circular audio buffer is allocated in PSRAM using `ps_malloc()`, buffering up to 4 seconds of speech to prevent network jitter underruns.
2. **Display Framebuffer:**
   - ILI9341 display resolution is 240 × 320 at 16-bit RGB565 color = 153.6 KB per full frame.
   - Display single-buffer off-screen sprite (153.6 KB) is allocated directly in PSRAM using LovyanGFX `canvas.setPsram(true)`. Off-screen rendering prevents screen flicker and is transferred to the ILI9341 display controller via SPI DMA once per frame (~30 FPS).
3. **Internal SRAM Preservation:**
   - All I2S DMA descriptors are strictly kept in internal SRAM (DMA cannot target external PSRAM on ESP32-S3 directly without cache coherency considerations).

---

## 3. Robot State Machine

The firmware implements a strict 5-state finite state machine (FSM):

```
       +--------------------------------------------------+
       |                                                  |
       v                                                  |
  +---------+   Touch / Wake-Word   +-----------+         |
  |  IDLE   | --------------------> | LISTENING |         |
  +---------+                       +-----------+         |
       ^                                  |               |
       |                             Silence / audio:end  |
       |                                  v               |
       |                            +-----------+         |
       |                            | THINKING  |         |
       |                            +-----------+         |
       |                                  |               |
       |                             audio:response:start |
       |                                  v               |
       |   audio:response:end       +-----------+         |
       +--------------------------- | SPEAKING  | <-------+
             (or audio:cancel)      +-----------+
                                          |
                                    Fatal Error
                                          v
                                    +-----------+
                                    |   ERROR   |
                                    +-----------+
```

| State | UI Display Face | Status LED / Screen Indicator | Audio Pipeline Activity |
|---|---|---|---|
| **IDLE** | Happy blinking eyes | Cyan neutral accent | Microphone standby; Speaker silent |
| **LISTENING** | Wide attentive eyes, sound waves | Green listening bar | I2S Microphone streaming 16kHz PCM chunks |
| **THINKING** | Moving thinking eyes, spinner | Yellow animated dots | Mic stopped; awaiting backend response |
| **SPEAKING** | Cute animated talking mouth | Blue pulse wave | I2S Speaker streaming 24kHz PCM chunks |
| **ERROR** | Sad/worried eyes | Red alert bar | Displays error diagnostic message |

---

## 4. FreeRTOS Task Matrix

| Task Name | Core Affinity | Priority | Stack Size | Allocation | Purpose |
|---|---|---|---|---|---|
| **`net_task`** | Core 0 | Priority 3 | 8,192 bytes | SRAM | WebSocket event loop, reconnect backoff, TLS handshake |
| **`hb_task`** | Core 0 | Priority 1 | 4,096 bytes | SRAM | Periodic 15s telemetry packet transmission |
| **`audio_in`** | Core 1 | Priority 5 (High) | 6,144 bytes | SRAM | I2S DMA microphone capture, chunk packaging |
| **`audio_out`** | Core 1 | Priority 5 (High) | 6,144 bytes | SRAM | Ring buffer feeder to I2S amplifier DMA |
| **`ui_task`** | Core 1 | Priority 2 | 8,192 bytes | SRAM / PSRAM | 30 FPS avatar drawing, screen switching |
| **`sensor_task`** | Core 1 | Priority 2 | 4,096 bytes | SRAM | Button polling, TTP223 debouncing, PN532 RFID polling |

---

## 5. Inter-Task Communication & Synchronization

1. **`audio_in_queue`:** Carries 4096-byte raw PCM audio pointers between `audio_in` and `net_task`.
2. **`audio_out_rb`:** A lock-free circular ring buffer (192KB in PSRAM) written by `net_task` and drained by `audio_out`.
3. **`app_event_queue`:** Event bus transferring button clicks, RFID badge scans, state changes, and remote commands (`set_volume`, `set_brightness`).
4. **Mutexes:** Used exclusively around shared configuration objects (Device Config, Settings) to prevent race conditions during REST/WebSocket updates.

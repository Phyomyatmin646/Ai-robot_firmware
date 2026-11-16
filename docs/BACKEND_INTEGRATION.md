# Backend Integration Report

## 1. Executive Summary & Verification Context

This report provides the complete, authoritative integration specification between the **ESP32-S3 AI Robot Firmware** and the existing **Node.js Fastify Backend** (`https://github.com/Phyomyatmin646/Ai_Robot_BE`).

All details below are based on **direct source code inspection** of the backend repository, with discrepancies between existing Markdown documentation (`docs/firmware-integration.md`) and actual TypeScript source code explicitly resolved and documented.

---

## 2. Server Infrastructure & Endpoints

| Resource | Value / Endpoint | Protocol / Mechanism | Notes |
|---|---|---|---|
| **Production Base URL** | `https://107.23.154.150.sslip.io` | HTTPS (TLS 1.2/1.3) | Self-signed or Let's Encrypt TLS cert |
| **API Prefix** | `/api/v1` | REST (JSON) | Registered via Fastify |
| **System Health Check** | `GET https://107.23.154.150.sslip.io/health` | HTTP GET | Checks MongoDB, Redis, VoiceEngine |
| **Readiness Check** | `GET https://107.23.154.150.sslip.io/ready` | HTTP GET | Checks MongoDB connectivity |
| **Liveness Check** | `GET https://107.23.154.150.sslip.io/live` | HTTP GET | Fast health probe |
| **ESP32 Native WebSocket** | `wss://107.23.154.150.sslip.io/ws/esp32` | Native RFC 6455 WebSocket | Dedicated ESP32 bidirectional gateway |
| **Web Dashboard Gateway** | `https://107.23.154.150.sslip.io/socket.io/` | Socket.IO | Web Dashboard clients ONLY (not ESP32) |

---

## 3. Device Registration & Provisioning Flow

### 3.1 Architecture Rule
The ESP32 does **NOT** register itself anonymously via public REST endpoints. The registration endpoint is protected by User Authentication (`authenticate` JWT middleware).
The expected flow is:
1. User logs into Web Dashboard (obtains User JWT).
2. Web Dashboard registers the robot using `POST /api/v1/devices/register`.
3. Backend generates a MongoDB `deviceId` and a random API key (prefixed `rbt_`), stores SHA-256 hash of the API key in MongoDB, and returns the raw API key **once**.
4. User provisions the ESP32 (via USB Serial CLI or local Provisioning Web Portal) with:
   - Wi-Fi SSID
   - Wi-Fi Password
   - Device ID
   - Device API Key
5. ESP32 stores credentials in Non-Volatile Storage (NVS) via ESP32 `Preferences`.
6. ESP32 establishes a secure TLS connection to `wss://107.23.154.150.sslip.io/ws/esp32`.

### 3.2 Registration Schema (`POST /api/v1/devices/register`)
- **Headers:** `Authorization: Bearer <USER_JWT>`
- **Request Body (JSON):**
  ```json
  {
    "name": "My Robot Desk Companion",
    "deviceType": "esp32-s3",
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "firmwareVersion": "1.0.0",
    "hardwareVersion": "ESP32-S3-WROOM-1",
    "capabilities": {
      "hasMicrophone": true,
      "hasSpeaker": true,
      "hasDisplay": true,
      "hasTouchSensor": true,
      "hasLED": false,
      "hasVolumeButtons": true,
      "maxSampleRate": 48000,
      "supportedCodecs": ["pcm_16bit"]
    }
  }
  ```
- **Response (HTTP 201 Created):**
  ```json
  {
    "success": true,
    "message": "Device registered successfully. Save the apiKey securely; it will not be shown again.",
    "data": {
      "device": {
        "id": "6732948271049281a10b90c1",
        "userId": "6732948271049281a10b90b0",
        "name": "My Robot Desk Companion",
        "deviceType": "esp32-s3",
        "status": "offline",
        "firmwareVersion": "1.0.0",
        "hardwareVersion": "ESP32-S3-WROOM-1",
        "macAddress": "AA:BB:CC:DD:EE:FF"
      },
      "apiKey": "rbt_9f8e7d6c5b4a3..."
    }
  }
  ```

---

## 4. ESP32 Native WebSocket Protocol (`/ws/esp32`)

### 4.1 Handshake & Authentication
- **Transport:** Native RFC 6455 over WSS (TLS).
- **Authentication Headers:**
  - `X-Device-ID: <deviceId>`
  - `X-Device-Key: <apiKey>`
  *(Query parameter fallback is also supported: `/ws/esp32?deviceId=...&key=...`)*
- **Handshake Rejection Codes:**
  - `4001`: Missing credentials (neither header nor query parameters provided).
  - `4003`: Invalid credentials (device ID not found or API key SHA-256 hash mismatch).

### 4.2 Initial Server Handshake Response (`device:connected`)
Immediately after authentication succeeds, the backend server pushes a JSON text frame to the ESP32 with the Source of Truth settings queried directly from MongoDB `device_settings`:
```json
{
  "event": "device:connected",
  "timestamp": 1731345600000,
  "data": {
    "deviceId": "6732948271049281a10b90c1",
    "serverTime": 1731345600000,
    "sampleRate": 16000,
    "chunkSize": 4096,
    "heartbeatInterval": 15,
    "volume": 80,
    "brightness": 80,
    "isMuted": false
  }
}
```
The firmware uses this payload to configure its heartbeat timer and synchronize its initial audio volume and display brightness with the cloud settings immediately upon connecting.

---

## 5. Bidirectional Audio Streaming Specification

### 5.1 Outgoing Audio (ESP32 Microphone → Backend)
- **Format:** Uncompressed Linear PCM, 16-bit signed, little-endian, mono.
- **Sampling Rate:** 16,000 Hz (16 kHz).
- **Transmission Mode:** **Raw Binary WebSocket Frames (opcode `0x2`)**. Zero JSON encapsulation overhead!
- **Chunk Size:** 4,096 bytes (representing 128 ms of 16kHz 16-bit mono audio).
- **Session Control Events (JSON text frames):**
  - **Start Session:**
    ```json
    {
      "event": "audio:start",
      "data": {
        "sessionId": "voice_esp32_1731345610000",
        "format": "pcm_16bit",
        "sampleRate": 16000
      }
    }
    ```
    *(Note: If raw PCM frames arrive without an active session, the backend automatically initializes an active session).*
  - **End Session:**
    ```json
    {
      "event": "audio:end",
      "data": {
        "totalChunks": 24
      }
    }
    ```

### 5.2 Incoming Audio (Backend / Gemini Live → ESP32 Speaker)
- **Control Notification (`audio:response:start`):**
  Sent before streaming response audio:
  ```json
  {
    "event": "audio:response:start",
    "timestamp": 1731345615000,
    "data": {
      "sessionId": "voice_esp32_1731345610000",
      "format": "pcm_16bit",
      "sampleRate": 24000,
      "channels": 1
    }
  }
  ```
  **CRITICAL NOTE ON SAMPLE RATE:**
  Gemini Live outputs **24,000 Hz (24 kHz) PCM mono**. The ESP32 I2S DAC clock must be dynamically adjusted to 24 kHz for playback upon receiving `audio:response:start`, or resampled locally.
- **Audio Payload:**
  Streamed as **Raw Binary WebSocket Frames (opcode `0x2`)** directly to the ESP32.
- **Completion Notification (`audio:response:end`):**
  ```json
  {
    "event": "audio:response:end",
    "timestamp": 1731345620000,
    "data": {
      "sessionId": "voice_esp32_1731345610000",
      "duration": 4820
    }
  }
  ```
- **Interruption / Barge-in (`audio:cancel`):**
  Sent when user starts speaking or interruption is triggered:
  ```json
  {
    "event": "audio:cancel",
    "timestamp": 1731345618000,
    "data": {
      "reason": "interruption"
    }
  }
  ```
  **ESP32 Action:** Stop I2S output DMA immediately, clear playback ring buffer, and return to listening/idle state.

---

## 6. Telemetry & Hardware Control Events

### 6.1 Heartbeat Telemetry (ESP32 → Backend)
Sent every `heartbeatInterval` (default 15 seconds):
```json
{
  "event": "device:heartbeat",
  "data": {
    "freeHeap": 218492,
    "uptimeSeconds": 1420,
    "wifiRSSI": -58,
    "cpuTemp": 42.5,
    "batteryLevel": 100
  }
}
```
Backend relays this to Web Dashboard via `dashboardBroadcast.emitDeviceHeartbeat`.

### 6.2 Capacitive Touch Sensor (ESP32 → Backend)
```json
{
  "event": "touch:event",
  "data": {
    "pad": 0,
    "pressed": true
  }
}
```
In backend `esp32.gateway.ts`:
- If `pressed == true` and `session.state == 'idle'`, the backend automatically triggers `session.state = 'listening'` and notifies dashboard Socket.IO.

### 6.3 Hardware Commands (Backend → ESP32)
Dispatched when dashboard user changes device volume or brightness (`PATCH /api/v1/settings/device/:deviceId`):
```json
{
  "event": "robot:command",
  "timestamp": 1731345625000,
  "data": {
    "commandId": "cmd_1731345625000",
    "type": "set_volume",
    "payload": {
      "volume": 75
    }
  }
}
```
Supported Command Types in Backend:
- `set_volume`: Payload `{ volume: number (0..100) }`
- `set_brightness`: Payload `{ brightness: number (0..100) }`

---

## 7. Discrepancies & Backend Integration Gaps Identified

1. **Gemini Live Output Sample Rate (24 kHz vs 16 kHz):**
   - Documentation in some sections suggested 16 kHz symmetric audio, but the actual Gemini Live client (`gemini-live.client.ts` line 186) delivers **24,000 Hz 16-bit PCM**. The firmware must support dynamic I2S sample rate reconfiguration (16 kHz for input, 24 kHz for output).
2. **PN532 RFID Handling (Backend Gap):**
   - The backend currently possesses **no RFID database model, WebSocket handler, or dashboard event**. If ESP32 sends an RFID event under `sensor:update`, the backend simply logs it at `debug` level (`logger.debug({ deviceId, data: payload })`) and discards it.
   - *Recommendation:* Firmware will emit RFID card read events as `{ "event": "sensor:update", "data": { "rfidUid": "...", "cardType": "..." } }`, and we provide the exact backend patch proposal in this documentation.
3. **Volume Button Sync (Backend Gap):**
   - Physical volume button presses change local volume immediately on the ESP32. Firmware sends `{ "event": "sensor:update", "data": { "volumeButtonState": "up", "currentVolume": 85 } }`. As above, backend logs this at debug level.
4. **WebSocket Native vs Socket.IO:**
   - ESP32 communicates exclusively via native RFC 6455 WebSocket (`/ws/esp32`). The Web Dashboard communicates via Socket.IO (`/socket.io`). The backend is the central bridge.

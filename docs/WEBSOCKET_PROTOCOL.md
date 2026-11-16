# Native WebSocket RFC 6455 Protocol Specification

## 1. Connection Endpoint & Security

- **Server URL:** `wss://107.23.154.150.sslip.io/ws/esp32`
- **Port:** `443` (TLS 1.2 / TLS 1.3)
- **Protocol:** RFC 6455 Native WebSocket (Direct binary and text framing).
- **Authentication:**
  - Header: `X-Device-ID: <MongoDB_Device_ID>`
  - Header: `X-Device-Key: <Raw_API_Key>`
- **Closure / Error Codes:**
  - `4001`: Unauthorized — Missing device ID or API Key in headers.
  - `4003`: Forbidden — Invalid device credentials or SHA-256 hash mismatch.

---

## 2. Inbound Frames (Backend → ESP32)

### 2.1 JSON Text Frames

#### Event: `device:connected`
Dispatched by backend immediately upon valid handshake:
```json
{
  "event": "device:connected",
  "timestamp": 1731345600000,
  "data": {
    "deviceId": "6732948271049281a10b90c1",
    "serverTime": 1731345600000,
    "sampleRate": 16000,
    "chunkSize": 4096,
    "heartbeatInterval": 15
  }
}
```

#### Event: `audio:response:start`
Notifies firmware that Gemini Live has generated an audio response:
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
*Firmware Behavior:* Configures I2S DAC clock to 24 kHz, clears playback buffer, sets robot state to `SPEAKING`.

#### Event: `audio:response:end`
Notifies firmware that AI audio response stream is complete:
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
*Firmware Behavior:* Plays remaining buffered audio in ring buffer, then transitions state to `IDLE`.

#### Event: `audio:cancel`
Dispatched on barge-in interruption (user started speaking during playback):
```json
{
  "event": "audio:cancel",
  "timestamp": 1731345618000,
  "data": {
    "reason": "interruption"
  }
}
```
*Firmware Behavior:* Immediately mutes I2S DAC, discards all queued audio in ring buffer, sets state to `LISTENING`.

#### Event: `robot:command`
Dispatched when dashboard user changes volume or display brightness:
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
Supported types:
- `set_volume` with payload `{ "volume": 0..100 }`
- `set_brightness` with payload `{ "brightness": 0..100 }`

---

### 2.2 Binary Frames
- **Opcode:** `0x2` (Binary Frame)
- **Content:** Raw Linear PCM 16-bit signed, little-endian, mono, 24 kHz.
- **Handling:** Written directly to circular audio ring buffer in PSRAM.

---

## 3. Outbound Frames (ESP32 → Backend)

### 3.1 Binary Frames
- **Opcode:** `0x2` (Binary Frame)
- **Content:** Raw Linear PCM 16-bit signed, little-endian, mono, 16 kHz.
- **Chunk Size:** 4,096 bytes (128 ms).
- **Handling:** Sent directly from microphone DMA capture task over WebSocket without JSON wrapping.

---

### 3.2 JSON Text Frames

#### Event: `device:heartbeat`
Sent every 15 seconds:
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

#### Event: `touch:event`
Sent on capacitive touch sensor press/release:
```json
{
  "event": "touch:event",
  "data": {
    "pad": 0,
    "pressed": true
  }
}
```

#### Event: `audio:start`
Sent when voice recording begins (Touch button press or Push-to-Talk):
```json
{
  "event": "audio:start",
  "data": {
    "sessionId": "voice_6732948271049281a10b90c1_1731345610000",
    "format": "pcm_16bit",
    "sampleRate": 16000
  }
}
```

#### Event: `audio:end`
Sent when voice recording ends (Touch button release or silence timeout):
```json
{
  "event": "audio:end",
  "data": {
    "totalChunks": 24
  }
}
```

#### Event: `sensor:update`
Sent when physical buttons or RFID badge are triggered:
```json
{
  "event": "sensor:update",
  "data": {
    "volumeButtonState": "up",
    "currentVolume": 85
  }
}
```
*(For RFID badges):*
```json
{
  "event": "sensor:update",
  "data": {
    "rfidUid": "A1B2C3D4",
    "cardType": "MIFARE_CLASSIC"
  }
}
```

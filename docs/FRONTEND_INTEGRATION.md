# Frontend & Dashboard Integration Specification

## 1. Overview & Architectural Role

The Web Dashboard client is a separate user-facing web application (e.g., React, Next.js, or Vue). It does **NOT** communicate directly with the ESP32-S3 over local network or direct WebSocket. 

Instead:
- The **Web Dashboard** interacts with the **Node.js Fastify Backend** using **REST APIs (HTTPS)** and **Socket.IO (WSS)**.
- The **ESP32-S3 Firmware** interacts with the **Node.js Fastify Backend** using **Native RFC 6455 WebSocket (WSS)**.
- The **Backend acts as the bidirectional message broker and state synchronizer**.

```
+--------------------+                       +----------------------+
|   Web Dashboard    |                       |   ESP32-S3 Robot     |
|   (React/Next.js)  |                       |   (Hardware Desk Bot)|
+---------+----------+                       +----------+-----------+
          |                                             |
   REST   | Socket.IO                        Native     |
  (JWT)   | (Rooms)                         RFC 6455    | (Device Key)
          v                                             v
+---------------------------------------------------------------+
|                    Node.js Fastify Backend                    |
|             (https://107.23.154.150.sslip.io)                |
+---------------------------------------------------------------+
```

---

## 2. Authentication Requirements

1. **User Authentication:**
   - Endpoint: `POST /api/v1/auth/login`
   - Response: Returns a JWT Bearer token `{ token: "..." }`.
   - Used in all subsequent REST requests: `Authorization: Bearer <USER_JWT>`
   - Used in Socket.IO handshake auth: `{ auth: { token: "<USER_JWT>" } }`.

2. **Device Registration:**
   - The user must register the robot from the dashboard:
     - `POST /api/v1/devices/register`
     - Body:
       ```json
       {
         "name": "Desk Robot",
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
     - Response provides `deviceId` and raw `apiKey`. The dashboard displays these to the user so they can provision the robot.

---

## 3. Web Dashboard Socket.IO Integration

### 3.1 Connection & Handshake
- **URL:** `wss://107.23.154.150.sslip.io`
- **Path:** `/socket.io/`
- **Handshake Authentication:**
  ```javascript
  import { io } from 'socket.io-client';

  const socket = io('https://107.23.154.150.sslip.io', {
    auth: {
      token: userJwtToken, // From auth login
    },
    transports: ['websocket', 'polling'],
  });
  ```

### 3.2 Room Subscription
Upon connection, the backend automatically places the socket in `user:<userId>`.
To receive device-specific real-time telemetry, the frontend emits:
```javascript
socket.emit('device:subscribe', deviceId);
```
To unsubscribe when switching views:
```javascript
socket.emit('device:unsubscribe', deviceId);
```

### 3.3 Dashboard Events Received

| Event Name | Source | Payload Interface | Description |
|---|---|---|---|
| `device:status` | Backend | `{ deviceId: string, status: "online" \| "offline", state: string }` | Robot online/offline state |
| `device:heartbeat` | ESP32 via Backend | `{ deviceId: string, timestamp: number, freeHeap: number, uptimeSeconds: number, wifiRSSI: number, cpuTemp?: number, batteryLevel?: number }` | Real-time health & heap metrics |
| `robot:state` | Backend / AI | `{ deviceId: string, state: "idle" \| "listening" \| "thinking" \| "speaking" \| "error", sessionId?: string, timestamp: number }` | Animated avatar state |
| `robot:touch` | ESP32 via Backend | `{ deviceId: string, pad: number, pressed: boolean, timestamp: number }` | Push-to-talk touch event |
| `audio:transcription`| Gemini Live | `{ sessionId: string, text: string, isFinal: boolean, speaker: "user" \| "assistant" }` | Real-time text speech transcript |
| `settings:updated` | Backend | `{ deviceId: string, settings: Partial<DeviceSettings> }` | Remote settings synchronization |

---

## 4. Hardware Control REST Endpoints (Dashboard → ESP32)

### 4.1 Volume Control
- **Endpoint:** `PATCH /api/v1/settings/device/:deviceId`
- **Headers:** `Authorization: Bearer <USER_JWT>`
- **Payload:**
  ```json
  {
    "sound": {
      "masterVolume": 85
    }
  }
  ```
- **Backend Behavior:** Backend updates database and immediately dispatches native WebSocket event `robot:command` (`set_volume`) to the online ESP32.

### 4.2 Display Brightness
- **Endpoint:** `PATCH /api/v1/settings/device/:deviceId`
- **Headers:** `Authorization: Bearer <USER_JWT>`
- **Payload:**
  ```json
  {
    "display": {
      "brightness": 90
    }
  }
  ```
- **Backend Behavior:** Backend updates database and immediately dispatches `robot:command` (`set_brightness`) to the online ESP32.

---

## 5. Complete Frontend Example Implementation

```typescript
import { useEffect, useState } from 'react';
import { io, Socket } from 'socket.io-client';

interface RobotTelemetry {
  status: 'online' | 'offline';
  state: 'idle' | 'listening' | 'thinking' | 'speaking' | 'error';
  freeHeap: number;
  uptimeSeconds: number;
  wifiRSSI: number;
  transcription: string;
}

export function useRobotDashboard(deviceId: string, userToken: string) {
  const [telemetry, setTelemetry] = useState<RobotTelemetry>({
    status: 'offline',
    state: 'idle',
    freeHeap: 0,
    uptimeSeconds: 0,
    wifiRSSI: 0,
    transcription: '',
  });

  useEffect(() => {
    const socket: Socket = io('https://107.23.154.150.sslip.io', {
      auth: { token: userToken },
      transports: ['websocket'],
    });

    socket.on('connect', () => {
      console.log('Connected to Backend Socket.IO');
      socket.emit('device:subscribe', deviceId);
    });

    socket.on('device:status', (data) => {
      if (data.deviceId === deviceId) {
        setTelemetry((prev) => ({ ...prev, status: data.status }));
      }
    });

    socket.on('device:heartbeat', (data) => {
      if (data.deviceId === deviceId) {
        setTelemetry((prev) => ({
          ...prev,
          freeHeap: data.freeHeap,
          uptimeSeconds: data.uptimeSeconds,
          wifiRSSI: data.wifiRSSI,
        }));
      }
    });

    socket.on('robot:state', (data) => {
      if (data.deviceId === deviceId) {
        setTelemetry((prev) => ({ ...prev, state: data.state }));
      }
    });

    socket.on('audio:transcription', (data) => {
      setTelemetry((prev) => ({
        ...prev,
        transcription: `[${data.speaker}]: ${data.text}`,
      }));
    });

    return () => {
      socket.emit('device:unsubscribe', deviceId);
      socket.disconnect();
    };
  }, [deviceId, userToken]);

  const setVolume = async (volume: number) => {
    await fetch(`https://107.23.154.150.sslip.io/api/v1/settings/device/${deviceId}`, {
      method: 'PATCH',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${userToken}`,
      },
      body: JSON.stringify({ sound: { masterVolume: volume } }),
    });
  };

  const setBrightness = async (brightness: number) => {
    await fetch(`https://107.23.154.150.sslip.io/api/v1/settings/device/${deviceId}`, {
      method: 'PATCH',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${userToken}`,
      },
      body: JSON.stringify({ display: { brightness } }),
    });
  };

  return { telemetry, setVolume, setBrightness };
}
```

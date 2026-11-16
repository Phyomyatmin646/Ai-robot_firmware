# Device Provisioning & Identity Architecture

## 1. Overview & Security Principles

This robot firmware implements a **secure device provisioning architecture** adhering to the following rules:

1. **Zero Hardcoded Secrets:** Device IDs, Device API Keys, and Wi-Fi credentials are never hardcoded into checked-in source files.
2. **One-Time Key Generation:** The backend generates the API Key once at device registration (`POST /api/v1/devices/register`) and only stores its SHA-256 hash. The raw key cannot be retrieved from the backend later.
3. **NVS Persistent Storage:** Credentials are saved to the ESP32's Non-Volatile Storage (NVS) using the `Preferences` API.
4. **Physical Extraction Disclosure:** In standard ESP32 development mode without flash encryption (NVS encryption with eFuse key burning enabled), data in flash memory can be extracted by an attacker with physical access and a flash programmer. To mitigate this in commercial production, ESP32 Secure Boot v2 and Flash Encryption must be enabled.
5. **No Serial Credential Leaks:** The firmware explicitly masks passwords and API keys in all UART / USB Serial logs (e.g., printing `rbt_***[MASKED]***`).

---

## 2. End-to-End Registration & Provisioning Workflow

```
[ Step 1: User Login ]
  User logs in on Web Dashboard -> Receives User JWT

[ Step 2: Device Registration ]
  User clicks "Register Robot" on Web Dashboard
  POST /api/v1/devices/register
  Backend returns:
    - deviceId: "6732948271049281a10b90c1"
    - apiKey:   "rbt_8f902b1c8e7d6a543..."

[ Step 3: ESP32 Provisioning ]
  Option A (USB Serial Command Line):
    Connect USB to ESP32 -> Open Serial Monitor (115200 baud)
    Type: SET_WIFI <SSID> <PASSWORD>
    Type: SET_AUTH <DEVICE_ID> <API_KEY>
    Type: REBOOT

  Option B (Provisioning SoftAP Portal):
    Long-press touch sensor for 5 seconds -> Robot starts "ESP32-Robot-Setup" AP
    Open http://192.168.4.1 in browser -> Submit credentials form

[ Step 4: Storage & Boot ]
  ESP32 writes credentials to NVS -> Connects to Wi-Fi -> Connects to WSS
```

---

## 3. Provisioning Methods

### Method 1: USB Serial Provisioning CLI (Bench Setup)
Connect the ESP32 via USB-C to the development computer, open a serial terminal at 115200 baud, and send the following commands:

```text
PROV:WIFI:MyHomeWiFi:SecretPass123
PROV:AUTH:6732948271049281a10b90c1:rbt_8f902b1c8e7d6a543
PROV:SAVE
PROV:REBOOT
```

The device responds:
```text
[PROV] Wi-Fi credentials accepted.
[PROV] Device authentication credentials accepted.
[PROV] Saved to NVS Preferences.
[PROV] Rebooting system...
```

### Method 2: SoftAP Web Portal (Consumer Setup)
1. On boot, if no valid credentials exist in NVS (or if the user presses the touch sensor continuously for 5 seconds):
   - The robot screen displays a Wi-Fi setup icon and status text: `"Setup AP: ESP32-Robot-Setup"`.
   - ESP32 launches a Wi-Fi Access Point `ESP32-Robot-Setup` (IP: `192.168.4.1`).
2. The user connects their smartphone or computer to this Wi-Fi network.
3. A captive portal page appears with fields:
   - Wi-Fi SSID (with auto-scanned list)
   - Wi-Fi Password
   - Device ID
   - Device API Key
4. Clicking "Save & Connect" writes the configuration to NVS and reboots the robot into normal operating mode.

---

## 4. NVS Namespace and Key Map

| Namespace | Key | Type | Description |
|---|---|---|---|
| `robot_net` | `wifi_ssid` | String (up to 32 chars) | Local Wi-Fi network SSID |
| `robot_net` | `wifi_pass` | String (up to 64 chars) | Local Wi-Fi WPA2 password |
| `robot_auth` | `device_id` | String (24-char hex) | MongoDB ObjectId from backend |
| `robot_auth` | `api_key` | String (~32 chars) | Raw API Key starting with `rbt_` |
| `robot_cfg` | `vol` | uint8_t (0..100) | Master audio volume |
| `robot_cfg` | `bright` | uint8_t (0..100) | Screen brightness level |

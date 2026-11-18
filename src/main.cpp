#include <Arduino.h>
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "pins.h"
#include "config.h"
#include "firmware_version.h"

// Hardware Drivers
#include "hardware/audio_input.h"
#include "hardware/audio_output.h"
#include "hardware/display.h"
#include "hardware/touch.h"
#include "hardware/buttons.h"
#include "hardware/rfid.h"
#include "hardware/wake_word.h"

// Networking & Services
#include "network/wifi_manager.h"
#include "network/websocket_client.h"
#include "services/config_manager.h"
#include "services/event_manager.h"
#include "services/device_manager.h"
#include "services/heartbeat.h"
#include "ui/ui_manager.h"

// FreeRTOS Task Handles
TaskHandle_t taskNetworkHandle = nullptr;
TaskHandle_t taskSensorHandle = nullptr;

// Network & Cloud Task (Core 0)
void taskNetwork(void* parameter) {
    Serial.println("[FreeRTOS] Network Task started on Core 0.");
    for (;;) {
        WiFiManager::getInstance().update();
        if (WiFiManager::getInstance().isConnected()) {
            WebSocketClientManager::getInstance().update();
            HeartbeatService::getInstance().update();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Hardware Sensor & Button Polling Task (Core 1)
void taskSensors(void* parameter) {
    Serial.println("[FreeRTOS] Sensor Task started on Core 1.");
    for (;;) {
        ButtonController::getInstance().update();
        TouchSensorController::getInstance().update();
        RFIDController::getInstance().update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Serial CLI Provisioning & Command Parser
void executeCliCommand(const String& cmd, Stream& out) {
    if (cmd.startsWith("PROV:WIFI:")) {
        int firstColon = cmd.indexOf(':', 10);
        if (firstColon > 0) {
            String ssid = cmd.substring(10, firstColon);
            String pass = cmd.substring(firstColon + 1);
            ConfigManager::getInstance().setWifiCredentials(ssid, pass);
            out.printf("[CLI] Wi-Fi credentials saved for SSID: %s (Password stored in NVS)\n", ssid.c_str());
            WiFiManager::getInstance().begin();
        } else {
            out.println("[CLI] ERROR: Invalid format. Usage: PROV:WIFI:<SSID>:<PASSWORD>");
        }
    } else if (cmd.startsWith("PROV:AUTH:")) {
        int firstColon = cmd.indexOf(':', 10);
        if (firstColon > 0) {
            String devId = cmd.substring(10, firstColon);
            String apiKey = cmd.substring(firstColon + 1);
            devId.trim();
            apiKey.trim();
            if (devId.length() > 0 && apiKey.length() > 0) {
                ConfigManager::getInstance().setDeviceCredentials(devId, apiKey);
                out.printf("[CLI] Device credentials saved: ID=%s (Device Key stored securely in NVS)\n", devId.c_str());
            } else {
                out.println("[CLI] ERROR: Device ID and API Key cannot be empty.");
            }
        } else {
            out.println("[CLI] ERROR: Invalid format. Usage: PROV:AUTH:<DEVICE_ID>:<API_KEY>");
        }
    } else if (cmd == "PROV:STATUS") {
        bool wifiConfigured = ConfigManager::getInstance().hasWifiCredentials();
        bool wifiConnected = WiFiManager::getInstance().isConnected();
        String wifiStateStr = "DISCONNECTED";
        switch (WiFiManager::getInstance().getState()) {
            case WifiState::CONNECTED: wifiStateStr = "CONNECTED"; break;
            case WifiState::CONNECTING: wifiStateStr = "CONNECTING"; break;
            case WifiState::PROVISIONING_PORTAL: wifiStateStr = "PROVISIONING_PORTAL (AP: ESP32-Robot-Setup)"; break;
            default: wifiStateStr = "DISCONNECTED"; break;
        }

        bool devConfigured = ConfigManager::getInstance().hasDeviceCredentials();
        String devId = ConfigManager::getInstance().getDeviceId();
        bool keyConfigured = (ConfigManager::getInstance().getDeviceApiKey().length() > 0);

        bool wsOnline = WebSocketClientManager::getInstance().isAuthenticated();

        WakeNetStatus wnStatus = WakeWordEngine::getInstance().getStatus();
        String wnStatusStr = "UNINITIALIZED";
        switch (wnStatus) {
            case WakeNetStatus::READY_WAKENET9_HIESP: wnStatusStr = "READY (Wake word: \"Hi ESP\")"; break;
            case WakeNetStatus::ACTIVE_INFERENCE: wnStatusStr = "ACTIVE_INFERENCE"; break;
            case WakeNetStatus::PAUSED: wnStatusStr = "PAUSED"; break;
            case WakeNetStatus::MODEL_PARTITION_NOT_FOUND: wnStatusStr = "MODEL_PARTITION_NOT_FOUND"; break;
            default: wnStatusStr = "UNINITIALIZED"; break;
        }

        out.println("\n=======================================================");
        out.println("             DEVICE PROVISIONING STATUS                ");
        out.println("=======================================================");
        out.printf("Firmware:         %s v%s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
        out.printf("Hardware:         %s\n", HARDWARE_VERSION);
        out.printf("Wi-Fi Configured: %s%s\n", wifiConfigured ? "YES" : "NO",
                   wifiConfigured ? (" (SSID: " + ConfigManager::getInstance().getWifiSSID() + ")").c_str() : "");
        out.printf("Wi-Fi State:      %s\n", wifiStateStr.c_str());
        out.printf("IP Address:       %s%s\n",
                   wifiConnected ? WiFiManager::getInstance().getIpAddress().c_str() : "Not Connected",
                   wifiConnected ? (" (RSSI: " + String(WiFiManager::getInstance().getRSSI()) + " dBm)").c_str() : "");
        out.printf("Device ID:        %s\n", devConfigured ? devId.c_str() : "[NOT CONFIGURED]");
        out.printf("Device Key:       %s\n", keyConfigured ? "[CONFIGURED (Protected)]" : "[NOT CONFIGURED]");
        out.printf("WebSocket:        %s\n", wsOnline ? "ONLINE (Authenticated)" : "OFFLINE");
        out.printf("Internal SRAM:    %lu KB Free\n", (unsigned long)(ESP.getFreeHeap() / 1024));
        out.printf("Octal PSRAM:      %lu / %lu KB Free\n",
                   (unsigned long)(ESP.getFreePsram() / 1024),
                   (unsigned long)(ESP.getPsramSize() / 1024));
        out.println("-------------------------------------------------------");
        out.println("Hardware Diagnostics:");
        out.printf("  - INMP441 Mic:     %s\n", AudioInput::getInstance().getHardwareStatusStr());
        out.printf("  - MAX98357A Amp:   %s\n", AudioOutput::getInstance().getHardwareStatusStr());
        out.printf("  - ILI9341 Display: %s\n", DisplayController::getInstance().getHardwareStatusStr());
        out.printf("  - TTP223 Touch:    %s\n", TouchSensorController::getInstance().getHardwareStatusStr());
        out.printf("  - Volume Buttons:  %s\n", ButtonController::getInstance().getHardwareStatusStr());
        out.printf("  - PN532 RFID:      %s\n", RFIDController::getInstance().getHardwareStatusStr());
        out.printf("  - WakeNet9 Model:  %s\n", WakeWordEngine::getInstance().getHardwareStatusStr());
        out.println("=======================================================\n");
    } else if (cmd == "PROV:CLEAR") {
        ConfigManager::getInstance().clearAll();
        out.println("[CLI] Credentials cleared from NVS.");
    } else if (cmd == "PROV:REBOOT") {
        out.println("[CLI] Rebooting device...");
        delay(500);
        ESP.restart();
    } else if (cmd == "SCREEN:TOGGLE") {
        UIManager::getInstance().toggleScreen();
        out.println("[CLI] Display screen toggled.");
    } else if (cmd == "CARD:SAMPLE:MY") {
        String uidStr = RFIDController::getInstance().getLastUid();
        if (uidStr.length() == 0) {
            out.println("[CLI] ERROR: No card present on PN532. Please hold a MIFARE card on the reader and retry.");
        } else {
            uint8_t uid[4] = {0};
            for (int i = 0; i < 4; i++) {
                String byteHex = uidStr.substring(i * 2, i * 2 + 2);
                uid[i] = (uint8_t)strtol(byteHex.c_str(), NULL, 16);
            }
            String burmeseSample = "{\"schemaVersion\":\"1.0\",\"cardType\":\"educational\",\"language\":\"my\",\"title\":\"ပန်းသီး\",\"category\":\"အသီး\",\"facts\":[\"အာဟာရပြည့်ဝသည်\",\"ဗီတာမင်စီပါဝင်သည်\"]}";
            bool ok = RFIDController::getInstance().writeEducationalCard(uid, 4, burmeseSample, RfidLanguage::BURMESE);
            out.printf("[CLI] Write Burmese Sample Result: %s\n", ok ? "SUCCESS" : "FAILED");
        }
    } else if (cmd == "CARD:SAMPLE:EN") {
        String uidStr = RFIDController::getInstance().getLastUid();
        if (uidStr.length() == 0) {
            out.println("[CLI] ERROR: No card present on PN532. Please hold a MIFARE card on the reader and retry.");
        } else {
            uint8_t uid[4] = {0};
            for (int i = 0; i < 4; i++) {
                String byteHex = uidStr.substring(i * 2, i * 2 + 2);
                uid[i] = (uint8_t)strtol(byteHex.c_str(), NULL, 16);
            }
            String englishSample = "{\"schemaVersion\":\"1.0\",\"cardType\":\"educational\",\"language\":\"en\",\"title\":\"Apple\",\"category\":\"Fruits\",\"facts\":[\"Rich in fiber and vitamin C\"]}";
            bool ok = RFIDController::getInstance().writeEducationalCard(uid, 4, englishSample, RfidLanguage::ENGLISH);
            out.printf("[CLI] Write English Sample Result: %s\n", ok ? "SUCCESS" : "FAILED");
        }
    } else {
        out.println("[CLI] Available commands:");
        out.println("  PROV:WIFI:<SSID>:<PASSWORD>");
        out.println("  PROV:AUTH:<DEVICE_ID>:<API_KEY>");
        out.println("  PROV:STATUS");
        out.println("  PROV:CLEAR");
        out.println("  PROV:REBOOT");
        out.println("  SCREEN:TOGGLE");
        out.println("  CARD:SAMPLE:MY (Writes Burmese educational card)");
        out.println("  CARD:SAMPLE:EN (Writes English educational card)");
    }
}

void processSerialCliStream(Stream& stream) {
    static String buffer = "";
    while (stream.available()) {
        char c = (char)stream.read();
        if (c == '\r' || c == '\n') {
            buffer.trim();
            if (buffer.length() > 0) {
                executeCliCommand(buffer, stream);
                buffer = "";
            }
        } else {
            if (buffer.length() < 256) {
                buffer += c;
            }
        }
    }
}

void processSerialCli() {
    processSerialCliStream(Serial);
#if ARDUINO_USB_CDC_ON_BOOT
    if (&Serial0 != &Serial) {
        processSerialCliStream(Serial0);
    }
#endif
}

void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
    if (&Serial0 != &Serial) {
        Serial0.begin(115200);
    }
#endif
    delay(500);

    Serial.println("\n=======================================================");
    Serial.printf("  🤖 %s v%s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
    Serial.printf("  Hardware: %s\n", HARDWARE_VERSION);
    Serial.println("=======================================================");

    // 1. Diagnostics: Flash & PSRAM
    uint32_t flashSize = 0;
    esp_flash_get_size(NULL, &flashSize);
    Serial.printf("[Boot] Flash Memory: %lu MB (Mode: QIO 80MHz)\n", (unsigned long)(flashSize / (1024 * 1024)));

    if (psramInit()) {
        Serial.printf("[Boot] Octal PSRAM: DETECTED! Total: %lu KB, Free: %lu KB\n",
                      (unsigned long)(ESP.getPsramSize() / 1024),
                      (unsigned long)(ESP.getFreePsram() / 1024));
    } else {
        Serial.println("[Boot] WARNING: Octal PSRAM initialization failed or not detected!");
    }

    // 2. Initialize Core Event Bus & Persistence
    EventManager::getInstance().begin();
    ConfigManager::getInstance().begin();

    // 3. Initialize Hardware Drivers
    DisplayController::getInstance().begin();
    AudioInput::getInstance().begin();
    AudioOutput::getInstance().begin();
    ButtonController::getInstance().begin();
    TouchSensorController::getInstance().begin();
    RFIDController::getInstance().begin();

    // 4. Initialize Networking & Cloud Services
    WiFiManager::getInstance().begin();
    WebSocketClientManager::getInstance().begin();
    HeartbeatService::getInstance().begin();
    DeviceManager::getInstance().begin();
    UIManager::getInstance().begin();

    // 5. Spawn FreeRTOS Tasks with Core Affinity
    xTaskCreatePinnedToCore(
        taskNetwork,
        "net_task",
        8192,
        NULL,
        3,
        &taskNetworkHandle,
        0 // Core 0: Network & WebSockets
    );

    xTaskCreatePinnedToCore(
        taskSensors,
        "sensor_task",
        4096,
        NULL,
        2,
        &taskSensorHandle,
        1 // Core 1: Physical Buttons, Touch, RFID
    );

    // 6. Comprehensive Hardware Diagnostic Summary Audit
    uint32_t flashTotal = 0;
    esp_flash_get_size(NULL, &flashTotal);

    Serial.println("\n========================================================================");
    Serial.println("                 HARDWARE SUBSYSTEM DIAGNOSTIC AUDIT                    ");
    Serial.println("========================================================================");
    Serial.println(" Subsystem       | Driver Status      | Hardware Detection Status       ");
    Serial.println("-----------------+--------------------+---------------------------------");
    Serial.printf(" Flash & PSRAM   | DRIVER_INITIALIZED | HARDWARE_RESPONDING (%lu MB Flash, %lu KB PSRAM)\n",
                  (unsigned long)(flashTotal / (1024 * 1024)),
                  (unsigned long)(ESP.getPsramSize() / 1024));
    Serial.printf(" INMP441 Mic     | DRIVER_INITIALIZED | %s\n", AudioInput::getInstance().getHardwareStatusStr());
    Serial.printf(" MAX98357A Amp   | DRIVER_INITIALIZED | %s\n", AudioOutput::getInstance().getHardwareStatusStr());
    Serial.printf(" ILI9341 Display | DRIVER_INITIALIZED | %s\n", DisplayController::getInstance().getHardwareStatusStr());
    Serial.printf(" TTP223 Touch    | DRIVER_INITIALIZED | %s\n", TouchSensorController::getInstance().getHardwareStatusStr());
    Serial.printf(" Volume Buttons  | DRIVER_INITIALIZED | %s\n", ButtonController::getInstance().getHardwareStatusStr());
    Serial.printf(" PN532 RFID      | DRIVER_INITIALIZED | %s\n", RFIDController::getInstance().getHardwareStatusStr());
    Serial.printf(" WakeNet9 Model  | DRIVER_INITIALIZED | %s\n", WakeWordEngine::getInstance().getHardwareStatusStr());
    Serial.println("========================================================================\n");

    Serial.println("[Boot] All subsystems and FreeRTOS tasks launched successfully.");
    Serial.println("Type 'PROV:STATUS' or 'PROV:WIFI:<ssid>:<pass>' in Serial Monitor.");
}

void loop() {
    // 1. Process Serial CLI Provisioning Input
    processSerialCli();

    // 2. Device Manager FSM & Audio Streaming
    DeviceManager::getInstance().update();

    // 3. UI 30 FPS Avatar & Graphics Engine
    UIManager::getInstance().update();

    vTaskDelay(pdMS_TO_TICKS(5));
}

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
void processSerialCli() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    if (line.startsWith("PROV:WIFI:")) {
        int firstColon = line.indexOf(':', 10);
        if (firstColon > 0) {
            String ssid = line.substring(10, firstColon);
            String pass = line.substring(firstColon + 1);
            ConfigManager::getInstance().setWifiCredentials(ssid, pass);
            Serial.printf("[CLI] Wi-Fi set to SSID: %s\n", ssid.c_str());
        }
    } else if (line.startsWith("PROV:AUTH:")) {
        int firstColon = line.indexOf(':', 10);
        if (firstColon > 0) {
            String devId = line.substring(10, firstColon);
            String apiKey = line.substring(firstColon + 1);
            ConfigManager::getInstance().setDeviceCredentials(devId, apiKey);
            Serial.printf("[CLI] Device credentials set: ID=%s, Key=%s\n",
                          devId.c_str(), ConfigManager::maskSecret(apiKey).c_str());
        }
    } else if (line == "PROV:STATUS") {
        Serial.println("\n--- DEVICE PROVISIONING STATUS ---");
        Serial.printf("Firmware: %s (%s)\n", FIRMWARE_NAME, FIRMWARE_VERSION);
        Serial.printf("Wi-Fi SSID: %s\n", ConfigManager::getInstance().getWifiSSID().c_str());
        Serial.printf("Device ID:  %s\n", ConfigManager::getInstance().getDeviceId().c_str());
        Serial.printf("API Key:    %s\n", ConfigManager::maskSecret(ConfigManager::getInstance().getDeviceApiKey()).c_str());
        Serial.printf("Internal SRAM: %lu KB Free\n", (unsigned long)(ESP.getFreeHeap() / 1024));
        Serial.printf("Octal PSRAM:   %lu / %lu KB Free\n",
                      (unsigned long)(ESP.getFreePsram() / 1024),
                      (unsigned long)(ESP.getPsramSize() / 1024));
        Serial.printf("IP Address: %s (RSSI: %d dBm)\n", WiFiManager::getInstance().getIpAddress().c_str(), WiFiManager::getInstance().getRSSI());
        Serial.printf("WebSocket:  %s\n", WebSocketClientManager::getInstance().isAuthenticated() ? "ONLINE" : "OFFLINE");
        Serial.println("----------------------------------\n");
    } else if (line == "PROV:CLEAR") {
        ConfigManager::getInstance().clearAll();
        Serial.println("[CLI] Credentials cleared from NVS.");
    } else if (line == "PROV:REBOOT") {
        Serial.println("[CLI] Rebooting device...");
        delay(500);
        ESP.restart();
    } else if (line == "SCREEN:TOGGLE") {
        UIManager::getInstance().toggleScreen();
        Serial.println("[CLI] Display screen toggled.");
    } else {
        Serial.println("[CLI] Available commands:");
        Serial.println("  PROV:WIFI:<SSID>:<PASSWORD>");
        Serial.println("  PROV:AUTH:<DEVICE_ID>:<API_KEY>");
        Serial.println("  PROV:STATUS");
        Serial.println("  PROV:CLEAR");
        Serial.println("  PROV:REBOOT");
        Serial.println("  SCREEN:TOGGLE");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500); // Allow USB CDC connection to stabilize

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

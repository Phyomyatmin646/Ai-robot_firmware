#include "services/heartbeat.h"
#include "network/websocket_client.h"
#include "network/wifi_manager.h"

void HeartbeatService::begin(uint32_t intervalSeconds) {
    setInterval(intervalSeconds);
    lastHeartbeatTime = millis();
    Serial.printf("[HeartbeatService] Initialized (Interval: %lu seconds).\n", (unsigned long)intervalSeconds);
}

void HeartbeatService::setInterval(uint32_t intervalSeconds) {
    if (intervalSeconds < 5) intervalSeconds = 5;
    if (intervalSeconds > 120) intervalSeconds = 120;
    intervalMs = intervalSeconds * 1000;
}

void HeartbeatService::update() {
    unsigned long now = millis();
    if (now - lastHeartbeatTime >= intervalMs) {
        lastHeartbeatTime = now;

        if (WebSocketClientManager::getInstance().isAuthenticated()) {
            uint32_t freeHeap = (uint32_t)ESP.getFreeHeap();
            uint32_t uptime = (uint32_t)(now / 1000);
            int8_t rssi = WiFiManager::getInstance().getRSSI();
            float cpuTemp = temperatureRead(); // Built-in ESP32-S3 internal temperature sensor

            bool sent = WebSocketClientManager::getInstance().sendHeartbeat(freeHeap, uptime, rssi, cpuTemp, 100);
            if (sent) {
                Serial.printf("[Heartbeat] Sent telemetry -> FreeHeap: %lu B, Uptime: %lu s, RSSI: %d dBm, Temp: %.1f C\n",
                              (unsigned long)freeHeap, (unsigned long)uptime, rssi, cpuTemp);
            }
        }
    }
}

#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "config.h"
#include "services/config_manager.h"
#include "services/event_manager.h"

using namespace websockets;

class WebSocketClientManager {
public:
    static WebSocketClientManager& getInstance() {
        static WebSocketClientManager instance;
        return instance;
    }

    void begin();
    void update(); // Called in network task loop

    bool isConnected() { return client.available(); }
    bool isAuthenticated() const { return authenticated; }

    // Outbound Telemetry & Control (JSON Text Frames)
    bool sendHeartbeat(uint32_t freeHeap, uint32_t uptimeSeconds, int8_t wifiRSSI, float cpuTemp = 0.0f, uint8_t batteryLevel = 100);
    bool sendTouchEvent(uint8_t pad, bool pressed);
    bool sendAudioStart(const String& sessionId, uint32_t sampleRate = 16000);
    bool sendAudioEnd(uint32_t totalChunks);
    bool sendSensorUpdate(const String& key, const String& value);

    // Outbound Audio Streaming (Raw Binary Frames)
    bool sendAudioChunk(const uint8_t* pcmData, size_t length);

    void disconnect();

private:
    WebSocketClientManager() = default;

    void handleTextMessage(const WebsocketsMessage& msg);
    void handleBinaryMessage(const WebsocketsMessage& msg);
    void connect();

    // FreeRTOS thread safety across Core 0 (Network) and Core 1 (Audio/Sensors)
    SemaphoreHandle_t wsMutex = nullptr;
    bool takeLock(TickType_t timeout = pdMS_TO_TICKS(50)) {
        if (!wsMutex) return true;
        return (xSemaphoreTake(wsMutex, timeout) == pdTRUE);
    }
    void giveLock() {
        if (wsMutex) {
            xSemaphoreGive(wsMutex);
        }
    }

    WebsocketsClient client;
    bool authenticated = false;
    bool authFailed = false;

    unsigned long lastReconnectAttempt = 0;
    unsigned long reconnectInterval = WS_RECONNECT_MIN_DELAY_MS;
};

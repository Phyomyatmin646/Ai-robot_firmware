#include "network/websocket_client.h"
#include "hardware/audio_output.h"
#include "hardware/display.h"

void WebSocketClientManager::begin() {
    if (!wsMutex) {
        wsMutex = xSemaphoreCreateMutex();
    }

    client.onMessage([this](WebsocketsMessage msg) {
        if (msg.isBinary()) {
            handleBinaryMessage(msg);
        } else if (msg.isText()) {
            handleTextMessage(msg);
        }
    });

    client.onEvent([this](WebsocketsEvent event, String data) {
        if (event == WebsocketsEvent::ConnectionOpened) {
            Serial.println("[WebSocket] TLS Connection Established. Waiting for device:connected handshake...");
        } else if (event == WebsocketsEvent::ConnectionClosed) {
            Serial.printf("[WebSocket] Connection closed. Data: %s\n", data.c_str());
            authenticated = false;
            AppEvent ev{};
            ev.type = EventType::WS_DISCONNECTED;
            snprintf(ev.text, sizeof(ev.text), "ws:closed");
            EventManager::getInstance().postEvent(ev);
        } else if (event == WebsocketsEvent::GotPing) {
            // ArduinoWebsockets handles Pong automatically
        }
    });

    Serial.println("[WebSocketClientManager] Initialized.");
}

void WebSocketClientManager::connect() {
    if (authFailed) {
        // Prevent infinite loops on invalid credentials
        return;
    }

    if (!ConfigManager::getInstance().hasDeviceCredentials()) {
        Serial.println("[WebSocket] Missing Device ID or API Key in NVS. Provisioning required.");
        return;
    }

    String deviceId = ConfigManager::getInstance().getDeviceId();
    String apiKey = ConfigManager::getInstance().getDeviceApiKey();

    Serial.printf("[WebSocket] Connecting to wss://%s:%d%s ...\n",
                  DEFAULT_WS_HOST, DEFAULT_WS_PORT, DEFAULT_WS_PATH);

    if (!takeLock(pdMS_TO_TICKS(500))) return;

    // Set custom authentication headers
    client.addHeader("X-Device-ID", deviceId);
    client.addHeader("X-Device-Key", apiKey);

    bool connected = client.connect(DEFAULT_WS_HOST, DEFAULT_WS_PORT, DEFAULT_WS_PATH);
    giveLock();

    if (!connected) {
        Serial.println("[WebSocket] Handshake failed. Will retry with backoff.");
    }
}

void WebSocketClientManager::update() {
    bool hasData = false;
    if (takeLock(pdMS_TO_TICKS(20))) {
        if (client.available()) {
            client.poll();
            hasData = true;
        }
        giveLock();
    }

    if (!hasData) {
        authenticated = false;
        unsigned long now = millis();
        if (WiFi.status() == WL_CONNECTED && !authFailed) {
            if (now - lastReconnectAttempt > reconnectInterval) {
                lastReconnectAttempt = now;
                reconnectInterval = (reconnectInterval * 2 <= WS_RECONNECT_MAX_DELAY_MS)
                                  ? (reconnectInterval * 2)
                                  : WS_RECONNECT_MAX_DELAY_MS;
                connect();
            }
        }
    }
}

void WebSocketClientManager::handleTextMessage(const WebsocketsMessage& msg) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg.data());
    if (err) {
        Serial.printf("[WebSocket] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* event = doc["event"] | doc["type"] | "";
    JsonObject data = doc["data"].as<JsonObject>();
    if (data.isNull()) {
        data = doc["payload"].as<JsonObject>();
    }

    if (strcmp(event, "device:connected") == 0) {
        authenticated = true;
        authFailed = false;
        reconnectInterval = WS_RECONNECT_MIN_DELAY_MS;
        Serial.println("[WebSocket] Handshake SUCCESS: device:connected acknowledged by backend!");

        // Initial settings synchronization from cloud source of truth
        if (!data["volume"].isNull()) {
            uint8_t syncedVol = data["volume"] | DEFAULT_VOLUME;
            ConfigManager::getInstance().setVolume(syncedVol);
            AudioOutput::getInstance().setVolume(syncedVol);
            Serial.printf("[WebSocket] Initial volume synced from cloud: %u%%\n", syncedVol);
        }
        if (!data["brightness"].isNull()) {
            uint8_t syncedBright = data["brightness"] | DEFAULT_BRIGHTNESS;
            ConfigManager::getInstance().setBrightness(syncedBright);
            DisplayController::getInstance().setBrightness(syncedBright);
            Serial.printf("[WebSocket] Initial brightness synced from cloud: %u%%\n", syncedBright);
        }

        AppEvent ev{};
        ev.type = EventType::WS_AUTHENTICATED;
        snprintf(ev.text, sizeof(ev.text), "authenticated");
        EventManager::getInstance().postEvent(ev);
    } else if (strcmp(event, "audio:response:start") == 0) {
        uint32_t sampleRate = data["sampleRate"] | 24000;
        AudioOutput::getInstance().setSampleRate(sampleRate);

        AppEvent ev{};
        ev.type = EventType::VOICE_RESPONSE_START;
        ev.data.intValue = (int)sampleRate;
        snprintf(ev.text, sizeof(ev.text), "speaking:%lu", (unsigned long)sampleRate);
        EventManager::getInstance().postEvent(ev);

        Serial.printf("[WebSocket] AI Voice response starting (Sample Rate: %lu Hz).\n", (unsigned long)sampleRate);
    } else if (strcmp(event, "audio:response:end") == 0) {
        AppEvent ev{};
        ev.type = EventType::VOICE_RESPONSE_END;
        snprintf(ev.text, sizeof(ev.text), "response:end");
        EventManager::getInstance().postEvent(ev);

        Serial.println("[WebSocket] AI Voice response completed.");
    } else if (strcmp(event, "audio:cancel") == 0) {
        const char* reason = data["reason"] | "interruption";
        AudioOutput::getInstance().stopAndFlush();

        AppEvent ev{};
        ev.type = EventType::VOICE_AUDIO_CANCEL;
        snprintf(ev.text, sizeof(ev.text), "%s", reason);
        EventManager::getInstance().postEvent(ev);

        Serial.printf("[WebSocket] Audio canceled by backend (%s).\n", reason);
    } else if (strcmp(event, "robot:command") == 0) {
        const char* cmdType = data["type"] | "";
        JsonObject payload = data["payload"].as<JsonObject>();

        if (strcmp(cmdType, "set_volume") == 0) {
            uint8_t vol = payload["volume"] | 80;
            ConfigManager::getInstance().setVolume(vol);
            AudioOutput::getInstance().setVolume(vol);

            AppEvent ev{};
            ev.type = EventType::CMD_SET_VOLUME;
            ev.data.intValue = vol;
            snprintf(ev.text, sizeof(ev.text), "set_vol:%u", vol);
            EventManager::getInstance().postEvent(ev);

            Serial.printf("[WebSocket] Remote command: set_volume -> %u%%\n", vol);
        } else if (strcmp(cmdType, "set_brightness") == 0) {
            uint8_t bright = payload["brightness"] | 80;
            ConfigManager::getInstance().setBrightness(bright);
            DisplayController::getInstance().setBrightness(bright);

            AppEvent ev{};
            ev.type = EventType::CMD_SET_BRIGHTNESS;
            ev.data.intValue = bright;
            snprintf(ev.text, sizeof(ev.text), "set_bright:%u", bright);
            EventManager::getInstance().postEvent(ev);

            Serial.printf("[WebSocket] Remote command: set_brightness -> %u%%\n", bright);
        } else if (strcmp(cmdType, "stop_audio") == 0) {
            AudioOutput::getInstance().stopAndFlush();
            AppEvent ev{};
            ev.type = EventType::CMD_STOP_AUDIO;
            EventManager::getInstance().postEvent(ev);
        }
    } else if (strcmp(event, "reminder:alert") == 0) {
        const char* title = data["title"] | "Reminder Alert";
        Serial.printf("[WebSocket] Reminder Alert received from cloud: %s\n", title);
        AppEvent ev{};
        ev.type = EventType::VOICE_RESPONSE_START;
        snprintf(ev.text, sizeof(ev.text), "alert:%s", title);
        EventManager::getInstance().postEvent(ev);
    }
}

void WebSocketClientManager::handleBinaryMessage(const WebsocketsMessage& msg) {
    // Binary audio frame from Gemini Live (24kHz 16-bit PCM mono)
    const char* rawData = msg.c_str();
    size_t length = msg.length();

    if (length > 0 && rawData) {
        size_t samples = length / sizeof(int16_t);
        AudioOutput::getInstance().writePcm((const int16_t*)rawData, samples);
    }
}

bool WebSocketClientManager::sendHeartbeat(uint32_t freeHeap, uint32_t uptimeSeconds, int8_t wifiRSSI, float cpuTemp, uint8_t batteryLevel) {
    if (!authenticated) return false;

    JsonDocument doc;
    doc["event"] = "device:heartbeat";
    JsonObject data = doc["data"].to<JsonObject>();
    data["freeHeap"] = freeHeap;
    data["uptimeSeconds"] = uptimeSeconds;
    data["wifiRSSI"] = wifiRSSI;
    data["cpuTemp"] = cpuTemp;
    data["batteryLevel"] = batteryLevel;

    String jsonStr;
    serializeJson(doc, jsonStr);

    if (!takeLock(pdMS_TO_TICKS(50))) return false;
    bool sent = false;
    if (client.available()) {
        sent = client.send(jsonStr);
    }
    giveLock();
    return sent;
}

bool WebSocketClientManager::sendTouchEvent(uint8_t pad, bool pressed) {
    JsonDocument doc;
    doc["event"] = "touch:event";
    JsonObject data = doc["data"].to<JsonObject>();
    data["pad"] = pad;
    data["pressed"] = pressed;

    String jsonStr;
    serializeJson(doc, jsonStr);

    if (!takeLock(pdMS_TO_TICKS(50))) return false;
    bool sent = false;
    if (client.available()) {
        sent = client.send(jsonStr);
    }
    giveLock();
    return sent;
}

bool WebSocketClientManager::sendAudioStart(const String& sessionId, uint32_t sampleRate) {
    JsonDocument doc;
    doc["event"] = "audio:start";
    JsonObject data = doc["data"].to<JsonObject>();
    data["sessionId"] = sessionId;
    data["format"] = "pcm_16bit";
    data["sampleRate"] = sampleRate;

    String jsonStr;
    serializeJson(doc, jsonStr);

    if (!takeLock(pdMS_TO_TICKS(100))) return false;
    bool sent = false;
    if (client.available()) {
        sent = client.send(jsonStr);
    }
    giveLock();
    return sent;
}

bool WebSocketClientManager::sendAudioEnd(uint32_t totalChunks) {
    JsonDocument doc;
    doc["event"] = "audio:end";
    JsonObject data = doc["data"].to<JsonObject>();
    data["totalChunks"] = totalChunks;

    String jsonStr;
    serializeJson(doc, jsonStr);

    if (!takeLock(pdMS_TO_TICKS(100))) return false;
    bool sent = false;
    if (client.available()) {
        sent = client.send(jsonStr);
    }
    giveLock();
    return sent;
}

bool WebSocketClientManager::sendSensorUpdate(const String& key, const String& value) {
    JsonDocument doc;
    doc["event"] = "sensor:update";
    JsonObject data = doc["data"].to<JsonObject>();
    data[key] = value;

    String jsonStr;
    serializeJson(doc, jsonStr);

    if (!takeLock(pdMS_TO_TICKS(50))) return false;
    bool sent = false;
    if (client.available()) {
        sent = client.send(jsonStr);
    }
    giveLock();
    return sent;
}

bool WebSocketClientManager::sendAudioChunk(const uint8_t* pcmData, size_t length) {
    if (!pcmData || length == 0) return false;

    // Fast acquisition for real-time audio chunk (timeout 25ms)
    if (!takeLock(pdMS_TO_TICKS(25))) return false;
    bool sent = false;
    if (client.available() && authenticated) {
        sent = client.sendBinary((const char*)pcmData, length);
    }
    giveLock();
    return sent;
}

void WebSocketClientManager::disconnect() {
    if (takeLock(pdMS_TO_TICKS(100))) {
        if (client.available()) {
            client.close();
        }
        giveLock();
    }
    authenticated = false;
}

#include "services/device_manager.h"
#include "services/event_manager.h"
#include "services/config_manager.h"
#include "hardware/audio_input.h"
#include "hardware/audio_output.h"
#include "hardware/display.h"
#include "hardware/rfid.h"
#include "hardware/wake_word.h"
#include "network/websocket_client.h"
#include "network/wifi_manager.h"

void DeviceManager::begin() {
    WakeWordEngine::getInstance().begin();
    setState(RobotState::IDLE);
    Serial.println("[DeviceManager] Initialized in Local WakeNet ('Hi ESP') & Head-Touch Mode.");
}

void DeviceManager::setState(RobotState state) {
    if (currentState != state) {
        currentState = state;
        Serial.printf("[DeviceManager] State transitioned to: %s\n", robotStateToString(state));

        if (state == RobotState::IDLE) {
            // Resume local wake-word monitoring and process any queued RFID scans
            WakeWordEngine::getInstance().setListening(true);
            checkAndDispatchPendingRfid();
        } else {
            // Pause local wake-word while active in speech, thinking, or playback
            WakeWordEngine::getInstance().setListening(false);
        }
    }
}

void DeviceManager::startVoiceSession() {
    if (currentState != RobotState::IDLE) return;

    if (!WebSocketClientManager::getInstance().isAuthenticated()) {
        Serial.println("[DeviceManager] Wake word detected, but robot is offline (WebSocket not authenticated).");
        WakeWordEngine::getInstance().setListening(true);
        return;
    }

    String devId = ConfigManager::getInstance().getDeviceId();
    currentSessionId = "voice_" + devId + "_" + String(millis());
    audioChunkCount = 0;
    listeningStartTime = millis();

    AudioInput::getInstance().startRecording();
    WebSocketClientManager::getInstance().sendAudioStart(currentSessionId, AUDIO_INPUT_SAMPLE_RATE);

    // Flush pre-roll audio so initial speech after "Hi ESP" is not lost
    int16_t preRoll[1024];
    size_t preRollSamples = WakeWordEngine::getInstance().getPreRollAudio(preRoll, 1024);
    if (preRollSamples > 0) {
        audioChunkCount++;
        WebSocketClientManager::getInstance().sendAudioChunk(
            (const uint8_t*)preRoll,
            preRollSamples * sizeof(int16_t)
        );
        Serial.printf("[DeviceManager] Flushed %u pre-roll samples into voice stream.\n", (unsigned int)preRollSamples);
    }

    setState(RobotState::LISTENING);
    Serial.printf("[DeviceManager] Voice session started: %s\n", currentSessionId.c_str());
}

void DeviceManager::endVoiceSession() {
    if (currentState != RobotState::LISTENING) return;

    AudioInput::getInstance().stopRecording();
    WebSocketClientManager::getInstance().sendAudioEnd(audioChunkCount);

    thinkingStartTime = millis();
    setState(RobotState::THINKING);
    Serial.printf("[DeviceManager] Voice capture ended (%lu chunks sent). Waiting for Gemini Live response...\n", (unsigned long)audioChunkCount);
}

void DeviceManager::processOrQueueRfid(const String& uid, const String& cardData) {
    // Concurrency Rule: If robot is speaking, listening, or thinking, DO NOT interrupt voice interaction!
    if (currentState == RobotState::LISTENING ||
        currentState == RobotState::THINKING ||
        currentState == RobotState::SPEAKING)
    {
        Serial.printf("[DeviceManager] Voice session active (%s). Queuing RFID scan (%s) until IDLE.\n",
                      robotStateToString(currentState), uid.c_str());
        pendingRfidUid = uid;
        pendingRfidData = cardData;
        hasPendingRfid = true;
        return;
    }

    // Robot is IDLE: dispatch scan immediately with duplicate suppression
    unsigned long now = millis();
    bool isDuplicate = (uid == lastDispatchedRfidUid) && ((now - lastRfidDispatchedTime) < rfidDuplicateWindowMs);
    if (isDuplicate) {
        Serial.printf("[DeviceManager] Duplicate RFID scan ignored: %s\n", uid.c_str());
        return;
    }

    lastDispatchedRfidUid = uid;
    lastRfidDispatchedTime = now;

    Serial.printf("[DeviceManager] Dispatching authoritative rfid:scan (UID: %s, Data: %s)\n",
                  uid.c_str(), cardData.length() > 0 ? "YES" : "NO");

    WebSocketClientManager::getInstance().sendRfidScan(uid, cardData);
    WebSocketClientManager::getInstance().sendSensorUpdate("rfidUid", uid);
}

void DeviceManager::checkAndDispatchPendingRfid() {
    if (!hasPendingRfid || currentState != RobotState::IDLE) return;

    unsigned long now = millis();
    bool isDuplicate = (pendingRfidUid == lastDispatchedRfidUid) && ((now - lastRfidDispatchedTime) < rfidDuplicateWindowMs);

    if (!isDuplicate) {
        lastDispatchedRfidUid = pendingRfidUid;
        lastRfidDispatchedTime = now;

        Serial.printf("[DeviceManager] Dispatching queued RFID scan after voice completion (UID: %s)\n", pendingRfidUid.c_str());
        WebSocketClientManager::getInstance().sendRfidScan(pendingRfidUid, pendingRfidData);
        WebSocketClientManager::getInstance().sendSensorUpdate("rfidUid", pendingRfidUid);
    } else {
        Serial.printf("[DeviceManager] Queued RFID scan ignored as duplicate: %s\n", pendingRfidUid.c_str());
    }

    hasPendingRfid = false;
    pendingRfidUid = "";
    pendingRfidData = "";
}

void DeviceManager::handleEvent(const AppEvent& event) {
    switch (event.type) {
        case EventType::TOUCH_SENSOR_PRESS:
            // Head-Touch Interaction on Memo Robot:
            // 1. Play cute local sound effect on ESP32
            AudioOutput::getInstance().playCuteChime();
            // 2. Transmit head-touch telemetry to cloud dashboard
            WebSocketClientManager::getInstance().sendTouchEvent(0, true);

            // 3. Touch-to-Talk for Memo:
            // Tapping Memo's head activates listening mode to talk to Memo!
            // If already listening, tapping head concludes utterance.
            if (currentState == RobotState::IDLE) {
                Serial.println("[DeviceManager] Head-touch on Memo detected: Activating Voice Session...");
                startVoiceSession();
            } else if (currentState == RobotState::LISTENING) {
                Serial.println("[DeviceManager] Head-touch detected while listening: Concluding voice session.");
                endVoiceSession();
            }
            break;

        case EventType::TOUCH_SENSOR_RELEASE:
            WebSocketClientManager::getInstance().sendTouchEvent(0, false);
            break;

        case EventType::TOUCH_SENSOR_LONG_PRESS:
            Serial.println("[DeviceManager] Entering Provisioning Portal...");
            WiFiManager::getInstance().startProvisioningPortal();
            break;

        case EventType::VOICE_SESSION_START:
            // Triggered exclusively by WakeNet9 detection ("Hi ESP")
            startVoiceSession();
            break;

        case EventType::VOICE_SESSION_END:
            endVoiceSession();
            break;

        case EventType::BUTTON_VOL_UP_CLICK:
        case EventType::BUTTON_VOL_DOWN_CLICK: {
            uint8_t vol = (uint8_t)event.data.intValue;
            AudioOutput::getInstance().setVolume(vol);
            WebSocketClientManager::getInstance().sendSensorUpdate("currentVolume", String(vol));
            break;
        }

        case EventType::RFID_TAG_DETECTED: {
            String uid = String(event.text);
            String cardData = RFIDController::getInstance().getLastCardData();
            processOrQueueRfid(uid, cardData);
            break;
        }

        case EventType::VOICE_RESPONSE_START:
            setState(RobotState::SPEAKING);
            break;

        case EventType::VOICE_RESPONSE_END:
            setState(RobotState::IDLE);
            break;

        case EventType::VOICE_AUDIO_CANCEL:
            setState(RobotState::LISTENING);
            break;

        case EventType::WS_AUTHENTICATED:
            setState(RobotState::IDLE);
            break;

        case EventType::WS_AUTH_FAILED:
        case EventType::NETWORK_DISCONNECTED:
            setState(RobotState::ERROR);
            break;

        case EventType::CMD_SET_VOLUME:
            AudioOutput::getInstance().setVolume((uint8_t)event.data.intValue);
            break;

        case EventType::CMD_SET_BRIGHTNESS:
            DisplayController::getInstance().setBrightness((uint8_t)event.data.intValue);
            break;

        case EventType::CMD_STOP_AUDIO:
            AudioOutput::getInstance().stopAndFlush();
            setState(RobotState::IDLE);
            break;

        default:
            break;
    }
}

void DeviceManager::update() {
    AppEvent event;
    // Process pending events from FreeRTOS queue non-blockingly
    while (EventManager::getInstance().receiveEvent(&event, 0)) {
        handleEvent(event);
    }

    // Pipeline State 1: Active Voice Session Streaming to Backend
    if (currentState == RobotState::LISTENING && AudioInput::getInstance().isRecording()) {
        int16_t chunkBuffer[AUDIO_CHUNK_SIZE / 2];
        size_t samplesRead = AudioInput::getInstance().readPcmChunk(chunkBuffer, AUDIO_CHUNK_SIZE / 2);

        if (samplesRead > 0) {
            audioChunkCount++;
            WebSocketClientManager::getInstance().sendAudioChunk(
                (const uint8_t*)chunkBuffer,
                samplesRead * sizeof(int16_t)
            );
        }

        // Automatic utterance completion timeout for hands-free interaction
        if ((millis() - listeningStartTime) > maxUtteranceDurationMs) {
            Serial.println("[DeviceManager] Maximum utterance duration reached. Concluding audio capture.");
            endVoiceSession();
        }
    }
    // Pipeline State 2: Waiting for Backend Response (with safety timeout)
    else if (currentState == RobotState::THINKING) {
        if ((millis() - thinkingStartTime) > maxThinkingDurationMs) {
            Serial.println("[DeviceManager] Backend response timeout. Returning to IDLE.");
            setState(RobotState::IDLE);
        }
    }
    // Pipeline State 3: IDLE Local WakeNet9 Inference (ZERO network streaming)
    else if (currentState == RobotState::IDLE && WakeWordEngine::getInstance().isListening()) {
        // Microphone reads chunks locally ONLY for WakeNet neural inference
        // Network transmission is strictly inactive.
        int16_t localBuffer[AUDIO_CHUNK_SIZE / 2];
        size_t samplesRead = AudioInput::getInstance().readPcmChunk(localBuffer, AUDIO_CHUNK_SIZE / 2);
        if (samplesRead > 0) {
            WakeWordEngine::getInstance().processPcmChunk(localBuffer, samplesRead);
        }
    }
}

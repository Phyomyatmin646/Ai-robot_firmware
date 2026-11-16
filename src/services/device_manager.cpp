#include "services/device_manager.h"
#include "services/event_manager.h"
#include "services/config_manager.h"
#include "hardware/audio_input.h"
#include "hardware/audio_output.h"
#include "hardware/display.h"
#include "network/websocket_client.h"
#include "network/wifi_manager.h"

void DeviceManager::begin() {
    setState(RobotState::IDLE);
    Serial.println("[DeviceManager] Initialized.");
}

void DeviceManager::setState(RobotState state) {
    if (currentState != state) {
        currentState = state;
        Serial.printf("[DeviceManager] State transitioned to: %s\n", robotStateToString(state));
    }
}

void DeviceManager::startVoiceSession() {
    if (currentState != RobotState::IDLE) return;

    String devId = ConfigManager::getInstance().getDeviceId();
    currentSessionId = "voice_" + devId + "_" + String(millis());
    audioChunkCount = 0;

    AudioInput::getInstance().startRecording();
    WebSocketClientManager::getInstance().sendAudioStart(currentSessionId, AUDIO_INPUT_SAMPLE_RATE);
    WebSocketClientManager::getInstance().sendTouchEvent(0, true);

    setState(RobotState::LISTENING);
}

void DeviceManager::endVoiceSession() {
    if (currentState != RobotState::LISTENING) return;

    AudioInput::getInstance().stopRecording();
    WebSocketClientManager::getInstance().sendAudioEnd(audioChunkCount);
    WebSocketClientManager::getInstance().sendTouchEvent(0, false);

    setState(RobotState::THINKING);
}

void DeviceManager::handleEvent(const AppEvent& event) {
    switch (event.type) {
        case EventType::TOUCH_SENSOR_PRESS:
            startVoiceSession();
            break;

        case EventType::TOUCH_SENSOR_RELEASE:
            endVoiceSession();
            break;

        case EventType::TOUCH_SENSOR_LONG_PRESS:
            Serial.println("[DeviceManager] Entering Provisioning Portal...");
            WiFiManager::getInstance().startProvisioningPortal();
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
            WebSocketClientManager::getInstance().sendSensorUpdate("rfidUid", uid);
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

    // Audio Streaming Pipeline: if recording, read chunks and stream via WebSocket
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
    }
}

#pragma once

#include <Arduino.h>

/**
 * ============================================================================
 * ROBOT APPLICATION EVENTS & STATE DEFINITIONS
 * ============================================================================
 */

enum class RobotState {
    IDLE,
    LISTENING,
    THINKING,
    SPEAKING,
    ERROR
};

inline const char* robotStateToString(RobotState state) {
    switch (state) {
        case RobotState::IDLE:      return "idle";
        case RobotState::LISTENING: return "listening";
        case RobotState::THINKING:  return "thinking";
        case RobotState::SPEAKING:  return "speaking";
        case RobotState::ERROR:     return "error";
        default:                    return "idle";
    }
}

enum class EventType {
    // Button events
    BUTTON_VOL_UP_CLICK,
    BUTTON_VOL_DOWN_CLICK,

    // Touch sensor events
    TOUCH_SENSOR_PRESS,
    TOUCH_SENSOR_RELEASE,
    TOUCH_SENSOR_LONG_PRESS,

    // RFID events
    RFID_TAG_DETECTED,
    RFID_TAG_REMOVED,

    // Network / Backend events
    NETWORK_CONNECTED,
    NETWORK_DISCONNECTED,
    WS_AUTHENTICATED,
    WS_AUTH_FAILED,
    WS_DISCONNECTED,

    // Voice lifecycle events
    VOICE_SESSION_START,
    VOICE_SESSION_END,
    VOICE_AUDIO_CANCEL,
    VOICE_RESPONSE_START,
    VOICE_RESPONSE_END,

    // Remote Hardware commands
    CMD_SET_VOLUME,
    CMD_SET_BRIGHTNESS,
    CMD_STOP_AUDIO
};

struct AppEvent {
    EventType type;
    union {
        int intValue;
        float floatValue;
        bool boolValue;
    } data;
    char text[64];
};

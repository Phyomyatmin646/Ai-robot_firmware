#pragma once

#include <Arduino.h>
#include "app_events.h"

class DeviceManager {
public:
    static DeviceManager& getInstance() {
        static DeviceManager instance;
        return instance;
    }

    void begin();
    void update(); // Main event loop processing

    RobotState getState() const { return currentState; }
    void setState(RobotState state);

    String getActiveSessionId() const { return currentSessionId; }

private:
    DeviceManager() = default;

    void handleEvent(const AppEvent& event);
    void startVoiceSession();
    void endVoiceSession();

    RobotState currentState = RobotState::IDLE;
    String currentSessionId = "";
    uint32_t audioChunkCount = 0;
};

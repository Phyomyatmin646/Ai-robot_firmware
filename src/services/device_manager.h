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

    // RFID and Voice Session Concurrency Control
    void processOrQueueRfid(const String& uid, const String& cardData);
    void checkAndDispatchPendingRfid();

    RobotState currentState = RobotState::IDLE;
    String currentSessionId = "";
    uint32_t audioChunkCount = 0;
    unsigned long listeningStartTime = 0;
    const unsigned long maxUtteranceDurationMs = 7000; // 7 seconds maximum hands-free speech window
    unsigned long thinkingStartTime = 0;
    const unsigned long maxThinkingDurationMs = 12000; // 12 seconds timeout waiting for cloud response

    // RFID Queuing
    bool hasPendingRfid = false;
    String pendingRfidUid = "";
    String pendingRfidData = "";
    unsigned long lastRfidDispatchedTime = 0;
    String lastDispatchedRfidUid = "";
    const unsigned long rfidDuplicateWindowMs = 3000;
};

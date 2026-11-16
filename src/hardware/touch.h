#pragma once

#include <Arduino.h>
#include "pins.h"
#include "services/event_manager.h"

class TouchSensorController {
public:
    static TouchSensorController& getInstance() {
        static TouchSensorController instance;
        return instance;
    }

    void begin();
    void update(); // Called periodically (e.g. 20ms)

    bool isPressed() const { return currentlyPressed; }

private:
    TouchSensorController() = default;

    bool currentlyPressed = false;
    unsigned long pressStartTime = 0;
    bool longPressTriggered = false;

    const unsigned long longPressThreshold = 4000; // 4 seconds for provisioning mode
    const unsigned long debounceTime = 30; // ms
    unsigned long lastChangeTime = 0;
    int lastPinState = LOW;
};

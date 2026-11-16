#pragma once

#include <Arduino.h>
#include "pins.h"
#include "services/event_manager.h"

class ButtonController {
public:
    static ButtonController& getInstance() {
        static ButtonController instance;
        return instance;
    }

    void begin();
    void update(); // Called periodically in sensor task (e.g. every 20ms)

private:
    ButtonController() = default;

    unsigned long lastDebounceTimeUp = 0;
    unsigned long lastDebounceTimeDown = 0;
    const unsigned long debounceDelay = 50; // ms

    int lastStateUp = HIGH;
    int currentStateUp = HIGH;

    int lastStateDown = HIGH;
    int currentStateDown = HIGH;
};

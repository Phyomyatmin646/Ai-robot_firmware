#include "hardware/touch.h"

void TouchSensorController::begin() {
    pinMode(PIN_TOUCH_SENSOR, INPUT);
    Serial.println("[TouchSensor] Initialized (GPIO 48).");
}

void TouchSensorController::update() {
    int pinReading = digitalRead(PIN_TOUCH_SENSOR);
    unsigned long now = millis();

    if (pinReading != lastPinState) {
        lastChangeTime = now;
        lastPinState = pinReading;
    }

    if ((now - lastChangeTime) > debounceTime) {
        bool pressed = (pinReading == HIGH); // TTP223 outputs HIGH on contact

        if (pressed && !currentlyPressed) {
            // Touch Press Start
            currentlyPressed = true;
            pressStartTime = now;
            longPressTriggered = false;

            AppEvent ev{};
            ev.type = EventType::TOUCH_SENSOR_PRESS;
            ev.data.boolValue = true;
            snprintf(ev.text, sizeof(ev.text), "touch:press");
            EventManager::getInstance().postEvent(ev);

            Serial.println("[TouchSensor] Press detected (PTT Start).");
        } else if (!pressed && currentlyPressed) {
            // Touch Released
            currentlyPressed = false;

            AppEvent ev{};
            ev.type = EventType::TOUCH_SENSOR_RELEASE;
            ev.data.boolValue = false;
            snprintf(ev.text, sizeof(ev.text), "touch:release");
            EventManager::getInstance().postEvent(ev);

            Serial.println("[TouchSensor] Release detected (PTT Stop).");
        } else if (pressed && currentlyPressed && !longPressTriggered) {
            // Check for long press (Provisioning trigger)
            if ((now - pressStartTime) >= longPressThreshold) {
                longPressTriggered = true;

                AppEvent ev{};
                ev.type = EventType::TOUCH_SENSOR_LONG_PRESS;
                ev.data.boolValue = true;
                snprintf(ev.text, sizeof(ev.text), "touch:long_press");
                EventManager::getInstance().postEvent(ev);

                Serial.println("[TouchSensor] LONG PRESS DETECTED -> Provisioning Mode Triggered!");
            }
        }
    }
}

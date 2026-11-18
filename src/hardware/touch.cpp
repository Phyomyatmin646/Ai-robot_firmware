#include "hardware/touch.h"

const char* TouchSensorController::getHardwareStatusStr() const {
    switch (hwStatus) {
        case TouchSensorHwStatus::IDLE_READY:
            return "IDLE_READY (GPIO pulled DOWN, stable LOW)";
        case TouchSensorHwStatus::UNSTABLE_FLOATING:
            return "UNSTABLE_FLOATING (Intermittent reading)";
        case TouchSensorHwStatus::STUCK_HIGH:
            return "STUCK_HIGH (Held HIGH at boot)";
        default:
            return "UNINITIALIZED";
    }
}

void TouchSensorController::begin() {
    // Configure internal pull-down so disconnected floating pins stay solidly at LOW (0V).
    pinMode(PIN_TOUCH_SENSOR, INPUT_PULLDOWN);
    delay(5); // Allow internal pull-down resistor to stabilize

    // Stability probe: Read 20 times across 10ms to verify line stability
    int highCount = 0;
    for (int i = 0; i < 20; i++) {
        if (digitalRead(PIN_TOUCH_SENSOR) == HIGH) {
            highCount++;
        }
        delayMicroseconds(500);
    }

    if (highCount == 0) {
        hwStatus = TouchSensorHwStatus::IDLE_READY;
        lastPinState = LOW;
        currentlyPressed = false;
        Serial.printf("[TouchSensor] Driver: DRIVER_INITIALIZED | Hardware: IDLE_READY (GPIO %d pulled DOWN, stable LOW idle).\n", PIN_TOUCH_SENSOR);
    } else if (highCount == 20) {
        hwStatus = TouchSensorHwStatus::STUCK_HIGH;
        lastPinState = HIGH;
        Serial.printf("[TouchSensor] Driver: DRIVER_INITIALIZED | Hardware: WARNING_STUCK_HIGH (GPIO %d held HIGH at boot - touched or shorted).\n", PIN_TOUCH_SENSOR);
    } else {
        hwStatus = TouchSensorHwStatus::UNSTABLE_FLOATING;
        Serial.printf("[TouchSensor] Driver: DRIVER_INITIALIZED | Hardware: UNSTABLE_FLOATING (GPIO %d noisy %d/20 readings).\n", PIN_TOUCH_SENSOR, highCount);
    }
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
            // Head Touch Contact
            currentlyPressed = true;
            pressStartTime = now;
            longPressTriggered = false;

            AppEvent ev{};
            ev.type = EventType::TOUCH_SENSOR_PRESS;
            ev.data.boolValue = true;
            snprintf(ev.text, sizeof(ev.text), "touch:press");
            EventManager::getInstance().postEvent(ev);

            Serial.println("[TouchSensor] Head touch detected.");
        } else if (!pressed && currentlyPressed) {
            // Head Touch Release
            currentlyPressed = false;

            AppEvent ev{};
            ev.type = EventType::TOUCH_SENSOR_RELEASE;
            ev.data.boolValue = false;
            snprintf(ev.text, sizeof(ev.text), "touch:release");
            EventManager::getInstance().postEvent(ev);

            Serial.println("[TouchSensor] Head touch released.");
        } else if (pressed && currentlyPressed && !longPressTriggered) {
            // Check for long press (Provisioning portal trigger)
            if ((now - pressStartTime) >= longPressThreshold) {
                longPressTriggered = true;

                AppEvent ev{};
                ev.type = EventType::TOUCH_SENSOR_LONG_PRESS;
                ev.data.boolValue = true;
                snprintf(ev.text, sizeof(ev.text), "touch:long_press");
                EventManager::getInstance().postEvent(ev);

                Serial.println("[TouchSensor] LONG PRESS DETECTED -> Launching Provisioning Mode!");
            }
        }
    }
}

#include "hardware/buttons.h"
#include "services/config_manager.h"

void ButtonController::begin() {
    pinMode(PIN_BTN_VOL_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_VOL_DOWN, INPUT_PULLUP);
    Serial.println("[ButtonController] Driver: DRIVER_INITIALIZED | Hardware: IDLE_READY (GPIO 41 & 42 INPUT_PULLUP, active LOW).");
}

void ButtonController::update() {
    unsigned long now = millis();

    // 1. Volume Up Button
    int readingUp = digitalRead(PIN_BTN_VOL_UP);
    if (readingUp != lastStateUp) {
        lastDebounceTimeUp = now;
    }
    if ((now - lastDebounceTimeUp) > debounceDelay) {
        if (readingUp != currentStateUp) {
            currentStateUp = readingUp;
            if (currentStateUp == LOW) { // Button pressed
                uint8_t currentVol = ConfigManager::getInstance().getVolume();
                uint8_t maxLim = ConfigManager::getInstance().getMaxVolumeLimit();
                uint8_t newVol = (currentVol + VOLUME_STEP <= maxLim) ? (currentVol + VOLUME_STEP) : maxLim;
                ConfigManager::getInstance().setVolume(newVol);

                AppEvent ev{};
                ev.type = EventType::BUTTON_VOL_UP_CLICK;
                ev.data.intValue = newVol;
                snprintf(ev.text, sizeof(ev.text), "vol:%u", newVol);
                EventManager::getInstance().postEvent(ev);

                Serial.printf("[Button] Vol Up Pressed -> Volume: %u%%\n", newVol);
            }
        }
    }
    lastStateUp = readingUp;

    // 2. Volume Down Button
    int readingDown = digitalRead(PIN_BTN_VOL_DOWN);
    if (readingDown != lastStateDown) {
        lastDebounceTimeDown = now;
    }
    if ((now - lastDebounceTimeDown) > debounceDelay) {
        if (readingDown != currentStateDown) {
            currentStateDown = readingDown;
            if (currentStateDown == LOW) { // Button pressed
                uint8_t currentVol = ConfigManager::getInstance().getVolume();
                uint8_t newVol = (currentVol >= VOLUME_STEP) ? (currentVol - VOLUME_STEP) : 0;
                ConfigManager::getInstance().setVolume(newVol);

                AppEvent ev{};
                ev.type = EventType::BUTTON_VOL_DOWN_CLICK;
                ev.data.intValue = newVol;
                snprintf(ev.text, sizeof(ev.text), "vol:%u", newVol);
                EventManager::getInstance().postEvent(ev);

                Serial.printf("[Button] Vol Down Pressed -> Volume: %u%%\n", newVol);
            }
        }
    }
    lastStateDown = readingDown;
}

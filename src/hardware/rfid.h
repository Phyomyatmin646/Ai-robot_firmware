#pragma once

#include <Arduino.h>
#include <Adafruit_PN532.h>
#include "pins.h"
#include "services/event_manager.h"

class RFIDController {
public:
    static RFIDController& getInstance() {
        static RFIDController instance;
        return instance;
    }

    bool begin();
    void update(); // Non-blocking card polling (e.g. called in sensor task)

    bool isModuleAvailable() const { return moduleReady; }
    String getLastUid() const { return lastUidStr; }

private:
    RFIDController();

    HardwareSerial nfcSerial;
    Adafruit_PN532 nfc;

    bool moduleReady = false;
    unsigned long lastScanTime = 0;
    const unsigned long duplicateCooldownMs = 2500; // 2.5s cooldown for duplicate card reads

    uint8_t lastUid[7] = {0};
    uint8_t lastUidLen = 0;
    String lastUidStr = "";
};

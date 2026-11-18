#pragma once

#include <Arduino.h>
#include <Adafruit_PN532.h>
#include "pins.h"
#include "hardware/rfid_format.h"
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
    const char* getHardwareStatusStr() const {
        return moduleReady ? "HARDWARE_RESPONDING (PN532 active on UART1)" : "NOT_DETECTED (UART1 timeout, no response)";
    }
    String getLastUid() const { return lastUidStr; }
    String getLastCardData() const { return lastCardDataJson; }

    // Multi-sector educational card writing utility
    bool writeEducationalCard(uint8_t* uid, uint8_t uidLength, const String& jsonPayload, RfidLanguage lang = RfidLanguage::BURMESE);

private:
    RFIDController();

    HardwareSerial nfcSerial;
    Adafruit_PN532 nfc;

    bool moduleReady = false;
    unsigned long lastScanTime = 0;
    const unsigned long duplicateCooldownMs = 3000; // 3.0s cooldown for duplicate card reads

    uint8_t lastUid[7] = {0};
    uint8_t lastUidLen = 0;
    String lastUidStr = "";
    String lastCardDataJson = "";

    // Multi-sector card reader across Sectors 1 through 10 (up to 480 bytes)
    bool tryReadMifareData(uint8_t* uid, uint8_t uidLength, String& outJson);
};

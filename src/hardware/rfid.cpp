#include "hardware/rfid.h"

RFIDController::RFIDController() 
    : nfcSerial(PN532_UART_NUM),
      nfc(PIN_PN532_RX, PIN_PN532_TX) // Use hardware UART configured via nfcSerial
{
}

bool RFIDController::begin() {
    Serial.println("[RFIDController] Initializing PN532 over UART (RX: GPIO 1, TX: GPIO 2)...");
    
    // Initialize Hardware UART1 at 115200 baud for PN532 High Speed UART
    nfcSerial.begin(115200, SERIAL_8N1, PIN_PN532_RX, PIN_PN532_TX);
    
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("[RFIDController] WARNING: PN532 module NOT found or not responding on UART1.");
        Serial.println("[RFIDController] Check DIP switches: [SEL0=0, SEL1=0] for HSU mode.");
        moduleReady = false;
        return false;
    }

    Serial.printf("[RFIDController] Found chip PN5%02X, Firmware ver. %d.%d\n",
                  (versiondata >> 24) & 0xFF,
                  (versiondata >> 16) & 0xFF,
                  (versiondata >> 8) & 0xFF);

    // Configure board to read RFID tags (SAMConfig: Normal mode)
    nfc.SAMConfig();
    
    // Set max retry to 1 to ensure non-blocking operation
    nfc.setPassiveActivationRetries(0x01);

    moduleReady = true;
    Serial.println("[RFIDController] PN532 RFID reader successfully initialized.");
    return true;
}

void RFIDController::update() {
    if (!moduleReady) return;

    unsigned long now = millis();
    uint8_t uid[7] = {0};
    uint8_t uidLength = 0;

    // Fast non-blocking read attempt for ISO14443A (Mifare) tags
    bool success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 25);

    if (success && uidLength > 0) {
        // Format UID to uppercase hex string
        char hexBuffer[16] = {0};
        for (uint8_t i = 0; i < uidLength; i++) {
            snprintf(hexBuffer + (i * 2), sizeof(hexBuffer) - (i * 2), "%02X", uid[i]);
        }
        String currentUidStr = String(hexBuffer);

        // Check duplicate suppression
        bool isDuplicate = (currentUidStr == lastUidStr) && ((now - lastScanTime) < duplicateCooldownMs);

        if (!isDuplicate) {
            lastScanTime = now;
            lastUidStr = currentUidStr;
            memcpy(lastUid, uid, uidLength);
            lastUidLen = uidLength;

            AppEvent ev{};
            ev.type = EventType::RFID_TAG_DETECTED;
            snprintf(ev.text, sizeof(ev.text), "%s", currentUidStr.c_str());
            EventManager::getInstance().postEvent(ev);

            Serial.printf("[RFID] Scanned Tag UID: %s (Length: %d)\n", currentUidStr.c_str(), uidLength);
        }
    } else {
        // Tag removed from field after cooldown
        if (lastUidStr.length() > 0 && (now - lastScanTime) > 1000) {
            lastUidStr = "";
        }
    }
}

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
    Serial.println("[RFIDController] Driver: DRIVER_INITIALIZED (UART1 at 115200 baud, GPIO 1/2).");

    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("[RFIDController] Hardware: NOT_DETECTED (PN532 module not responding on UART1).");
        Serial.println("[RFIDController] Check wiring & DIP switches: [SEL0=0, SEL1=0] for HSU mode.");
        moduleReady = false;
        return false;
    }

    Serial.printf("[RFIDController] Hardware: HARDWARE_RESPONDING (Chip PN5%02X, Firmware ver. %d.%d)\n",
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

bool RFIDController::tryReadMifareData(uint8_t* uid, uint8_t uidLength, String& outJson) {
    outJson = "";
    if (!moduleReady || uidLength != 4) {
        // MIFARE Classic 1K standard uses 4-byte UIDs
        return false;
    }

    // Default factory Key A for MIFARE Classic
    uint8_t defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Buffer to hold up to 10 sectors of continuous payload data
    // 10 sectors * 3 data blocks * 16 bytes = 480 bytes
    uint8_t payloadBuffer[480] = {0};
    size_t totalBytesRead = 0;

    for (uint8_t sector = RFID_DATA_START_SECTOR; sector <= RFID_DATA_MAX_SECTOR; sector++) {
        uint8_t firstBlock = sector * RFID_BLOCKS_PER_SECTOR;

        // Authenticate sector with Key A
        bool auth = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, firstBlock, 0, defaultKey);
        if (!auth) {
            if (sector == RFID_DATA_START_SECTOR) {
                // If Sector 1 authentication fails, card is not authenticated
                return false;
            }
            // Stop reading further sectors if subsequent sector fails
            break;
        }

        // Read strictly the 3 Data Blocks in this sector
        // Block 0, 1, 2 = Data Blocks
        // Block 3 = Sector Trailer (CRITICAL: NEVER read as payload)
        for (uint8_t blkOffset = 0; blkOffset < 3; blkOffset++) {
            uint8_t blockNum = firstBlock + blkOffset;
            uint8_t blockData[RFID_BYTES_PER_BLOCK] = {0};

            if (nfc.mifareclassic_ReadDataBlock(blockNum, blockData)) {
                if (totalBytesRead + RFID_BYTES_PER_BLOCK <= sizeof(payloadBuffer)) {
                    memcpy(payloadBuffer + totalBytesRead, blockData, RFID_BYTES_PER_BLOCK);
                    totalBytesRead += RFID_BYTES_PER_BLOCK;
                }
            } else {
                // If reading a block fails, stop further reads
                break;
            }
        }
    }

    if (totalBytesRead < sizeof(RfidCardHeader)) {
        return false;
    }

    // Parse versioned binary format or raw JSON
    return RfidFormatUtils::parseCardBuffer(payloadBuffer, totalBytesRead, outJson);
}

bool RFIDController::writeEducationalCard(uint8_t* uid, uint8_t uidLength, const String& jsonPayload, RfidLanguage lang) {
    if (!moduleReady || uidLength != 4) {
        return false;
    }

    uint8_t payloadBuffer[480] = {0};
    size_t packedBytes = 0;

    if (!RfidFormatUtils::packCardBuffer(jsonPayload, lang, payloadBuffer, sizeof(payloadBuffer), packedBytes)) {
        Serial.println("[RFIDController] Failed to pack card buffer: payload too large or invalid.");
        return false;
    }

    uint8_t defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    size_t bytesWritten = 0;
    uint8_t sector = RFID_DATA_START_SECTOR;

    while (bytesWritten < packedBytes && sector <= RFID_DATA_MAX_SECTOR) {
        uint8_t firstBlock = sector * RFID_BLOCKS_PER_SECTOR;

        bool auth = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, firstBlock, 0, defaultKey);
        if (!auth) {
            Serial.printf("[RFIDController] Sector %u auth failed during write.\n", sector);
            return false;
        }

        // Write only Data Blocks 0, 1, 2
        // NEVER overwrite Block 3 (Sector Trailer)
        for (uint8_t blkOffset = 0; blkOffset < 3 && bytesWritten < packedBytes; blkOffset++) {
            uint8_t blockNum = firstBlock + blkOffset;
            uint8_t blockData[RFID_BYTES_PER_BLOCK] = {0};

            size_t chunk = (packedBytes - bytesWritten < RFID_BYTES_PER_BLOCK)
                         ? (packedBytes - bytesWritten)
                         : RFID_BYTES_PER_BLOCK;
            memcpy(blockData, payloadBuffer + bytesWritten, chunk);

            if (!nfc.mifareclassic_WriteDataBlock(blockNum, blockData)) {
                Serial.printf("[RFIDController] Failed to write block %u.\n", blockNum);
                return false;
            }
            bytesWritten += RFID_BYTES_PER_BLOCK;
        }
        sector++;
    }

    Serial.printf("[RFIDController] Successfully wrote %u bytes across %u sectors.\n", packedBytes, sector - RFID_DATA_START_SECTOR);
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

            // Attempt multi-sector educational card read across Sectors 1..10
            lastCardDataJson = "";
            if (tryReadMifareData(uid, uidLength, lastCardDataJson)) {
                Serial.printf("[RFID] Multi-sector educational data verified: %s\n", lastCardDataJson.c_str());
            } else {
                Serial.printf("[RFID] Scanned card UID: %s (No valid multi-sector educational payload)\n", currentUidStr.c_str());
            }

            AppEvent ev{};
            ev.type = EventType::RFID_TAG_DETECTED;
            snprintf(ev.text, sizeof(ev.text), "%s", currentUidStr.c_str());
            EventManager::getInstance().postEvent(ev);
        }
    } else {
        // Tag removed from field after cooldown
        if (lastUidStr.length() > 0 && (now - lastScanTime) > 1200) {
            lastUidStr = "";
            lastCardDataJson = "";
        }
    }
}

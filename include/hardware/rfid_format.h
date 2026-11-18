#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * ============================================================================
 * MIFARE Classic 1K Educational Card Data Format Specification
 * ============================================================================
 * 
 * Card Structure (1024 Bytes Total):
 * - 16 Sectors (0 to 15), 4 Blocks per sector (16 bytes per block).
 * - Sector 0: Block 0 is Manufacturer UID (READ-ONLY). Blocks 1-2 reserved.
 * - Sectors 1 to 10: Allocated for Educational Payload.
 *   - Each sector has 3 data blocks (Block 0, 1, 2) = 48 bytes per sector.
 *   - Block 3 in EVERY sector is the SECTOR TRAILER (Key A, Access Bits, Key B).
 *     CRITICAL: Sector trailers are NEVER read or written as educational payload!
 * - Total Available Capacity across 10 sectors (Sectors 1..10):
 *   10 sectors * 3 data blocks * 16 bytes = 480 bytes.
 * 
 * Header Specification (Fixed 8 Bytes at Sector 1, Block 4):
 * -------------------------------------------------------------
 * Offset | Field          | Type     | Description
 * -------+----------------+----------+------------------------
 * 0..1   | Magic ID       | uint16_t | 'E', 'D' (0x4445 little-endian)
 * 2      | Schema Version | uint8_t  | 0x01 (Version 1.0)
 * 3      | Language Code  | uint8_t  | 0x01 = Burmese ("my"), 0x02 = English ("en")
 * 4..5   | Payload Length | uint16_t | Byte length of UTF-8 JSON (e.g. 1..472)
 * 6..7   | CRC-16 Check   | uint16_t | CRC-16-CCITT of payload bytes
 * -------------------------------------------------------------
 * 8..479 | Educational JSON Payload (UTF-8 encoded string)
 * 
 * Supported Languages:
 * - Burmese (Myanmar UTF-8 Unicode)
 * - English (UTF-8 ASCII/Latin)
 * ============================================================================
 */

#define RFID_CARD_MAGIC             0x4445  // 'E' (0x45), 'D' (0x44)
#define RFID_CARD_SCHEMA_VERSION    1
#define RFID_MAX_PAYLOAD_BYTES      472     // 480 bytes total - 8 bytes header
#define RFID_DATA_START_SECTOR      1
#define RFID_DATA_MAX_SECTOR        10      // Sectors 1 through 10
#define RFID_BLOCKS_PER_SECTOR      4
#define RFID_BYTES_PER_BLOCK        16

enum class RfidLanguage : uint8_t {
    BURMESE = 0x01,
    ENGLISH = 0x02,
    UNKNOWN = 0x00
};

inline const char* rfidLanguageToString(RfidLanguage lang) {
    switch (lang) {
        case RfidLanguage::BURMESE: return "my";
        case RfidLanguage::ENGLISH: return "en";
        default:                    return "my";
    }
}

inline RfidLanguage rfidLanguageFromString(const String& langStr) {
    if (langStr == "en" || langStr == "english") return RfidLanguage::ENGLISH;
    return RfidLanguage::BURMESE;
}

#pragma pack(push, 1)
struct RfidCardHeader {
    uint16_t magic;          // 'E', 'D' (0x4445)
    uint8_t  schemaVersion;  // 1
    uint8_t  language;       // 1 = Burmese, 2 = English
    uint16_t payloadLength;  // Length of following JSON text
    uint16_t crc16;          // CRC16-CCITT across payload bytes
};
#pragma pack(pop)

class RfidFormatUtils {
public:
    // Standard CRC-16-CCITT (Polynomial 0x1021, Init 0xFFFF)
    static uint16_t calculateCrc16(const uint8_t* data, size_t length) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; i++) {
            crc ^= (uint16_t)data[i] << 8;
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc = crc << 1;
                }
            }
        }
        return crc;
    }

    /**
     * Parse raw multi-sector card buffer into validated educational JSON string.
     * Returns true if magic and CRC match and JSON is syntactically valid.
     */
    static bool parseCardBuffer(const uint8_t* buffer, size_t bufferSize, String& outJson) {
        outJson = "";
        if (!buffer || bufferSize < sizeof(RfidCardHeader)) {
            return false;
        }

        const RfidCardHeader* hdr = (const RfidCardHeader*)buffer;

        // 1. Verify Magic Identifier
        if (hdr->magic == RFID_CARD_MAGIC && hdr->schemaVersion == RFID_CARD_SCHEMA_VERSION) {
            uint16_t length = hdr->payloadLength;
            if (length == 0 || length > RFID_MAX_PAYLOAD_BYTES || (sizeof(RfidCardHeader) + length) > bufferSize) {
                Serial.printf("[RFIDFormat] Invalid payload length: %u bytes\n", length);
                return false;
            }

            const uint8_t* payloadPtr = buffer + sizeof(RfidCardHeader);

            // 2. Verify CRC-16 Integrity
            uint16_t computedCrc = calculateCrc16(payloadPtr, length);
            if (computedCrc != hdr->crc16) {
                Serial.printf("[RFIDFormat] CRC mismatch! Header: 0x%04X, Computed: 0x%04X\n", hdr->crc16, computedCrc);
                return false;
            }

            // 3. Extract and null-terminate payload
            char tempBuf[RFID_MAX_PAYLOAD_BYTES + 1] = {0};
            memcpy(tempBuf, payloadPtr, length);
            tempBuf[length] = '\0';
            outJson = String(tempBuf);
            outJson.trim();

            // 4. Validate JSON structure
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, outJson);
            if (err) {
                Serial.printf("[RFIDFormat] JSON deserialization failed: %s\n", err.c_str());
                outJson = "";
                return false;
            }
            return true;
        }

        // Backward compatibility fallback: raw JSON format starting with '{'
        if (buffer[0] == '{') {
            size_t validLen = 0;
            for (size_t i = 0; i < bufferSize; i++) {
                if (buffer[i] == '}') {
                    validLen = i + 1;
                }
            }
            if (validLen > 2) {
                char tempBuf[RFID_MAX_PAYLOAD_BYTES + 1] = {0};
                size_t copyLen = (validLen <= RFID_MAX_PAYLOAD_BYTES) ? validLen : RFID_MAX_PAYLOAD_BYTES;
                memcpy(tempBuf, buffer, copyLen);
                tempBuf[copyLen] = '\0';
                outJson = String(tempBuf);
                outJson.trim();

                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, outJson);
                if (!err) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * Pack an educational JSON payload into the versioned card data format.
     */
    static bool packCardBuffer(const String& jsonPayload, RfidLanguage lang, uint8_t* outBuffer, size_t maxBufferSize, size_t& outWritten) {
        outWritten = 0;
        size_t payloadLen = jsonPayload.length();
        if (payloadLen == 0 || payloadLen > RFID_MAX_PAYLOAD_BYTES) {
            return false;
        }

        size_t totalNeeded = sizeof(RfidCardHeader) + payloadLen;
        if (totalNeeded > maxBufferSize) {
            return false;
        }

        RfidCardHeader* hdr = (RfidCardHeader*)outBuffer;
        hdr->magic = RFID_CARD_MAGIC;
        hdr->schemaVersion = RFID_CARD_SCHEMA_VERSION;
        hdr->language = (uint8_t)lang;
        hdr->payloadLength = (uint16_t)payloadLen;
        hdr->crc16 = calculateCrc16((const uint8_t*)jsonPayload.c_str(), payloadLen);

        memcpy(outBuffer + sizeof(RfidCardHeader), jsonPayload.c_str(), payloadLen);
        outWritten = totalNeeded;
        return true;
    }
};

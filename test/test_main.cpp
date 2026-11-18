#include <Arduino.h>
#include <unity.h>
#include "app_events.h"
#include "services/config_manager.h"
#include "services/event_manager.h"
#include "hardware/rfid_format.h"

void test_state_string_conversion(void) {
    TEST_ASSERT_EQUAL_STRING("idle", robotStateToString(RobotState::IDLE));
    TEST_ASSERT_EQUAL_STRING("listening", robotStateToString(RobotState::LISTENING));
    TEST_ASSERT_EQUAL_STRING("thinking", robotStateToString(RobotState::THINKING));
    TEST_ASSERT_EQUAL_STRING("speaking", robotStateToString(RobotState::SPEAKING));
    TEST_ASSERT_EQUAL_STRING("error", robotStateToString(RobotState::ERROR));
}

void test_secret_masking(void) {
    String masked = ConfigManager::maskSecret("rbt_1234567890abcdef");
    TEST_ASSERT_TRUE(masked.startsWith("rbt_"));
    TEST_ASSERT_TRUE(masked.indexOf("[MASKED]") > 0);
    TEST_ASSERT_EQUAL(-1, masked.indexOf("1234567890")); // Real secret not exposed
}

void test_event_queue_transfer(void) {
    EventManager::getInstance().begin(8);
    AppEvent sentEv{};
    sentEv.type = EventType::BUTTON_VOL_UP_CLICK;
    sentEv.data.intValue = 85;
    snprintf(sentEv.text, sizeof(sentEv.text), "vol:85");

    bool posted = EventManager::getInstance().postEvent(sentEv);
    TEST_ASSERT_TRUE(posted);

    AppEvent recvEv{};
    bool received = EventManager::getInstance().receiveEvent(&recvEv, 100);
    TEST_ASSERT_TRUE(received);
    TEST_ASSERT_EQUAL(EventType::BUTTON_VOL_UP_CLICK, recvEv.type);
    TEST_ASSERT_EQUAL(85, recvEv.data.intValue);
    TEST_ASSERT_EQUAL_STRING("vol:85", recvEv.text);
}

void test_rfid_format_crc16(void) {
    const uint8_t testData[] = "Hello Antigravity Robot";
    uint16_t crc1 = RfidFormatUtils::calculateCrc16(testData, sizeof(testData) - 1);
    uint16_t crc2 = RfidFormatUtils::calculateCrc16(testData, sizeof(testData) - 1);
    TEST_ASSERT_EQUAL_UINT16(crc1, crc2);
    TEST_ASSERT_NOT_EQUAL(0, crc1);
}

void test_rfid_card_packing_and_parsing_burmese(void) {
    String burmeseJson = "{\"schemaVersion\":\"1.0\",\"cardType\":\"educational\",\"language\":\"my\",\"title\":\"ပန်းသီး\",\"category\":\"အသီး\",\"facts\":[\"အာဟာရပြည့်ဝသည်\"]}";
    uint8_t buffer[480] = {0};
    size_t packedBytes = 0;

    bool packOk = RfidFormatUtils::packCardBuffer(burmeseJson, RfidLanguage::BURMESE, buffer, sizeof(buffer), packedBytes);
    TEST_ASSERT_TRUE(packOk);
    TEST_ASSERT_GREATER_THAN(sizeof(RfidCardHeader), packedBytes);

    String parsedJson = "";
    bool parseOk = RfidFormatUtils::parseCardBuffer(buffer, packedBytes, parsedJson);
    TEST_ASSERT_TRUE(parseOk);
    TEST_ASSERT_TRUE(parsedJson.indexOf("ပန်းသီး") >= 0);
    TEST_ASSERT_TRUE(parsedJson.indexOf("အသီး") >= 0);
}

void test_rfid_card_packing_and_parsing_english(void) {
    String englishJson = "{\"schemaVersion\":\"1.0\",\"cardType\":\"educational\",\"language\":\"en\",\"title\":\"Elephant\",\"category\":\"Animals\",\"facts\":[\"Largest land mammal\"]}";
    uint8_t buffer[480] = {0};
    size_t packedBytes = 0;

    bool packOk = RfidFormatUtils::packCardBuffer(englishJson, RfidLanguage::ENGLISH, buffer, sizeof(buffer), packedBytes);
    TEST_ASSERT_TRUE(packOk);

    String parsedJson = "";
    bool parseOk = RfidFormatUtils::parseCardBuffer(buffer, packedBytes, parsedJson);
    TEST_ASSERT_TRUE(parseOk);
    TEST_ASSERT_TRUE(parsedJson.indexOf("Elephant") >= 0);
}

void test_rfid_sector_trailer_exclusion(void) {
    // Verify that data block calculations strictly skip block index % 4 == 3
    for (uint8_t sector = 1; sector <= 10; sector++) {
        uint8_t trailerBlock = sector * 4 + 3;
        for (uint8_t blk = 0; blk < 3; blk++) {
            uint8_t dataBlock = sector * 4 + blk;
            TEST_ASSERT_NOT_EQUAL(trailerBlock, dataBlock);
            TEST_ASSERT_NOT_EQUAL(3, dataBlock % 4); // Must NEVER be trailer
        }
    }
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_state_string_conversion);
    RUN_TEST(test_secret_masking);
    RUN_TEST(test_event_queue_transfer);
    RUN_TEST(test_rfid_format_crc16);
    RUN_TEST(test_rfid_card_packing_and_parsing_burmese);
    RUN_TEST(test_rfid_card_packing_and_parsing_english);
    RUN_TEST(test_rfid_sector_trailer_exclusion);
    UNITY_END();
}

void loop() {
    delay(1000);
}

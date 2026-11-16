#include <Arduino.h>
#include <unity.h>
#include "app_events.h"
#include "services/config_manager.h"
#include "services/event_manager.h"

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

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_state_string_conversion);
    RUN_TEST(test_secret_masking);
    RUN_TEST(test_event_queue_transfer);
    UNITY_END();
}

void loop() {
    delay(1000);
}

#pragma once

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "hardware/display.h"
#include "app_events.h"

enum class CurrentScreen {
    ROBOT_FACE,
    SETTINGS_VIEW
};

class UIManager {
public:
    static UIManager& getInstance() {
        static UIManager instance;
        return instance;
    }

    bool begin();
    void update(); // Called in UI task loop (~30 FPS)

    void setScreen(CurrentScreen screen) { activeScreen = screen; }
    CurrentScreen getScreen() const { return activeScreen; }
    void toggleScreen();

    // Process on-screen touch interactions
    void processTouch();

private:
    UIManager() = default;

    void drawStatusBar();
    void drawRobotFace();
    void drawSettingsScreen();
    void handleScreenTouch(int32_t x, int32_t y);

    // Robot Facial Expressions
    void drawIdleFace(int eyeBlink);
    void drawListeningFace(unsigned long timeMs);
    void drawThinkingFace(unsigned long timeMs);
    void drawSpeakingFace(unsigned long timeMs);
    void drawErrorFace();

    LGFX_Sprite canvas{&DisplayController::getInstance().getDisplay()};
    CurrentScreen activeScreen = CurrentScreen::ROBOT_FACE;

    unsigned long lastFrameTime = 0;
    unsigned long lastBlinkTime = 0;
    int blinkPhase = 0; // 0: Open, 1: Closing, 2: Closed, 3: Opening

    // Touch Screen Interactive Features
    int lookOffsetX = 0;
    unsigned long lookResetTime = 0;
    bool isWinking = false;
    unsigned long winkEndTime = 0;
    unsigned long lastTouchDebounce = 0;
    int32_t lastTouchRippleX = -1;
    int32_t lastTouchRippleY = -1;
    unsigned long rippleEndTime = 0;
};

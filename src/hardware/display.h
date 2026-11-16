#pragma once

#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "pins.h"
#include "config.h"

// Custom LovyanGFX Board Setup for ILI9341 on ESP32-S3 V2775
class LGFX_ESP32S3_V2775 : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
#if FEATURE_TOUCH_XPT2046_PRESENT
    lgfx::Touch_XPT2046 _touch_instance;
#endif
public:
    LGFX_ESP32S3_V2775();
};

class DisplayController {
public:
    static DisplayController& getInstance() {
        static DisplayController instance;
        return instance;
    }

    bool begin();
    void setBrightness(uint8_t percentage);
    uint8_t getBrightness() const { return currentBrightness; }

    LGFX_ESP32S3_V2775& getDisplay() { return gfx; }

    // Screen dimensions
    int32_t width() const { return TFT_WIDTH; }
    int32_t height() const { return TFT_HEIGHT; }

    // Touch readout
    bool getTouch(int32_t* x, int32_t* y);

private:
    DisplayController() = default;

    LGFX_ESP32S3_V2775 gfx;
    uint8_t currentBrightness = DEFAULT_BRIGHTNESS;
    bool isInitialized = false;
};

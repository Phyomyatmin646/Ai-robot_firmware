#include "hardware/display.h"

LGFX_ESP32S3_V2775::LGFX_ESP32S3_V2775() {
    // 1. Configure SPI Bus
    {
        auto cfg = _bus_instance.config();
        cfg.spi_host = SPI2_HOST;
        cfg.spi_mode = 0;
        cfg.freq_write = 40000000; // 40 MHz SPI clock for fast refresh
        cfg.freq_read  = 16000000;
        cfg.spi_3wire  = false;
        cfg.use_lock   = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;
        cfg.pin_sclk = PIN_TFT_SCK;  // GPIO 12
        cfg.pin_mosi = PIN_TFT_MOSI; // GPIO 11
        cfg.pin_miso = PIN_TFT_MISO; // GPIO 13
        cfg.pin_dc   = PIN_TFT_DC;   // GPIO 9
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
    }

    // 2. Configure ILI9341 Panel
    {
        auto cfg = _panel_instance.config();
        cfg.pin_cs           = PIN_TFT_CS;  // GPIO 10
        cfg.pin_rst          = PIN_TFT_RST; // GPIO 14
        cfg.pin_busy         = -1;
        cfg.memory_width     = 240;
        cfg.memory_height    = 320;
        cfg.panel_width      = 240;
        cfg.panel_height     = 320;
        cfg.offset_x         = 0;
        cfg.offset_y         = 0;
        cfg.offset_rotation  = TFT_ROTATION; // 1: Landscape (320x240)
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = true;
        cfg.invert           = false;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = true;
        _panel_instance.config(cfg);
    }

    // 3. Configure Backlight PWM
    {
        auto cfg = _light_instance.config();
        cfg.pin_bl = PIN_TFT_BL; // GPIO 4
        cfg.invert = false;
        cfg.freq   = 44100;
        cfg.pwm_channel = 0;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
    }

#if FEATURE_TOUCH_XPT2046_PRESENT
    // 4. Configure XPT2046 Touch (if verified and present)
    {
        auto cfg = _touch_instance.config();
        cfg.x_min      = 0;
        cfg.x_max      = TFT_WIDTH - 1;
        cfg.y_min      = 0;
        cfg.y_max      = TFT_HEIGHT - 1;
        cfg.pin_int    = PIN_TOUCH_IRQ; // GPIO 18
        cfg.bus_shared = true;
        cfg.spi_host   = SPI2_HOST;
        cfg.freq       = 2500000;
        cfg.pin_sclk   = PIN_TFT_SCK;  // GPIO 12
        cfg.pin_mosi   = PIN_TFT_MOSI; // GPIO 11
        cfg.pin_miso   = PIN_TFT_MISO; // GPIO 13
        cfg.pin_cs     = PIN_TOUCH_CS;  // GPIO 8
        _touch_instance.config(cfg);
        _panel_instance.setTouch(&_touch_instance);
    }
#endif

    setPanel(&_panel_instance);
}

const char* DisplayController::getHardwareStatusStr() const {
    switch (hwStatus) {
        case DisplayHwStatus::HARDWARE_RESPONDING:
            return "HARDWARE_RESPONDING (Display active on SPI)";
        case DisplayHwStatus::NOT_DETECTED:
            return "NOT_DETECTED (SPI MISO silent/unconnected)";
        case DisplayHwStatus::DRIVER_INITIALIZED:
            return "DRIVER_INITIALIZED (SPI initialized)";
        default:
            return "UNINITIALIZED";
    }
}

bool DisplayController::begin() {
    if (isInitialized) return true;

    Serial.println("[Display] Initializing ILI9341 SPI Display Driver...");
    if (!gfx.init()) {
        Serial.println("[Display] ERROR: Failed to initialize LovyanGFX display driver!");
        hwStatus = DisplayHwStatus::UNINITIALIZED;
        return false;
    }

    isInitialized = true;
    hwStatus = DisplayHwStatus::DRIVER_INITIALIZED;
    Serial.println("[Display] Driver: DRIVER_INITIALIZED (SPI2_HOST, 40MHz write, GPIO 11/12/10/9/14/4).");

    gfx.setRotation(TFT_ROTATION);
    setBrightness(currentBrightness);
    gfx.fillScreen(TFT_BLACK);

    // Physical verification: Read data from display over MISO (GPIO 13).
    // An unconnected display has floating/silent MISO returning 0x00000000 or 0xFFFFFFFF.
    displayId = gfx.readData32();
    if (displayId == 0x00000000 || displayId == 0xFFFFFFFF) {
        hwStatus = DisplayHwStatus::NOT_DETECTED;
        Serial.printf("[Display] Hardware: NOT_DETECTED (SPI MISO returned 0x%08X; display unattached or write-only).\n", (unsigned int)displayId);
    } else {
        hwStatus = DisplayHwStatus::HARDWARE_RESPONDING;
        Serial.printf("[Display] Hardware: HARDWARE_RESPONDING (Read data: 0x%08X confirmed).\n", (unsigned int)displayId);
    }

    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.setTextSize(2);
    gfx.drawString("ESP32-S3 AI Robot", 20, 20);

    return true;
}

void DisplayController::setBrightness(uint8_t percentage) {
    if (percentage > 100) percentage = 100;
    currentBrightness = percentage;
    // LovyanGFX brightness range is 0 - 255
    uint8_t val255 = (uint8_t)((percentage * 255) / 100);
    gfx.setBrightness(val255);
}

bool DisplayController::getTouch(int32_t* x, int32_t* y) {
#if FEATURE_TOUCH_XPT2046_PRESENT
    if (!isInitialized) return false;
    return gfx.getTouch(x, y);
#else
    (void)x; (void)y;
    return false; // Touch not physically verified
#endif
}

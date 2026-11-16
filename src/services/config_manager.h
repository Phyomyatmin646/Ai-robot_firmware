#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    void begin();

    // Wi-Fi Credentials
    bool hasWifiCredentials() const;
    String getWifiSSID() const;
    String getWifiPassword() const;
    void setWifiCredentials(const String& ssid, const String& password);

    // Device Credentials
    bool hasDeviceCredentials() const;
    String getDeviceId() const;
    String getDeviceApiKey() const;
    void setDeviceCredentials(const String& deviceId, const String& apiKey);

    // Audio & Display Settings
    uint8_t getVolume() const;
    void setVolume(uint8_t volume);

    uint8_t getMaxVolumeLimit() const { return cachedMaxVolumeLimit; }
    void setMaxVolumeLimit(uint8_t limit);

    uint8_t getBrightness() const;
    void setBrightness(uint8_t brightness);

    // Factory Reset
    void clearAll();

    // Safe logging helper: never prints real secret to serial
    static String maskSecret(const String& secret);

private:
    ConfigManager() = default;
    Preferences prefsNet;
    Preferences prefsAuth;
    Preferences prefsCfg;

    String cachedSsid;
    String cachedPass;
    String cachedDeviceId;
    String cachedApiKey;
    uint8_t cachedVolume = DEFAULT_VOLUME;
    uint8_t cachedMaxVolumeLimit = CONFIG_SOFTWARE_MAX_VOLUME_LIMIT;
    uint8_t cachedBrightness = DEFAULT_BRIGHTNESS;
};

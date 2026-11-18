#include "services/config_manager.h"

void ConfigManager::begin() {
    prefsNet.begin(NVS_NAMESPACE_NET, false);
    cachedSsid = prefsNet.getString(NVS_KEY_SSID, "");
    cachedPass = prefsNet.getString(NVS_KEY_PASS, "");
    prefsNet.end();

    prefsAuth.begin(NVS_NAMESPACE_AUTH, false);
    cachedDeviceId = prefsAuth.getString(NVS_KEY_DEVICE_ID, "");
    cachedApiKey = prefsAuth.getString(NVS_KEY_API_KEY, "");
    prefsAuth.end();

    // Sanitize any existing whitespace loaded from NVS
    cachedDeviceId.trim();
    cachedApiKey.trim();

    prefsCfg.begin(NVS_NAMESPACE_CONFIG, false);
    cachedVolume = prefsCfg.getUChar(NVS_KEY_VOLUME, DEFAULT_VOLUME);
    cachedMaxVolumeLimit = prefsCfg.getUChar(NVS_KEY_MAX_VOL_LIMIT, CONFIG_SOFTWARE_MAX_VOLUME_LIMIT);
    cachedBrightness = prefsCfg.getUChar(NVS_KEY_BRIGHTNESS, DEFAULT_BRIGHTNESS);
    prefsCfg.end();

    Serial.println("[ConfigManager] Initialized.");
    if (hasWifiCredentials()) {
        Serial.printf("[ConfigManager] Loaded Wi-Fi SSID: %s\n", cachedSsid.c_str());
    } else {
        Serial.println("[ConfigManager] No Wi-Fi credentials stored in NVS.");
    }

    if (hasDeviceCredentials()) {
        Serial.printf("[ConfigManager] Loaded Device ID: %s, API Key: %s\n",
                      cachedDeviceId.c_str(),
                      maskSecret(cachedApiKey).c_str());
    } else {
        Serial.println("[ConfigManager] No Device credentials stored in NVS.");
    }
}

bool ConfigManager::hasWifiCredentials() const {
    return cachedSsid.length() > 0;
}

String ConfigManager::getWifiSSID() const {
    return cachedSsid;
}

String ConfigManager::getWifiPassword() const {
    return cachedPass;
}

void ConfigManager::setWifiCredentials(const String& ssid, const String& password) {
    cachedSsid = ssid;
    cachedPass = password;

    prefsNet.begin(NVS_NAMESPACE_NET, false);
    prefsNet.putString(NVS_KEY_SSID, ssid);
    prefsNet.putString(NVS_KEY_PASS, password);
    prefsNet.end();

    Serial.printf("[ConfigManager] Wi-Fi credentials saved: %s\n", ssid.c_str());
}

bool ConfigManager::hasDeviceCredentials() const {
    return cachedDeviceId.length() > 0 && cachedApiKey.length() > 0;
}

String ConfigManager::getDeviceId() const {
    return cachedDeviceId;
}

String ConfigManager::getDeviceApiKey() const {
    return cachedApiKey;
}

void ConfigManager::setDeviceCredentials(const String& deviceId, const String& apiKey) {
    cachedDeviceId = deviceId;
    cachedApiKey = apiKey;
    cachedDeviceId.trim();
    cachedApiKey.trim();

    prefsAuth.begin(NVS_NAMESPACE_AUTH, false);
    prefsAuth.putString(NVS_KEY_DEVICE_ID, cachedDeviceId);
    prefsAuth.putString(NVS_KEY_API_KEY, cachedApiKey);
    prefsAuth.end();

    Serial.printf("[ConfigManager] Device credentials saved: ID=%s, Key=%s\n",
                  cachedDeviceId.c_str(),
                  maskSecret(cachedApiKey).c_str());
}

uint8_t ConfigManager::getVolume() const {
    return cachedVolume;
}

void ConfigManager::setVolume(uint8_t volume) {
    if (volume > cachedMaxVolumeLimit) volume = cachedMaxVolumeLimit;
    cachedVolume = volume;

    prefsCfg.begin(NVS_NAMESPACE_CONFIG, false);
    prefsCfg.putUChar(NVS_KEY_VOLUME, volume);
    prefsCfg.end();
}

void ConfigManager::setMaxVolumeLimit(uint8_t limit) {
    cachedMaxVolumeLimit = (limit <= 100) ? limit : 100;
    prefsCfg.begin(NVS_NAMESPACE_CONFIG, false);
    prefsCfg.putUChar(NVS_KEY_MAX_VOL_LIMIT, cachedMaxVolumeLimit);
    prefsCfg.end();
    if (cachedVolume > cachedMaxVolumeLimit) {
        setVolume(cachedMaxVolumeLimit);
    }
}

uint8_t ConfigManager::getBrightness() const {
    return cachedBrightness;
}

void ConfigManager::setBrightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    cachedBrightness = brightness;

    prefsCfg.begin(NVS_NAMESPACE_CONFIG, false);
    prefsCfg.putUChar(NVS_KEY_BRIGHTNESS, brightness);
    prefsCfg.end();
}

void ConfigManager::clearAll() {
    prefsNet.begin(NVS_NAMESPACE_NET, false);
    prefsNet.clear();
    prefsNet.end();

    prefsAuth.begin(NVS_NAMESPACE_AUTH, false);
    prefsAuth.clear();
    prefsAuth.end();

    prefsCfg.begin(NVS_NAMESPACE_CONFIG, false);
    prefsCfg.clear();
    prefsCfg.end();

    cachedSsid = "";
    cachedPass = "";
    cachedDeviceId = "";
    cachedApiKey = "";
    cachedVolume = DEFAULT_VOLUME;
    cachedBrightness = DEFAULT_BRIGHTNESS;

    Serial.println("[ConfigManager] All NVS preferences wiped.");
}

String ConfigManager::maskSecret(const String& secret) {
    if (secret.length() <= 6) return "***[PROTECTED]***";
    return secret.substring(0, 4) + "***[MASKED]***" + secret.substring(secret.length() - 2);
}

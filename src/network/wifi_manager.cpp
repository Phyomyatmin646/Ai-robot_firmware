#include "network/wifi_manager.h"
#include <time.h>

void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // We control reconnection with custom backoff

    if (ConfigManager::getInstance().hasWifiCredentials()) {
        String ssid = ConfigManager::getInstance().getWifiSSID();
        String pass = ConfigManager::getInstance().getWifiPassword();
        Serial.printf("[WiFiManager] Connecting to SSID: %s ...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        state = WifiState::CONNECTING;
        lastReconnectAttempt = millis();
    } else {
        Serial.println("[WiFiManager] No credentials found. Starting Provisioning Portal...");
        startProvisioningPortal();
    }
}

void WiFiManager::update() {
    unsigned long now = millis();

    if (state == WifiState::PROVISIONING_PORTAL) {
        if (dnsServer) dnsServer->processNextRequest();
        if (server) server->handleClient();
        return;
    }

    if (state == WifiState::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            state = WifiState::CONNECTED;
            failedAttempts = 0;
            reconnectDelay = 2000;
            Serial.printf("[WiFiManager] Connected! IP: %s, RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());

            // Synchronize system time via SNTP for TLS certificates
            configTime(0, 0, "pool.ntp.org", "time.google.com");
            Serial.println("[WiFiManager] SNTP time synchronization requested.");

            AppEvent ev{};
            ev.type = EventType::NETWORK_CONNECTED;
            snprintf(ev.text, sizeof(ev.text), "%s", WiFi.localIP().toString().c_str());
            EventManager::getInstance().postEvent(ev);
        } else if (now - lastReconnectAttempt > 15000) {
            // Connection timeout
            state = WifiState::DISCONNECTED;
            failedAttempts++;
            lastReconnectAttempt = now;
            Serial.printf("[WiFiManager] Connection attempt %u timed out.\n", failedAttempts);

            if (failedAttempts >= 5 && !ConfigManager::getInstance().hasDeviceCredentials()) {
                startProvisioningPortal();
            }
        }
    } else if (state == WifiState::CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            state = WifiState::DISCONNECTED;
            lastReconnectAttempt = now;
            Serial.println("[WiFiManager] Wi-Fi link lost! Scheduling reconnect...");

            AppEvent ev{};
            ev.type = EventType::NETWORK_DISCONNECTED;
            snprintf(ev.text, sizeof(ev.text), "wifi:lost");
            EventManager::getInstance().postEvent(ev);
        }
    } else if (state == WifiState::DISCONNECTED) {
        if (now - lastReconnectAttempt > reconnectDelay) {
            lastReconnectAttempt = now;
            reconnectDelay = (reconnectDelay * 2 <= maxReconnectDelay) ? (reconnectDelay * 2) : maxReconnectDelay;

            String ssid = ConfigManager::getInstance().getWifiSSID();
            String pass = ConfigManager::getInstance().getWifiPassword();
            if (ssid.length() > 0) {
                Serial.printf("[WiFiManager] Reconnecting to %s (backoff: %lu ms)...\n", ssid.c_str(), (unsigned long)reconnectDelay);
                WiFi.disconnect(true);
                WiFi.begin(ssid.c_str(), pass.c_str());
                state = WifiState::CONNECTING;
            }
        }
    }
}

int8_t WiFiManager::getRSSI() const {
    if (state == WifiState::CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

String WiFiManager::getIpAddress() const {
    if (state == WifiState::CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

void WiFiManager::startProvisioningPortal() {
    state = WifiState::PROVISIONING_PORTAL;
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-Robot-Setup", "");

    dnsServer = std::make_unique<DNSServer>();
    dnsServer->start(53, "*", WiFi.softAPIP());

    server = std::make_unique<WebServer>(80);
    server->on("/", HTTP_GET, [this]() { handlePortalRoot(); });
    server->on("/save", HTTP_POST, [this]() { handlePortalSave(); });
    server->onNotFound([this]() {
        server->sendHeader("Location", "/", true);
        server->send(302, "text/plain", "");
    });
    server->begin();

    Serial.printf("[WiFiManager] Provisioning Portal running at http://%s\n",
                  WiFi.softAPIP().toString().c_str());
}

void WiFiManager::stopProvisioningPortal() {
    if (server) {
        server->stop();
        server.reset();
    }
    if (dnsServer) {
        dnsServer->stop();
        dnsServer.reset();
    }
    WiFi.softAPdisconnect(true);
    begin();
}

void WiFiManager::handlePortalRoot() {
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>AI Robot Setup</title><style>"
                  "body{font-family:Arial,sans-serif;background:#121212;color:#fff;padding:20px;text-align:center;}"
                  "input{width:90%;max-width:320px;padding:10px;margin:8px 0;border-radius:6px;border:1px solid #444;background:#222;color:#fff;}"
                  "button{background:#00bcd4;color:#000;font-weight:bold;padding:12px 30px;border:none;border-radius:6px;cursor:pointer;margin-top:12px;}"
                  ".card{background:#1e1e1e;max-width:400px;margin:0 auto;padding:24px;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,0.5);}"
                  "</style></head><body><div class='card'>"
                  "<h2>🤖 AI Robot Setup</h2>"
                  "<form action='/save' method='POST'>"
                  "<input type='text' name='ssid' placeholder='Wi-Fi SSID' required><br>"
                  "<input type='password' name='pass' placeholder='Wi-Fi Password'><br>"
                  "<input type='text' name='device_id' placeholder='Device ID' required><br>"
                  "<input type='password' name='api_key' placeholder='Device API Key (rbt_...)' required><br>"
                  "<button type='submit'>Save & Connect</button>"
                  "</form></div></body></html>";
    server->send(200, "text/html", html);
}

void WiFiManager::handlePortalSave() {
    if (!server) return;
    String ssid = server->arg("ssid");
    String pass = server->arg("pass");
    String devId = server->arg("device_id");
    String apiKey = server->arg("api_key");

    if (ssid.length() > 0 && devId.length() > 0 && apiKey.length() > 0) {
        ConfigManager::getInstance().setWifiCredentials(ssid, pass);
        ConfigManager::getInstance().setDeviceCredentials(devId, apiKey);

        String response = "<html><body style='background:#121212;color:#fff;text-align:center;font-family:Arial;padding:40px;'>"
                          "<h3>Credentials Saved!</h3><p>Robot is rebooting and connecting to backend...</p></body></html>";
        server->send(200, "text/html", response);
        delay(1500);
        ESP.restart();
    } else {
        server->send(400, "text/plain", "Missing required fields.");
    }
}

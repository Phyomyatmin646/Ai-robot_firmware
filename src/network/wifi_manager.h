#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "services/config_manager.h"
#include "services/event_manager.h"

enum class WifiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    PROVISIONING_PORTAL
};

class WiFiManager {
public:
    static WiFiManager& getInstance() {
        static WiFiManager instance;
        return instance;
    }

    void begin();
    void update(); // Called in network task loop

    bool isConnected() const { return state == WifiState::CONNECTED; }
    WifiState getState() const { return state; }
    int8_t getRSSI() const;
    String getIpAddress() const;

    // Trigger local Captive Portal for user setup
    void startProvisioningPortal();
    void stopProvisioningPortal();

private:
    WiFiManager() = default;

    void handlePortalRoot();
    void handlePortalSave();

    WifiState state = WifiState::DISCONNECTED;
    unsigned long lastReconnectAttempt = 0;
    unsigned long reconnectDelay = 2000;
    const unsigned long maxReconnectDelay = 30000;
    uint8_t failedAttempts = 0;

    std::unique_ptr<WebServer> server;
    std::unique_ptr<DNSServer> dnsServer;
};

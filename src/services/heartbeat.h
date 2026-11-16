#pragma once

#include <Arduino.h>
#include "config.h"

class HeartbeatService {
public:
    static HeartbeatService& getInstance() {
        static HeartbeatService instance;
        return instance;
    }

    void begin(uint32_t intervalSeconds = DEFAULT_HEARTBEAT_INTERVAL_SEC);
    void update(); // Called in background or network task loop

    void setInterval(uint32_t intervalSeconds);

private:
    HeartbeatService() = default;

    unsigned long lastHeartbeatTime = 0;
    uint32_t intervalMs = DEFAULT_HEARTBEAT_INTERVAL_SEC * 1000;
};

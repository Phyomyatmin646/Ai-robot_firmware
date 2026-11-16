#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "app_events.h"

class EventManager {
public:
    static EventManager& getInstance() {
        static EventManager instance;
        return instance;
    }

    void begin(size_t queueSize = 32);
    bool postEvent(const AppEvent& event, TickType_t waitTicks = 0);
    bool postEventFromISR(const AppEvent& event, BaseType_t* higherPriorityTaskWoken);
    bool receiveEvent(AppEvent* event, TickType_t waitTicks = portMAX_DELAY);

    QueueHandle_t getQueueHandle() const { return eventQueue; }

private:
    EventManager() = default;
    QueueHandle_t eventQueue = nullptr;
};

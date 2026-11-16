#include "services/event_manager.h"

void EventManager::begin(size_t queueSize) {
    if (!eventQueue) {
        eventQueue = xQueueCreate(queueSize, sizeof(AppEvent));
        if (eventQueue == nullptr) {
            Serial.println("[EventManager] ERROR: Failed to allocate FreeRTOS Event Queue!");
        } else {
            Serial.printf("[EventManager] Event Queue initialized (capacity: %u events).\n", queueSize);
        }
    }
}

bool EventManager::postEvent(const AppEvent& event, TickType_t waitTicks) {
    if (!eventQueue) return false;
    return (xQueueSend(eventQueue, &event, waitTicks) == pdTRUE);
}

bool EventManager::postEventFromISR(const AppEvent& event, BaseType_t* higherPriorityTaskWoken) {
    if (!eventQueue) return false;
    return (xQueueSendFromISR(eventQueue, &event, higherPriorityTaskWoken) == pdTRUE);
}

bool EventManager::receiveEvent(AppEvent* event, TickType_t waitTicks) {
    if (!eventQueue || !event) return false;
    return (xQueueReceive(eventQueue, event, waitTicks) == pdTRUE);
}

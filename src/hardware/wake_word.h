#pragma once

#include <Arduino.h>
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "config.h"
#include "app_events.h"
#include "services/event_manager.h"

#ifdef __cplusplus
extern "C" {
#endif
extern const esp_wn_iface_t esp_sr_wakenet9_quantized;
#ifdef __cplusplus
}
#endif

/**
 * ============================================================================
 * Espressif ESP-SR WakeNet9 Wake-Word Engine ("Hi ESP")
 * ============================================================================
 * 
 * Target Pre-Trained Model: "wn9_hiesp" (Wake word: "Hi ESP")
 * 
 * Functional Architecture:
 * - Powered by Espressif's neural network WakeNet9 engine (`libwakenet.a`).
 * - Listens locally on 16 kHz 16-bit Mono PCM audio captured by INMP441.
 * - Zero microphone audio is transmitted to the network/cloud during IDLE.
 * - Pre-roll circular audio buffer preserves initial speech after "Hi ESP".
 * - Only a genuine WakeNet detection (`WAKENET_DETECTED`) triggers voice streaming.
 * ============================================================================
 */

enum class WakeNetStatus {
    UNINITIALIZED,
    MODEL_LOADED_AWAITING_MIC,
    READY_WAKENET9_HIESP,
    MODEL_PARTITION_NOT_FOUND,
    ACTIVE_INFERENCE,
    PAUSED
};

class WakeWordEngine {
public:
    static WakeWordEngine& getInstance() {
        static WakeWordEngine instance;
        return instance;
    }

    bool begin();
    void setListening(bool listening);
    bool isListening() const { return activeListening; }
    bool isModelLoaded() const { return modelLoaded; }

    // Feeds real microphone PCM chunk into WakeNet neural inference engine
    bool processPcmChunk(const int16_t* pcmSamples, size_t sampleCount);

    WakeNetStatus getStatus() const { return status; }
    const char* getActiveWakeWord() const { return activeWakeWord; }
    const char* getModelName() const { return activeModelName; }
    const char* getHardwareStatusStr() const;

    // Pre-roll audio extraction to prevent losing speech following wake word
    size_t getPreRollAudio(int16_t* outBuffer, size_t maxSamples);

private:
    WakeWordEngine() = default;

    WakeNetStatus status = WakeNetStatus::UNINITIALIZED;
    bool modelLoaded = false;
    bool activeListening = false;
    const char* activeWakeWord = "Hi ESP";
    char activeModelName[32] = "wn9_hiesp";

    // ESP-SR Handles
    srmodel_list_t* srModels = nullptr;
    const esp_wn_iface_t* wakenetIface = nullptr;
    model_iface_data_t* modelData = nullptr;
    int chunkSampleSize = 512;

    // Pre-roll Circular Buffer (holds up to 1024 samples = 64ms pre-roll)
    static const size_t PRE_ROLL_SAMPLES = 1024;
    int16_t preRollBuffer[PRE_ROLL_SAMPLES] = {0};
    size_t preRollHead = 0;
    bool hasValidPreRoll = false;

    // Feed buffer to accumulate samples for WakeNet chunksize
    int16_t accumBuffer[1024] = {0};
    size_t accumCount = 0;

    unsigned long lastDetectionTime = 0;
    const unsigned long detectionCooldownMs = 3000;
};

#include "hardware/wake_word.h"
#include "hardware/audio_input.h"

const char* WakeWordEngine::getHardwareStatusStr() const {
    switch (status) {
        case WakeNetStatus::READY_WAKENET9_HIESP:
            return "READY_ACTIVE_LISTENING (\"Hi ESP\")";
        case WakeNetStatus::MODEL_LOADED_AWAITING_MIC:
            return "MODEL_LOADED (Awaiting microphone hardware)";
        case WakeNetStatus::ACTIVE_INFERENCE:
            return "ACTIVE_INFERENCE";
        case WakeNetStatus::PAUSED:
            return "PAUSED";
        case WakeNetStatus::MODEL_PARTITION_NOT_FOUND:
            return "MODEL_PARTITION_NOT_FOUND";
        default:
            return "UNINITIALIZED";
    }
}

bool WakeWordEngine::begin() {
    Serial.println("[WakeWordEngine] Initializing Espressif ESP-SR WakeNet9 Engine...");

    // 1. Mount model partition from flash
    // Partition table defines "model" at offset 0xc10000 (size 0x350000)
    srModels = esp_srmodel_init("model");
    if (!srModels) {
        Serial.println("[WakeWordEngine] NOTICE: Flash 'model' partition not mounted or empty.");
        Serial.println("[WakeWordEngine] Note: Flash 'srmodels.bin' to offset 0xc10000 for flash-mapped WakeNet9.");
    } else {
        char* foundName = esp_srmodel_filter(srModels, ESP_WN_PREFIX, "hiesp");
        if (foundName) {
            strncpy(activeModelName, foundName, sizeof(activeModelName) - 1);
            Serial.printf("[WakeWordEngine] Found WakeNet model in partition: %s\n", activeModelName);
        }
    }

    // 2. Obtain WakeNet interface handle for target model ("wn9_hiesp")
    wakenetIface = esp_wn_handle_from_name(activeModelName);
    if (!wakenetIface) {
        // Fallback to default quantized wakenet9 interface
        wakenetIface = &esp_sr_wakenet9_quantized;
    }

    if (wakenetIface) {
        // Create neural model inference instance with standard sensitivity (DET_MODE_90)
        modelData = wakenetIface->create(activeModelName, DET_MODE_90);
        if (modelData) {
            chunkSampleSize = wakenetIface->get_samp_chunksize(modelData);
            int sampRate = wakenetIface->get_samp_rate(modelData);
            char* wordName = wakenetIface->get_word_name(modelData, 1);
            if (wordName) {
                activeWakeWord = wordName;
            }

            // Set detection threshold (0.65 threshold provides high precision)
            wakenetIface->set_det_threshold(modelData, 0.65f, 1);

            modelLoaded = true;
            bool micDetected = (AudioInput::getInstance().getHardwareStatus() == AudioInputHwStatus::AUDIO_INPUT_VALID);
            if (micDetected) {
                status = WakeNetStatus::READY_WAKENET9_HIESP;
                activeListening = true;
                Serial.printf("[WakeWordEngine] Model: MODEL_LOADED (Model: %s, Word: \"%s\") | Audio: AUDIO_INPUT_VALID -> Detection READY.\n",
                              activeModelName, activeWakeWord);
            } else {
                status = WakeNetStatus::MODEL_LOADED_AWAITING_MIC;
                activeListening = false;
                Serial.printf("[WakeWordEngine] Model: MODEL_LOADED (Model: %s, Word: \"%s\") | Audio: AWAITING_AUDIO_HARDWARE (Microphone not detected; inference idle until mic attached).\n",
                              activeModelName, activeWakeWord);
            }
            return true;
        } else {
            Serial.println("[WakeWordEngine] WARNING: WakeNet model create returned NULL. Flash partition data required.");
            status = WakeNetStatus::MODEL_PARTITION_NOT_FOUND;
        }
    } else {
        Serial.println("[WakeWordEngine] ERROR: Failed to obtain WakeNet interface handle.");
        status = WakeNetStatus::UNINITIALIZED;
    }

    return false;
}

void WakeWordEngine::setListening(bool listening) {
    activeListening = listening;
    accumCount = 0;
    if (listening && wakenetIface && modelData) {
        wakenetIface->clean(modelData);
    }
}

size_t WakeWordEngine::getPreRollAudio(int16_t* outBuffer, size_t maxSamples) {
    if (!outBuffer || maxSamples == 0 || !hasValidPreRoll) return 0;

    size_t samplesToCopy = (maxSamples < PRE_ROLL_SAMPLES) ? maxSamples : PRE_ROLL_SAMPLES;
    for (size_t i = 0; i < samplesToCopy; i++) {
        size_t idx = (preRollHead + i) % PRE_ROLL_SAMPLES;
        outBuffer[i] = preRollBuffer[idx];
    }

    hasValidPreRoll = false;
    return samplesToCopy;
}

bool WakeWordEngine::processPcmChunk(const int16_t* pcmSamples, size_t sampleCount) {
    if (!activeListening || !pcmSamples || sampleCount == 0) return false;

    // 1. Maintain circular pre-roll buffer to preserve user's initial speech following wake word
    for (size_t i = 0; i < sampleCount; i++) {
        preRollBuffer[preRollHead] = pcmSamples[i];
        preRollHead = (preRollHead + 1) % PRE_ROLL_SAMPLES;
    }

    // 2. If WakeNet model is active, feed samples into neural inference engine
    if (wakenetIface && modelData) {
        size_t offset = 0;
        while (offset < sampleCount) {
            size_t needed = (chunkSampleSize > (int)accumCount) ? (chunkSampleSize - accumCount) : 0;
            size_t toCopy = (sampleCount - offset < needed) ? (sampleCount - offset) : needed;

            memcpy(accumBuffer + accumCount, pcmSamples + offset, toCopy * sizeof(int16_t));
            accumCount += toCopy;
            offset += toCopy;

            if (accumCount >= (size_t)chunkSampleSize) {
                // Run genuine WakeNet neural inference
                wakenet_state_t result = wakenetIface->detect(modelData, accumBuffer);
                accumCount = 0;

                unsigned long now = millis();
                if (result == WAKENET_DETECTED && (now - lastDetectionTime > detectionCooldownMs)) {
                    lastDetectionTime = now;
                    hasValidPreRoll = true;
                    activeListening = false;

                    Serial.printf("[WakeWordEngine] Genuine WakeNet9 Detection: \"%s\" detected! Launching Voice Session...\n", activeWakeWord);

                    AppEvent ev{};
                    ev.type = EventType::VOICE_SESSION_START;
                    snprintf(ev.text, sizeof(ev.text), "wake:%s", activeWakeWord);
                    EventManager::getInstance().postEvent(ev);
                    return true;
                }
            }
        }
    }

    return false;
}

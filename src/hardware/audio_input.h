#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include "pins.h"
#include "config.h"

enum class AudioInputHwStatus {
    UNINITIALIZED,
    DRIVER_INITIALIZED,
    AUDIO_INPUT_VALID,
    NOT_DETECTED
};

class AudioInput {
public:
    static AudioInput& getInstance() {
        static AudioInput instance;
        return instance;
    }

    bool begin();
    void startRecording();
    void stopRecording();
    bool isRecording() const { return capturing; }

    AudioInputHwStatus getHardwareStatus() const { return hwStatus; }
    const char* getHardwareStatusStr() const;

    // Read a 4096-byte 16kHz PCM16 chunk from DMA
    size_t readPcmChunk(int16_t* outputBuffer, size_t maxSamples);

private:
    AudioInput() = default;

    AudioInputHwStatus hwStatus = AudioInputHwStatus::UNINITIALIZED;
    bool capturing = false;
    bool isInitialized = false;
    const i2s_port_t i2sPort = I2S_NUM_0;

    // Internal scratch buffer for 32-bit DMA read
    int32_t rawDmaBuffer[AUDIO_CHUNK_SIZE / 2];
};

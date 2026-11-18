#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include "pins.h"
#include "config.h"

class AudioOutput {
public:
    static AudioOutput& getInstance() {
        static AudioOutput instance;
        return instance;
    }

    bool begin();
    void setSampleRate(uint32_t sampleRate);
    void setVolume(uint8_t volume); // 0..100
    uint8_t getVolume() const { return currentVolume; }
    void setMaxVolumeLimit(uint8_t limit) { maxSoftwareVolumeLimit = (limit <= 100) ? limit : 100; }
    uint8_t getMaxVolumeLimit() const { return maxSoftwareVolumeLimit; }

    // Write 16-bit PCM buffer to I2S DMA with software volume scaling
    size_t writePcm(const int16_t* pcmSamples, size_t sampleCount);

    // Instant mute & DMA clear for barge-in / interruption
    void stopAndFlush();

    const char* getHardwareStatusStr() const {
        return isInitialized ? "NOT_VERIFIABLE (Simplex I2S TX, no feedback pin)" : "UNINITIALIZED";
    }

    // Play local cute synthesized head-touch sound effect
    void playCuteChime();

private:
    AudioOutput() = default;

    bool isInitialized = false;
    const i2s_port_t i2sPort = I2S_NUM_1;
    uint32_t currentSampleRate = AUDIO_OUTPUT_SAMPLE_RATE;
    uint8_t currentVolume = DEFAULT_VOLUME;
    uint8_t maxSoftwareVolumeLimit = CONFIG_SOFTWARE_MAX_VOLUME_LIMIT;
    float gainMultiplier = 0.8f;

    int16_t scaleBuffer[AUDIO_CHUNK_SIZE / 2];
};

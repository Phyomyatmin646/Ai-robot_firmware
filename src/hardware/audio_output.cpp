#include "hardware/audio_output.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool AudioOutput::begin() {
    Serial.println("[AudioOutput] Initializing MAX98357A Amplifier (I2S_NUM_1)...");

    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = currentSampleRate, // Default 24000 Hz
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = AUDIO_DMA_BUF_COUNT,
        .dma_buf_len = AUDIO_DMA_BUF_LEN,
        .use_apll = true, // Use Audio PLL for precise 24kHz / 16kHz clock
        .tx_desc_auto_clear = true, // Auto clear underflow DMA descriptors
        .fixed_mclk = 0
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num = PIN_I2S_SPK_BCLK,
        .ws_io_num = PIN_I2S_SPK_LRC,
        .data_out_num = PIN_I2S_SPK_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(i2sPort, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[AudioOutput] ERROR: i2s_driver_install failed (0x%x)\n", err);
        return false;
    }

    err = i2s_set_pin(i2sPort, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("[AudioOutput] ERROR: i2s_set_pin failed (0x%x)\n", err);
        return false;
    }

    i2s_zero_dma_buffer(i2sPort);
    setVolume(currentVolume);

    isInitialized = true;
    Serial.printf("[AudioOutput] Driver: DRIVER_INITIALIZED (I2S_NUM_1 TX configured at %u Hz, Gain: %.2f).\n",
                  currentSampleRate, gainMultiplier);
    Serial.println("[AudioOutput] Hardware: NOT_VERIFIABLE (MAX98357A is simplex I2S TX only; no digital feedback/telemetry pin).");
    return true;
}

void AudioOutput::setSampleRate(uint32_t sampleRate) {
    if (!isInitialized || sampleRate == currentSampleRate || sampleRate == 0) return;
    
    currentSampleRate = sampleRate;
    i2s_set_sample_rates(i2sPort, currentSampleRate);
    Serial.printf("[AudioOutput] Dynamic I2S sample rate set to: %u Hz\n", currentSampleRate);
}

void AudioOutput::setVolume(uint8_t volume) {
    if (volume > maxSoftwareVolumeLimit) volume = maxSoftwareVolumeLimit;
    currentVolume = volume;
    // Linear-to-perceptual scaling curve with clipping headroom
    gainMultiplier = ((float)volume / 100.0f) * 0.95f;
}

size_t AudioOutput::writePcm(const int16_t* pcmSamples, size_t sampleCount) {
    if (!isInitialized || !pcmSamples || sampleCount == 0) return 0;

    size_t processed = 0;
    while (processed < sampleCount) {
        size_t batch = (sampleCount - processed < (AUDIO_CHUNK_SIZE / 2)) 
                     ? (sampleCount - processed) 
                     : (AUDIO_CHUNK_SIZE / 2);

        // Apply digital scaling with soft saturation clipping protection
        for (size_t i = 0; i < batch; i++) {
            int32_t val = (int32_t)(pcmSamples[processed + i] * gainMultiplier);
            // Soft clamp to prevent wrapping distortion
            if (val > 30000) val = 30000;
            if (val < -30000) val = -30000;
            scaleBuffer[i] = (int16_t)val;
        }

        size_t bytesWritten = 0;
        esp_err_t err = i2s_write(i2sPort, scaleBuffer, batch * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            break;
        }
        processed += (bytesWritten / sizeof(int16_t));
    }

    return processed;
}

void AudioOutput::stopAndFlush() {
    if (!isInitialized) return;
    i2s_zero_dma_buffer(i2sPort);
    Serial.println("[AudioOutput] DMA buffers flushed (Barge-in / Stop).");
}

void AudioOutput::playCuteChime() {
    if (!isInitialized) return;

    // Synthesize a cute, cheerful two-stage rising robot chirp (180ms total)
    // Stage 1: 880 Hz (A5), Stage 2: 1320 Hz (E6)
    const uint32_t sampleRate = currentSampleRate;
    const size_t totalSamples = (sampleRate * 180) / 1000; // 180 ms
    int16_t buffer[128];

    float f1 = 880.0f;
    float f2 = 1320.0f;
    float phase = 0.0f;

    for (size_t i = 0; i < totalSamples; ) {
        size_t batch = (totalSamples - i < 128) ? (totalSamples - i) : 128;
        for (size_t b = 0; b < batch; b++) {
            size_t sampleIdx = i + b;
            float progress = (float)sampleIdx / (float)totalSamples;
            float freq = (progress < 0.45f) ? f1 : f2;

            // Envelope: quick attack, smooth exponential decay
            float env = 1.0f;
            if (progress < 0.05f) {
                env = progress / 0.05f;
            } else {
                float decayProg = (progress - 0.05f) / 0.95f;
                env = (1.0f - decayProg) * (1.0f - decayProg);
            }

            phase += 2.0f * (float)M_PI * freq / (float)sampleRate;
            if (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;

            // Harmonic warmth with 2nd harmonic
            float val = (sinf(phase) + 0.25f * sinf(2.0f * phase)) * env * 16000.0f * (currentVolume / 100.0f);
            buffer[b] = (int16_t)val;
        }

        size_t bytesWritten = 0;
        i2s_write(i2sPort, buffer, batch * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(50));
        i += batch;
    }
    Serial.println("[AudioOutput] Played local cute head-touch chime.");
}

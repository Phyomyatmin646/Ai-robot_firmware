#include "hardware/audio_input.h"

bool AudioInput::begin() {
    Serial.println("[AudioInput] Initializing INMP441 Microphone (I2S_NUM_0)...");

    i2s_config_t i2sConfig = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_INPUT_SAMPLE_RATE, // 16000 Hz
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 outputs 24-bit in 32-bit slot
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = AUDIO_DMA_BUF_COUNT,
        .dma_buf_len = AUDIO_DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pinConfig = {
        .bck_io_num = PIN_I2S_MIC_SCK,
        .ws_io_num = PIN_I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_MIC_SD
    };

    esp_err_t err = i2s_driver_install(i2sPort, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[AudioInput] ERROR: i2s_driver_install failed (0x%x)\n", err);
        return false;
    }

    err = i2s_set_pin(i2sPort, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("[AudioInput] ERROR: i2s_set_pin failed (0x%x)\n", err);
        return false;
    }

    i2s_zero_dma_buffer(i2sPort);
    isInitialized = true;
    Serial.println("[AudioInput] INMP441 I2S initialized (16 kHz, Mono, 16-bit PCM).");
    return true;
}

void AudioInput::startRecording() {
    if (!isInitialized) return;
    i2s_zero_dma_buffer(i2sPort);
    i2s_start(i2sPort);
    capturing = true;
    Serial.println("[AudioInput] Started recording.");
}

void AudioInput::stopRecording() {
    if (!isInitialized) return;
    capturing = false;
    i2s_stop(i2sPort);
    i2s_zero_dma_buffer(i2sPort);
    Serial.println("[AudioInput] Stopped recording.");
}

size_t AudioInput::readPcmChunk(int16_t* outputBuffer, size_t maxSamples) {
    if (!isInitialized || !capturing || !outputBuffer) return 0;

    size_t bytesToRead = maxSamples * sizeof(int32_t);
    size_t bytesRead = 0;

    // Timeout set to 80ms (slightly above 64ms chunk time) to avoid blocking
    esp_err_t err = i2s_read(i2sPort, rawDmaBuffer, bytesToRead, &bytesRead, pdMS_TO_TICKS(80));
    if (err != ESP_OK || bytesRead == 0) {
        return 0;
    }

    size_t samplesRead = bytesRead / sizeof(int32_t);
    // Convert 32-bit slot (24-bit INMP441 data) to signed 16-bit PCM
    for (size_t i = 0; i < samplesRead; i++) {
        // Shift 14 bits for balanced volume gain without distortion
        int32_t sample = rawDmaBuffer[i] >> 14;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        outputBuffer[i] = (int16_t)sample;
    }

    return samplesRead;
}

#include "hardware/audio_input.h"

const char* AudioInput::getHardwareStatusStr() const {
    switch (hwStatus) {
        case AudioInputHwStatus::AUDIO_INPUT_VALID:
            return "AUDIO_INPUT_VALID (MEMS audio signal active)";
        case AudioInputHwStatus::NOT_DETECTED:
            return "NOT_DETECTED (Audio line silent/disconnected, all zeros)";
        case AudioInputHwStatus::DRIVER_INITIALIZED:
            return "DRIVER_INITIALIZED (Awaiting audio validation)";
        default:
            return "UNINITIALIZED";
    }
}

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
        hwStatus = AudioInputHwStatus::UNINITIALIZED;
        return false;
    }

    err = i2s_set_pin(i2sPort, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("[AudioInput] ERROR: i2s_set_pin failed (0x%x)\n", err);
        hwStatus = AudioInputHwStatus::UNINITIALIZED;
        return false;
    }

    i2s_zero_dma_buffer(i2sPort);
    isInitialized = true;
    hwStatus = AudioInputHwStatus::DRIVER_INITIALIZED;
    Serial.println("[AudioInput] Driver: DRIVER_INITIALIZED (I2S_NUM_0 peripheral configured).");

    // Perform real signal-level diagnostic read (256 samples = 16ms of audio)
    i2s_start(i2sPort);
    int32_t testBuf[256];
    size_t bytesRead = 0;
    esp_err_t readErr = i2s_read(i2sPort, testBuf, sizeof(testBuf), &bytesRead, pdMS_TO_TICKS(50));
    i2s_stop(i2sPort);
    i2s_zero_dma_buffer(i2sPort);

    size_t testSamples = (readErr == ESP_OK) ? (bytesRead / sizeof(int32_t)) : 0;
    bool allZero = true;
    int32_t minVal = INT32_MAX;
    int32_t maxVal = INT32_MIN;

    for (size_t i = 0; i < testSamples; i++) {
        int32_t val = testBuf[i];
        if (val != 0) allZero = false;
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
    }

    int32_t dynamicRange = (testSamples > 0 && !allZero) ? (maxVal - minVal) : 0;

    // Disconnected / idle I2S line reads 0x00000000 or flatline.
    // A real INMP441 MEMS microphone always outputs room ambient noise (dynamicRange > 500 in 24-bit).
    if (testSamples == 0 || allZero || dynamicRange < 500) {
        hwStatus = AudioInputHwStatus::NOT_DETECTED;
        Serial.printf("[AudioInput] Hardware: NOT_DETECTED (Audio line silent/disconnected, %u samples read, range: %ld).\n",
                      (unsigned int)testSamples, (long)dynamicRange);
    } else {
        hwStatus = AudioInputHwStatus::AUDIO_INPUT_VALID;
        Serial.printf("[AudioInput] Hardware: AUDIO_INPUT_VALID (Real MEMS microphone signal detected, dynamic range: %ld).\n",
                      (long)dynamicRange);
    }

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

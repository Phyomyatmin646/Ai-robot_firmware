#pragma once

#include <Arduino.h>

/**
 * ============================================================================
 * SYSTEM CONFIGURATION CONSTANTS
 * ============================================================================
 */

// Production Backend Connection
#define API_BASE_URL                "https://107.23.154.150.sslip.io"
#define API_PREFIX                  "https://107.23.154.150.sslip.io/api"
#define WS_URL                      "wss://107.23.154.150.sslip.io/ws/esp32"
#define HEALTH_CHECK_URL            "https://107.23.154.150.sslip.io/health"

#define DEFAULT_WS_HOST             "107.23.154.150.sslip.io"
#define DEFAULT_WS_PORT             443
#define DEFAULT_WS_PATH             "/ws/esp32"
#define DEFAULT_API_BASE_URL        API_BASE_URL

// Audio Buffer & Stream Parameters
#define AUDIO_INPUT_SAMPLE_RATE     16000   // 16 kHz for INMP441 capture
#define AUDIO_OUTPUT_SAMPLE_RATE    24000   // 24 kHz for Gemini Live audio playback
#define AUDIO_BITS_PER_SAMPLE       16      // 16-bit linear PCM
#define AUDIO_CHANNELS              1       // Mono
#define AUDIO_CHUNK_SIZE            2048    // 2048 bytes per audio chunk (64ms @ 16kHz) - Low-Latency
#define AUDIO_DMA_BUF_COUNT         8       // 8 DMA buffers
#define AUDIO_DMA_BUF_LEN           512     // 512 samples per buffer (1024 bytes)

// Ring Buffer in PSRAM for smooth audio playback
#define AUDIO_PLAYBACK_RB_SIZE      (192 * 1024) // 192 KB (~4 seconds @ 24kHz 16-bit)

// Display Configuration (Landscape 320x240 for Robot Face)
#define TFT_WIDTH                   320
#define TFT_HEIGHT                  240
#define TFT_ROTATION                1       // 1: Landscape (or 3 for 180° inverted)
#define DEFAULT_BRIGHTNESS          80      // 0 - 100%

// Volume & Limiter Configuration
#define DEFAULT_VOLUME                      80      // 0 - 100%
#define VOLUME_STEP                         5       // 5% per button press
// UNCALIBRATED Software Volume Ceiling (Default 100% - Unclamped in software)
// NOTE: Bench power measurement (oscilloscope/power meter) with real 8Ω 2W speaker
// is required before setting an electrical hardware protection limit.
#define CONFIG_SOFTWARE_MAX_VOLUME_LIMIT    100     // 0 - 100%
#define NVS_KEY_MAX_VOL_LIMIT               "max_vol"

// Telemetry & Network Timing
#define DEFAULT_HEARTBEAT_INTERVAL_SEC  15  // 15 seconds
#define WS_RECONNECT_MIN_DELAY_MS       1000 // 1 second
#define WS_RECONNECT_MAX_DELAY_MS       30000 // 30 seconds
#define WIFI_CONNECT_TIMEOUT_MS         20000 // 20 seconds

// NVS Preference Namespaces & Keys
#define NVS_NAMESPACE_NET           "robot_net"
#define NVS_KEY_SSID                "wifi_ssid"
#define NVS_KEY_PASS                "wifi_pass"

#define NVS_NAMESPACE_AUTH          "robot_auth"
#define NVS_KEY_DEVICE_ID           "device_id"
#define NVS_KEY_API_KEY             "api_key"

#define NVS_NAMESPACE_CONFIG        "robot_cfg"
#define NVS_KEY_VOLUME              "vol"
#define NVS_KEY_BRIGHTNESS          "bright"

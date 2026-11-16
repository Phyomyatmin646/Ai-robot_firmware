#pragma once

/**
 * ============================================================================
 * SECRETS & CREDENTIALS TEMPLATE
 * ============================================================================
 * 
 * Instructions:
 * 1. Do NOT put real credentials in this file.
 * 2. In production, credentials should be provisioned into NVS Preferences
 *    via USB Serial Provisioning Mode or the Captive Portal.
 * 3. For local bench testing only, copy this file to `include/secrets.h`
 *    (which is git-ignored) and insert your credentials.
 */

// Default Wi-Fi Credentials (Fallback if NVS empty)
#define DEFAULT_WIFI_SSID       "YOUR_WIFI_SSID"
#define DEFAULT_WIFI_PASS       "YOUR_WIFI_PASSWORD"

// Production Backend Connection
#define API_BASE_URL            "https://107.23.154.150.sslip.io"
#define WS_URL                  "wss://107.23.154.150.sslip.io/ws/esp32"
#define DEFAULT_WS_HOST         "107.23.154.150.sslip.io"
#define DEFAULT_WS_PORT         443
#define DEFAULT_WS_PATH         "/ws/esp32"
#define DEFAULT_API_BASE_URL    API_BASE_URL

// Device Credentials (Obtained from Dashboard POST /api/v1/devices/register)
#define DEFAULT_DEVICE_ID       ""
#define DEFAULT_DEVICE_API_KEY  ""

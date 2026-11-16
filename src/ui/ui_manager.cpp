#include "ui/ui_manager.h"
#include "services/device_manager.h"
#include "services/config_manager.h"
#include "hardware/rfid.h"
#include "network/wifi_manager.h"
#include "network/websocket_client.h"
#include "firmware_version.h"

bool UIManager::begin() {
    Serial.println("[UIManager] Initializing UI Graphics Engine...");
    if (!DisplayController::getInstance().begin()) {
        Serial.println("[UIManager] ERROR: DisplayController initialization failed!");
        return false;
    }

    // Allocate full-screen 240x320 16-bit single-buffer off-screen sprite (153.6 KB) in 8MB Octal PSRAM
    canvas.setColorDepth(16);
    canvas.setPsram(true);
    if (!canvas.createSprite(TFT_WIDTH, TFT_HEIGHT)) {
        Serial.println("[UIManager] WARNING: Could not allocate PSRAM sprite, falling back to direct drawing.");
    } else {
        Serial.println("[UIManager] Allocated 153.6 KB frame sprite in Octal PSRAM.");
    }

    return true;
}

void UIManager::toggleScreen() {
    activeScreen = (activeScreen == CurrentScreen::ROBOT_FACE) ? CurrentScreen::SETTINGS_VIEW : CurrentScreen::ROBOT_FACE;
}

void UIManager::drawStatusBar() {
    // Top status bar with translucent dark cyber-blue theme
    canvas.fillRect(0, 0, TFT_WIDTH, 20, canvas.color565(8, 20, 42));
    canvas.drawFastHLine(0, 20, TFT_WIDTH, canvas.color565(25, 55, 95));

    // 1. Wi-Fi Status Icon / Text (Left side)
    if (WiFiManager::getInstance().isConnected()) {
        canvas.setTextColor(canvas.color565(0, 230, 118));
        canvas.setTextSize(1);
        int8_t rssi = WiFiManager::getInstance().getRSSI();
        canvas.setCursor(8, 6);
        canvas.printf("WiFi:%d dBm", rssi);
    } else {
        canvas.setTextColor(canvas.color565(255, 82, 82));
        canvas.setTextSize(1);
        canvas.setCursor(8, 6);
        canvas.print("WiFi:OFF");
    }

    // 2. Cloud WebSocket Connection Status (Center)
    if (WebSocketClientManager::getInstance().isAuthenticated()) {
        canvas.fillCircle(145, 10, 4, canvas.color565(0, 230, 118));
        canvas.setTextColor(canvas.color565(210, 235, 255));
        canvas.setCursor(155, 6);
        canvas.print("LIVE");
    } else {
        canvas.fillCircle(145, 10, 4, canvas.color565(255, 171, 0));
        canvas.setTextColor(canvas.color565(160, 170, 185));
        canvas.setCursor(155, 6);
        canvas.print("OFF");
    }

    // 3. RFID Active Tag Indicator
    if (RFIDController::getInstance().getLastUid().length() > 0) {
        canvas.fillRoundRect(210, 3, 34, 14, 3, canvas.color565(0, 188, 212));
        canvas.setTextColor(TFT_BLACK);
        canvas.setCursor(214, 6);
        canvas.print("RFID");
    }

    // 4. Volume Indicator (Right side)
    uint8_t vol = ConfigManager::getInstance().getVolume();
    canvas.setTextColor(canvas.color565(220, 235, 250));
    canvas.setCursor(260, 6);
    canvas.printf("Vol:%d%%", vol);
}

void UIManager::drawIdleFace(int eyeBlink) {
    // Rich deep cyber-blue background (matching photo)
    const uint16_t COLOR_BG = canvas.color565(14, 40, 78);
    canvas.fillRect(0, 21, TFT_WIDTH, TFT_HEIGHT - 21, COLOR_BG);

    // Landscape 320x240 eye geometry: wide horizontal pills / rounded rectangles
    int eyeW = 76;
    int eyeH = 50;
    int eyeR = 24; // Smooth capsule pill radius
    int leftX = 58;
    int rightX = 186;
    int eyeY = 75;

    uint16_t eyeColor = TFT_WHITE;
    uint16_t glowBorder = canvas.color565(80, 190, 255);

    if (eyeBlink > 0) {
        // Natural blinking phase (horizontal closed slit eyes)
        canvas.fillRoundRect(leftX, eyeY + 21, eyeW, 8, 4, eyeColor);
        canvas.fillRoundRect(rightX, eyeY + 21, eyeW, 8, 4, eyeColor);
    } else {
        // Subtle outer glowing contour
        canvas.drawRoundRect(leftX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, glowBorder);
        canvas.drawRoundRect(rightX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, glowBorder);

        // Solid white horizontal pill eyes
        canvas.fillRoundRect(leftX, eyeY, eyeW, eyeH, eyeR, eyeColor);
        canvas.fillRoundRect(rightX, eyeY, eyeW, eyeH, eyeR, eyeColor);

        // Glossy eye shine reflection highlights (top-left)
        canvas.fillCircle(leftX + 20, eyeY + 16, 5, canvas.color565(230, 245, 255));
        canvas.fillCircle(rightX + 20, eyeY + 16, 5, canvas.color565(230, 245, 255));
    }

    // Idle friendly subtle smile mouth
    canvas.fillRoundRect(160 - 20, 172, 40, 8, 4, canvas.color565(200, 240, 255));
}

void UIManager::drawListeningFace(unsigned long timeMs) {
    const uint16_t COLOR_BG = canvas.color565(10, 36, 68);
    canvas.fillRect(0, 21, TFT_WIDTH, TFT_HEIGHT - 21, COLOR_BG);

    uint16_t green = canvas.color565(0, 230, 118);
    int eyeW = 78;
    int eyeH = 54;
    int eyeR = 25;
    int leftX = 57;
    int rightX = 185;
    int eyeY = 73;

    // Attentive wide bright eyes with green-tinted outer glow
    canvas.drawRoundRect(leftX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, green);
    canvas.drawRoundRect(rightX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, green);
    canvas.fillRoundRect(leftX, eyeY, eyeW, eyeH, eyeR, TFT_WHITE);
    canvas.fillRoundRect(rightX, eyeY, eyeW, eyeH, eyeR, TFT_WHITE);

    // Microphone sound wave pulse animation mouth
    int pulseW = 36 + (int)(sin(timeMs / 110.0) * 18);
    if (pulseW < 20) pulseW = 20;
    canvas.fillRoundRect(160 - (pulseW / 2), 170, pulseW, 10, 5, green);

    canvas.setTextColor(green);
    canvas.setTextSize(1);
    canvas.drawCenterString("Listening...", 160, 210);
}

void UIManager::drawThinkingFace(unsigned long timeMs) {
    const uint16_t COLOR_BG = canvas.color565(12, 35, 65);
    canvas.fillRect(0, 21, TFT_WIDTH, TFT_HEIGHT - 21, COLOR_BG);

    uint16_t yellow = canvas.color565(255, 215, 64);
    int eyeW = 74;
    int eyeH = 46;
    int eyeR = 22;
    int leftX = 59;
    int rightX = 187;
    int eyeY = 66; // Eyes looking slightly upward

    // Inquisitive eyes
    canvas.drawRoundRect(leftX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, yellow);
    canvas.drawRoundRect(rightX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, yellow);
    canvas.fillRoundRect(leftX, eyeY, eyeW, eyeH, eyeR, TFT_WHITE);
    canvas.fillRoundRect(rightX, eyeY, eyeW, eyeH, eyeR, TFT_WHITE);

    // 3 cute bouncing thinking dots in place of mouth
    for (int i = 0; i < 3; i++) {
        int bounce = (int)(abs(sin((timeMs + (i * 150)) / 180.0)) * 8);
        canvas.fillCircle(140 + (i * 20), 172 - bounce, 5, yellow);
    }

    canvas.setTextColor(yellow);
    canvas.setTextSize(1);
    canvas.drawCenterString("Thinking...", 160, 210);
}

void UIManager::drawSpeakingFace(unsigned long timeMs) {
    const uint16_t COLOR_BG = canvas.color565(14, 42, 82);
    canvas.fillRect(0, 21, TFT_WIDTH, TFT_HEIGHT - 21, COLOR_BG);

    uint16_t cyan = canvas.color565(0, 229, 255);
    int eyeW = 76;
    int eyeH = 50;
    int eyeR = 24;
    int leftX = 58;
    int rightX = 186;
    int eyeY = 74;

    // Cheerful expressive eyes with cyan aura
    canvas.drawRoundRect(leftX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, cyan);
    canvas.drawRoundRect(rightX - 2, eyeY - 2, eyeW + 4, eyeH + 4, eyeR + 2, cyan);
    canvas.fillRoundRect(leftX, eyeY, eyeW, eyeH, eyeR, TFT_WHITE);
    canvas.fillRoundRect(rightX, eyeY, eyeW, eyeH, eyeR, TFT_WHITE);

    // Eye shine reflections
    canvas.fillCircle(leftX + 20, eyeY + 16, 5, canvas.color565(210, 245, 255));
    canvas.fillCircle(rightX + 20, eyeY + 16, 5, canvas.color565(210, 245, 255));

    // =========================================================================
    // Dynamic Talking Mouth Animation (ပါးစပ်လှုပ်ရှားမှု)
    // Simulates speech phoneme cadence using harmonic sine waves
    // =========================================================================
    float s1 = sin(timeMs / 85.0);
    float s2 = sin(timeMs / 135.0);
    float c1 = cos(timeMs / 195.0);

    // Mouth height fluctuates dynamically between 8px (closed) and 36px (wide open)
    int mouthH = 8 + (int)(abs(s1 * 0.65f + s2 * 0.35f) * 28.0f);
    // Mouth width varies subtly between 40px and 56px
    int mouthW = 40 + (int)(abs(c1) * 16.0f);
    int mouthX = 160 - (mouthW / 2);
    int mouthY = 172 - (mouthH / 2);
    int mouthR = mouthH / 2;
    if (mouthR > 14) mouthR = 14;

    // 1. Outer glowing lip contour (Bright Cyan)
    canvas.fillRoundRect(mouthX, mouthY, mouthW, mouthH, mouthR, cyan);

    // 2. Inner mouth cavity when open (> 14px) for 3D animated mouth depth
    if (mouthH > 14) {
        int inW = mouthW - 6;
        int inH = mouthH - 6;
        int inR = inH / 2;
        if (inR > 10) inR = 10;
        canvas.fillRoundRect(160 - (inW / 2), 172 - (inH / 2), inW, inH, inR, canvas.color565(8, 22, 48));

        // 3. Cute tongue highlight when mouth opens wide (> 22px)
        if (mouthH > 22) {
            canvas.fillCircle(160, 172 + (inH / 4), 5, canvas.color565(255, 110, 140));
        }
    }

    canvas.setTextColor(cyan);
    canvas.setTextSize(1);
    canvas.drawCenterString("Speaking...", 160, 210);
}

void UIManager::drawErrorFace() {
    const uint16_t COLOR_BG = canvas.color565(35, 12, 16);
    canvas.fillRect(0, 21, TFT_WIDTH, TFT_HEIGHT - 21, COLOR_BG);

    uint16_t red = canvas.color565(255, 82, 82);
    int eyeW = 72;
    int eyeH = 46;
    int eyeR = 20;
    int leftX = 60;
    int rightX = 188;

    // Slanted worried eyes
    canvas.fillRoundRect(leftX, 76, eyeW, eyeH, eyeR, red);
    canvas.fillRoundRect(rightX, 76, eyeW, eyeH, eyeR, red);

    // Worried curved mouth
    canvas.fillRoundRect(160 - 22, 172, 44, 8, 4, red);

    canvas.setTextColor(red);
    canvas.setTextSize(1);
    canvas.drawCenterString("Offline / Error", 160, 204);
    canvas.setTextColor(canvas.color565(180, 180, 180));
    canvas.drawCenterString("Long-press Touch for Setup AP", 160, 218);
}

void UIManager::drawRobotFace() {
    unsigned long now = millis();

    // Eye blink timing (blinks every ~3.5 seconds)
    if (now - lastBlinkTime > 3500) {
        lastBlinkTime = now;
        blinkPhase = 1;
    }
    int eyeBlink = 0;
    if (now - lastBlinkTime < 160) {
        eyeBlink = 1;
    }

    RobotState st = DeviceManager::getInstance().getState();
    switch (st) {
        case RobotState::IDLE:
            drawIdleFace(eyeBlink);
            break;
        case RobotState::LISTENING:
            drawListeningFace(now);
            break;
        case RobotState::THINKING:
            drawThinkingFace(now);
            break;
        case RobotState::SPEAKING:
            drawSpeakingFace(now);
            break;
        case RobotState::ERROR:
            drawErrorFace();
            break;
    }
}

void UIManager::drawSettingsScreen() {
    canvas.fillRect(0, 21, TFT_WIDTH, TFT_HEIGHT - 21, canvas.color565(12, 18, 28));

    canvas.setTextColor(canvas.color565(0, 229, 255));
    canvas.setTextSize(1);
    canvas.setCursor(16, 28);
    canvas.print("=== SYSTEM TELEMETRY (LANDSCAPE) ===");

    canvas.setTextColor(TFT_WHITE);
    int lineH = 15;

    // Column 1: System & Network
    int y = 46;
    canvas.setCursor(16, y);
    canvas.printf("FW: %s | HW: %s", FIRMWARE_VERSION, HARDWARE_VERSION);

    String devId = ConfigManager::getInstance().getDeviceId();
    canvas.setCursor(16, y += lineH);
    canvas.printf("ID: %s", devId.length() > 0 ? devId.substring(0, 14).c_str() : "UNPROVISIONED");

    canvas.setCursor(16, y += lineH);
    canvas.printf("IP: %s", WiFiManager::getInstance().getIpAddress().c_str());

    canvas.setCursor(16, y += lineH);
    canvas.printf("RSSI: %d dBm | WSS: %s", WiFiManager::getInstance().getRSSI(),
                  WebSocketClientManager::getInstance().isAuthenticated() ? "ONLINE" : "OFFLINE");

    // Column 2: Memory & Sliders
    uint32_t freeHeap = (uint32_t)(ESP.getFreeHeap() / 1024);
    uint32_t freePsram = (uint32_t)(ESP.getFreePsram() / 1024);

    canvas.setCursor(16, y += lineH);
    canvas.printf("SRAM: %lu KB | PSRAM: %lu KB", (unsigned long)freeHeap, (unsigned long)freePsram);

    uint8_t vol = ConfigManager::getInstance().getVolume();
    uint8_t bright = ConfigManager::getInstance().getBrightness();

    y += 10;
    canvas.setCursor(16, y += lineH);
    canvas.printf("Volume: %u%%", vol);
    canvas.drawRect(100, y - 2, 200, 8, canvas.color565(70, 70, 70));
    canvas.fillRect(102, y, (vol * 196) / 100, 4, canvas.color565(0, 230, 118));

    canvas.setCursor(16, y += lineH);
    canvas.printf("Bright: %u%%", bright);
    canvas.drawRect(100, y - 2, 200, 8, canvas.color565(70, 70, 70));
    canvas.fillRect(102, y, (bright * 196) / 100, 4, canvas.color565(0, 229, 255));

    canvas.setTextColor(canvas.color565(150, 160, 175));
    canvas.setCursor(16, 215);
    canvas.print("Serial CLI 'SCREEN:TOGGLE' to return to Robot Face");
}

void UIManager::update() {
    unsigned long now = millis();
    // Prioritize audio recording latency: throttle UI to ~15 FPS during LISTENING, ~30 FPS otherwise
    uint32_t minFrameInterval = (DeviceManager::getInstance().getState() == RobotState::LISTENING) ? 66 : 33;
    if (now - lastFrameTime < minFrameInterval) {
        return;
    }
    lastFrameTime = now;

    canvas.fillScreen(TFT_BLACK);
    drawStatusBar();

    if (activeScreen == CurrentScreen::ROBOT_FACE) {
        drawRobotFace();
    } else {
        drawSettingsScreen();
    }

    // Push frame to ILI9341 over SPI DMA
    canvas.pushSprite(0, 0);
}

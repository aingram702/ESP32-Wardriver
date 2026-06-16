// ============================================================================
//  ESP32-S3 Wardriver  -  main orchestration & mode manager.
//
//  WARDRIVE mode : WiFi scanning, GPS tagging, de-dup logging, web UI.
//  ATTACK mode   : 802.11 promiscuous packet monitor + targeted deauth.
//
//  The two modes are mutually exclusive because monitor/promiscuous mode and
//  active dual-radio scanning contend for the radio. Switching is centralised
//  here so the controller is always reconfigured safely.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>

#include "config.h"
#include "Modes.h"
#include "NetworkStore.h"
#include "GpsModule.h"
#include "WifiScanner.h"
#include "PacketSniffer.h"
#include "WebInterface.h"

static WardriverMode s_mode    = MODE_WARDRIVE;
static WardriverMode s_pending  = MODE_WARDRIVE;
static bool          s_fsReady = false;

// --- Mode contract (declared in Modes.h) ------------------------------------
WardriverMode currentMode()     { return s_mode; }
const char*   currentModeName() { return s_mode == MODE_ATTACK ? "attack" : "wardrive"; }
void          requestMode(WardriverMode m) { s_pending = m; }

// --- Status LED (DevKitC-1 addressable RGB on GPIO48) -----------------------
static void setLed(uint8_t r, uint8_t g, uint8_t b) {
#if STATUS_LED_PIN >= 0
    neopixelWrite(STATUS_LED_PIN, r, g, b);
#endif
}

// --- Mode transitions -------------------------------------------------------
static void enterWardrive() {
    g_sniffer.stop();
    setLed(0, 40, 0);              // green
    Serial.println("[MODE] -> WARDRIVE");
}

static void enterAttack() {
    // Sniffer is started on demand from the UI; deauth requires it active.
    setLed(40, 0, 0);              // red
    Serial.println("[MODE] -> ATTACK");
}

static void applyPendingMode() {
    if (s_pending == s_mode) return;
    s_mode = s_pending;
    if (s_mode == MODE_ATTACK) enterAttack(); else enterWardrive();
}

// --- Persistence ------------------------------------------------------------
static uint32_t s_lastSave = 0;
static void maybeAutosave() {
#if AUTOSAVE_ENABLED
    if (!s_fsReady) return;
    if (millis() - s_lastSave < AUTOSAVE_INTERVAL_MS) return;
    s_lastSave = millis();
    g_store.save(LittleFS, CAPTURE_LOG_PATH);
#endif
}

// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== ESP32-S3 WARDRIVER booting ===");
    setLed(0, 0, 40);              // blue during boot

    if (psramFound())
        Serial.printf("[SYS] PSRAM: %u bytes\n", ESP.getPsramSize());
    Serial.printf("[SYS] Flash: %u bytes\n", ESP.getFlashChipSize());

    // Filesystem (capture backup that survives reboots).
    s_fsReady = LittleFS.begin(true);
    Serial.printf("[FS] LittleFS %s\n", s_fsReady ? "mounted" : "FAILED");

    g_store.begin();
    if (s_fsReady && g_store.load(LittleFS, CAPTURE_LOG_PATH))
        Serial.printf("[FS] restored %u devices from backup\n", g_store.count());

    if (GPS_ENABLED) g_gps.begin();

    // Radio: scanner sets WIFI_AP_STA, web layer brings up the SoftAP.
    g_wifiScanner.begin();
    g_web.begin();
    g_sniffer.begin();

    enterWardrive();
    Serial.println("=== ready ===");
}

void loop() {
    applyPendingMode();

    g_web.loop();                  // service the captive-portal DNS
    g_gps.poll();

    if (s_mode == MODE_WARDRIVE) {
        g_wifiScanner.poll();
    }
    // ATTACK mode: sniffer/deauth run via the async web handlers; nothing to
    // pump here. The web server runs in its own task either way.

    maybeAutosave();
    delay(2);                      // yield to WiFi/Async tasks
}

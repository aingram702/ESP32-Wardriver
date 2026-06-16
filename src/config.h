// ============================================================================
//  config.h  -  Central configuration for the ESP32-S3 Wardriver.
//  Tweak the values here to match your hardware and preferences.
// ============================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Access Point (the WiFi network the device hosts for its web UI)
// ---------------------------------------------------------------------------
// SECURITY: the AP is WPA2-protected by default. CHANGE THIS PASSWORD before
// you deploy the device. It must be 8-63 characters. Leaving it as the default
// means anyone nearby can connect and control the radio.
#define AP_SSID            "Wardriver"
#define AP_PASSWORD        "wardrive-me"     // >= 8 chars; CHANGE ME
#define AP_CHANNEL         1                 // 1-13; also the default sniff channel
#define AP_MAX_CLIENTS     2
#define AP_HIDDEN          false

// Optional HTTP basic-auth on the web UI (defense in depth on top of WPA2).
// Set WEB_AUTH_ENABLED to true and pick a strong password to require login.
#define WEB_AUTH_ENABLED   false
#define WEB_AUTH_USER      "admin"
#define WEB_AUTH_PASS      "change-me"

// ---------------------------------------------------------------------------
//  GPS  (ATGM336H / NEO-M8N / NEO-6M class module, NMEA over UART)
//  The module is optional and may be added later - the firmware boots and
//  runs fine without it, reporting "no fix".
// ---------------------------------------------------------------------------
#define GPS_ENABLED        true     // set false to skip GPS init entirely
#define GPS_UART_NUM       1        // use Serial1
#define GPS_BAUD           9600     // ATGM336H default
#define GPS_RX_PIN         18       // ESP32 RX  <- GPS TX
#define GPS_TX_PIN         17       // ESP32 TX  -> GPS RX  (often unused)

// ---------------------------------------------------------------------------
//  Scanning behaviour
//  To keep the SoftAP stable, WiFi scans hop ONE channel per cycle (passive)
//  instead of abandoning the AP channel for a full ~1.5 s sweep. The AP stays
//  on its home channel the vast majority of the time.
// ---------------------------------------------------------------------------
#define WIFI_SCAN_INTERVAL_MS   2000    // how often to scan the next channel
#define WIFI_SCAN_PER_CH_MS     200     // dwell time (>=1 beacon interval)
#define WIFI_SCAN_PASSIVE       true    // passive = far less AP disruption
#define WIFI_MAX_CHANNEL        13      // top of the 2.4 GHz channel rotation

#define MAX_DEVICES         4000        // hard cap on stored unique devices
#define PACKET_LOG_DEPTH    128         // recent frames kept for the monitor

// ---------------------------------------------------------------------------
//  Persistence
// ---------------------------------------------------------------------------
#define AUTOSAVE_ENABLED    true
#define AUTOSAVE_INTERVAL_MS 30000      // flush capture log to LittleFS
// Bumped to /capture2 so an old BLE-era backup isn't reloaded after upgrading.
#define CAPTURE_LOG_PATH    "/capture2.csv"

// ---------------------------------------------------------------------------
//  Status LED (DevKitC-1 RGB on GPIO48). Set to -1 to disable.
// ---------------------------------------------------------------------------
#define STATUS_LED_PIN      48

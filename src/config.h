// ===========================================================================
//  config.h  —  Compile-time configuration for the ESP32-S3 WarDriver
// ===========================================================================
#pragma once

// ---- Soft Access Point (the dashboard you connect your phone/laptop to) ----
// The device hosts its own WiFi network. Connect to it, then browse to
// http://192.168.4.1/  to reach the dashboard.
#define AP_SSID          "WARDRIVER"
#define AP_PASSWORD      "roguehunter"   // >= 8 chars, or "" for an open AP
#define AP_CHANNEL       6
#define AP_MAX_CLIENTS   4

// ---- Optional: also join an existing WiFi network (Station mode) -----------
// Leave STA_SSID empty ("") to run in pure Access-Point mode (recommended for
// portable field use). If set, the device joins this network *and* keeps the
// SoftAP up, so you can reach the dashboard over your LAN too.
#define STA_SSID         ""
#define STA_PASSWORD     ""

// ---- mDNS hostname  (browse to http://wardriver.local/ when on STA) --------
#define MDNS_HOSTNAME    "wardriver"

// ---- Scanner behaviour -----------------------------------------------------
#define WIFI_SCAN_ENABLED_DEFAULT   true
#define BLE_SCAN_ENABLED_DEFAULT    true

#define WIFI_SCAN_CHANNEL_DWELL_MS  120     // per-channel dwell for active scan
#define BLE_SCAN_SECONDS            5       // BLE scan window per cycle
#define CYCLE_PAUSE_MS              250     // pause between full scan cycles

// ---- Capacity limits (kept in RAM; logs on flash are unbounded-ish) --------
#define MAX_WIFI_APS     300
#define MAX_BLE_DEVS     300

// ---- Logging ---------------------------------------------------------------
#define LOG_TO_FLASH        true
#define WIFI_LOG_PATH       "/wifi_log.csv"
#define BLE_LOG_PATH        "/ble_log.csv"
#define WHITELIST_PATH      "/whitelist.json"
// Roll the log file once it grows past this size (bytes) to protect the FS.
#define LOG_MAX_BYTES       (6UL * 1024UL * 1024UL)

// ---- Onboard RGB status LED (DevKitC-1 addressable LED on GPIO 48) ---------
#define STATUS_LED_PIN   48
#define STATUS_LED_ON    true

// ---- Firmware identity -----------------------------------------------------
#define FW_NAME          "ESP32-S3 WarDriver"
#define FW_VERSION       "1.0.0"

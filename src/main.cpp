// ===========================================================================
//  ESP32-S3 WarDriver  —  main.cpp
//
//  WiFi + BLE survey / rogue-AP detection appliance with a self-hosted
//  hacker-themed web dashboard.
//
//  Target: ESP32-S3-DevKitC-1 N16R8 (16 MB flash / 8 MB OPI PSRAM)
//
//  USE RESPONSIBLY: only scan networks you are authorised to assess.
// ===========================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <vector>
#include <algorithm>

#include "config.h"
#include "models.h"
#include "oui.h"
#include "web_assets.h"
#include "blue_routes.h"   // BlueDriver link: /ingest + /api/blue/* endpoints

// ---------------------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------------------
AsyncWebServer  server(80);
AsyncWebSocket  ws("/ws");

// Canonical stores, keyed for de-duplication across scan cycles.
static std::map<String, WiFiAP>  g_wifi;     // key = bssid
static std::map<String, BLEDev>  g_ble;      // key = address
SemaphoreHandle_t                g_mutex;     // guards the two maps above

// Whitelist of known-good APs:  ssid -> set of allowed BSSIDs (lowercase).
static std::map<String, std::vector<String>> g_whitelist;

// Runtime state
struct State {
    bool     wifiScanEnabled = WIFI_SCAN_ENABLED_DEFAULT;
    bool     bleScanEnabled  = BLE_SCAN_ENABLED_DEFAULT;
    bool     scanning        = false;
    uint32_t cycles          = 0;
    uint32_t wifiSeenTotal   = 0;
    uint32_t bleSeenTotal    = 0;
    uint32_t rogueCount      = 0;
    uint32_t lastCycleMs     = 0;
    // String literals only (static lifetime) — written by loop, read by the
    // async web task. A plain pointer write is atomic on the ESP32, so this is
    // race-safe whereas an Arduino String (which reallocates) would not be.
    const char* phase        = "idle";
} g_state;

// JSON documents live in PSRAM so large device tables never starve the heap.
struct SpiRamAllocator : ArduinoJson::Allocator {
    void* allocate(size_t n) override   { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
    void  deallocate(void* p) override  { heap_caps_free(p); }
    void* reallocate(void* p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
};
static SpiRamAllocator g_psram;

// ---------------------------------------------------------------------------
//  Small helpers
// ---------------------------------------------------------------------------
static inline void lockData()   { xSemaphoreTake(g_mutex, portMAX_DELAY); }
static inline void unlockData() { xSemaphoreGive(g_mutex); }

static const char* authModeToStr(wifi_auth_mode_t m) {
    switch (m) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
        default:                        return "UNKNOWN";
    }
}

static bool encIsOpenOrWeak(const String& e) {
    return e == "OPEN" || e == "WEP";
}

// ---------------------------------------------------------------------------
//  Status LED (DevKitC-1 addressable RGB on GPIO 48)
// ---------------------------------------------------------------------------
static void setLed(uint8_t r, uint8_t g, uint8_t b) {
#if STATUS_LED_ON
    neopixelWrite(STATUS_LED_PIN, r, g, b);
#endif
}

// ---------------------------------------------------------------------------
//  Whitelist persistence (LittleFS JSON)
// ---------------------------------------------------------------------------
static void loadWhitelist() {
    g_whitelist.clear();
    if (!LittleFS.exists(WHITELIST_PATH)) return;
    File f = LittleFS.open(WHITELIST_PATH, "r");
    if (!f) return;
    JsonDocument doc(&g_psram);
    if (deserializeJson(doc, f) == DeserializationError::Ok && doc.is<JsonObject>()) {
        for (JsonPair kv : doc.as<JsonObject>()) {
            std::vector<String> bssids;
            for (JsonVariant v : kv.value().as<JsonArray>()) {
                String b = v.as<String>(); b.toLowerCase();
                bssids.push_back(b);
            }
            g_whitelist[String(kv.key().c_str())] = bssids;
        }
    }
    f.close();
}

static void saveWhitelist() {
    JsonDocument doc(&g_psram);
    JsonObject root = doc.to<JsonObject>();
    for (auto& kv : g_whitelist) {
        JsonArray arr = root[kv.first].to<JsonArray>();
        for (auto& b : kv.second) arr.add(b);
    }
    File f = LittleFS.open(WHITELIST_PATH, "w");
    if (f) { serializeJson(doc, f); f.close(); }
}

static bool bssidWhitelisted(const String& ssid, const String& bssid) {
    auto it = g_whitelist.find(ssid);
    if (it == g_whitelist.end()) return false;
    return std::find(it->second.begin(), it->second.end(), bssid) != it->second.end();
}

// ---------------------------------------------------------------------------
//  Logging to flash (CSV)
// ---------------------------------------------------------------------------
static void ensureLogHeaders() {
    if (!LittleFS.exists(WIFI_LOG_PATH)) {
        File f = LittleFS.open(WIFI_LOG_PATH, "w");
        if (f) { f.println("uptime_s,ssid,bssid,rssi,channel,encryption,vendor,hidden,severity,reason"); f.close(); }
    }
    if (!LittleFS.exists(BLE_LOG_PATH)) {
        File f = LittleFS.open(BLE_LOG_PATH, "w");
        if (f) { f.println("uptime_s,address,addr_type,name,rssi,vendor,manufacturer,appearance,services"); f.close(); }
    }
}

static String csvEscape(const String& s) {
    String out = s; out.replace("\"", "\"\"");
    return "\"" + out + "\"";
}

static void rollIfTooBig(const char* path) {
    File f = LittleFS.open(path, "r");
    if (f) {
        size_t sz = f.size();
        f.close();
        if (sz > LOG_MAX_BYTES) {
            String bak = String(path) + ".1";
            LittleFS.remove(bak);
            LittleFS.rename(path, bak);
        }
    }
}

// Append one row to an already-open file. Callers batch all writes into a
// single open/close so we never thrash the flash (or hold the data mutex)
// opening the log hundreds of times per scan.
static void writeWifiLine(File& f, const WiFiAP& a) {
    const char* sev = a.severity == SEV_ALERT ? "ALERT" : a.severity == SEV_WATCH ? "WATCH" : "OK";
    f.printf("%lu,%s,%s,%ld,%u,%s,%s,%d,%s,%s\n",
             millis() / 1000UL, csvEscape(a.ssid).c_str(), a.bssid.c_str(),
             (long)a.rssi, a.channel, a.encryption.c_str(),
             csvEscape(a.vendor).c_str(), a.hidden ? 1 : 0, sev,
             csvEscape(a.reason).c_str());
}

static void writeBleLine(File& f, const BLEDev& d) {
    f.printf("%lu,%s,%s,%s,%ld,%s,%s,%d,%s\n",
             millis() / 1000UL, d.address.c_str(), d.addrType.c_str(),
             csvEscape(d.name).c_str(), (long)d.rssi, csvEscape(d.vendor).c_str(),
             d.manufacturer.c_str(), d.appearance, csvEscape(d.services).c_str());
}

// ---------------------------------------------------------------------------
//  Rogue-AP detection
//
//  Runs over the full current set of WiFi APs after every scan. Flags:
//   * ALERT  — an AP broadcasts a whitelisted SSID from a BSSID that is NOT
//              on the allow-list  => classic "evil twin" / spoofed AP.
//   * ALERT  — a whitelisted (normally secured) SSID seen OPEN/WEP.
//   * WATCH  — same SSID advertised by multiple BSSIDs with mismatched
//              encryption (possible impersonation of an un-listed network).
//   * WATCH  — an un-listed SSID appears OPEN while a same-named secured
//              network also exists.
// ---------------------------------------------------------------------------
static void evaluateRogues() {
    // Build SSID -> list of APs from the live set (held under lock by caller).
    std::map<String, std::vector<WiFiAP*>> bySsid;
    for (auto& kv : g_wifi) {
        kv.second.severity = SEV_NONE;
        kv.second.reason   = "";
        if (kv.second.ssid.length()) bySsid[kv.second.ssid].push_back(&kv.second);
    }

    uint32_t rogues = 0;
    for (auto& kv : bySsid) {
        const String& ssid = kv.first;
        auto& aps = kv.second;
        bool listed = g_whitelist.count(ssid) > 0;

        // Encryption diversity for this SSID across all observed BSSIDs.
        bool anyOpen = false, anySecure = false;
        for (auto* ap : aps) {
            if (encIsOpenOrWeak(ap->encryption)) anyOpen = true; else anySecure = true;
        }

        for (auto* ap : aps) {
            if (listed) {
                if (!bssidWhitelisted(ssid, ap->bssid)) {
                    ap->severity = SEV_ALERT;
                    ap->reason   = "Unknown BSSID broadcasting a trusted SSID (possible evil twin)";
                } else if (encIsOpenOrWeak(ap->encryption)) {
                    ap->severity = SEV_ALERT;
                    ap->reason   = "Trusted SSID is OPEN/WEP — should be encrypted";
                }
            } else {
                if (aps.size() > 1 && anyOpen && anySecure && encIsOpenOrWeak(ap->encryption)) {
                    ap->severity = SEV_WATCH;
                    ap->reason   = "OPEN clone of a same-named secured network";
                } else if (aps.size() > 2) {
                    ap->severity = std::max<Severity>(ap->severity, SEV_WATCH);
                    if (ap->reason.isEmpty())
                        ap->reason = "SSID broadcast by " + String((int)aps.size()) + " BSSIDs";
                }
            }
            if (ap->severity == SEV_ALERT) rogues++;
        }
    }
    g_state.rogueCount = rogues;
}

// ---------------------------------------------------------------------------
//  WiFi scanning (asynchronous, non-blocking)
// ---------------------------------------------------------------------------
static bool    g_wifiScanInFlight = false;
static uint8_t g_scanChannel      = 1;   // rotated one channel per cycle

static void startWifiScan() {
    if (g_wifiScanInFlight) return;
    // Scan ONE channel per cycle, not a full sweep. In AP(+STA) the single radio
    // must leave the SoftAP's channel to scan, so a 13-channel sweep would park
    // the AP off-channel ~1.5 s at a time and the connected BlueDriver/clients
    // would keep dropping. One channel per cycle keeps the AP on-channel and the
    // /ingest link stable; the full band is still covered every ~13 cycles.
    // args: async, show_hidden, passive, dwell ms, channel
    WiFi.scanNetworks(true, true, false, WIFI_SCAN_CHANNEL_DWELL_MS, g_scanChannel);
    g_wifiScanInFlight = true;
    g_state.phase = "wifi";
}

static void collectWifiScan() {
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING || n == WIFI_SCAN_FAILED) {
        if (n == WIFI_SCAN_FAILED) g_wifiScanInFlight = false;
        return;
    }
    uint32_t now = millis();
    lockData();
    for (int i = 0; i < n; i++) {
        const uint8_t* raw = WiFi.BSSID(i);
        String bssid = macToStr(raw);
        WiFiAP& ap = g_wifi[bssid];
        bool isNew = (ap.timesSeen == 0);
        ap.bssid = bssid;
        memcpy(ap.bssidRaw, raw, 6);
        ap.ssid       = WiFi.SSID(i);
        ap.hidden     = ap.ssid.isEmpty();
        ap.rssi       = WiFi.RSSI(i);
        ap.rssiBest   = std::max(ap.rssiBest, ap.rssi);
        ap.channel    = WiFi.channel(i);
        ap.encryption = authModeToStr(WiFi.encryptionType(i));
        ap.vendor     = ouiLookup(raw);
        if (isNew) { ap.firstSeenMs = now; g_state.wifiSeenTotal++; }
        ap.lastSeenMs = now;
        ap.timesSeen++;
    }
    // Cap memory: if over the limit, drop the oldest-seen entries.
    if (g_wifi.size() > MAX_WIFI_APS) {
        std::vector<std::pair<uint32_t, String>> ages;
        for (auto& kv : g_wifi) ages.push_back({kv.second.lastSeenMs, kv.first});
        std::sort(ages.begin(), ages.end());
        for (size_t i = 0; i + MAX_WIFI_APS < ages.size(); i++) g_wifi.erase(ages[i].second);
    }
    evaluateRogues();
    // Log first sightings, plus any flagged AP on each cycle it is seen.
    // One open/close for the whole batch.
    if (LOG_TO_FLASH) {
        rollIfTooBig(WIFI_LOG_PATH);
        File lf = LittleFS.open(WIFI_LOG_PATH, "a");
        if (lf) {
            for (auto& kv : g_wifi) {
                WiFiAP& a = kv.second;
                if (a.lastSeenMs == now && (a.timesSeen == 1 || a.severity != SEV_NONE))
                    writeWifiLine(lf, a);
            }
            lf.close();
        }
    }
    unlockData();

    WiFi.scanDelete();
    g_wifiScanInFlight = false;
    if (++g_scanChannel > 13) g_scanChannel = 1;   // advance for the next cycle
}

// ---------------------------------------------------------------------------
//  BLE ingest
//
//  This board no longer runs its own BLE radio. The companion ESP32-BlueDriver
//  scans BLE and POSTs its finds to /ingest (see BlueLink / blue_routes), which
//  calls this to merge each device into the shared store. Keeping BLE off this
//  board's radio also removes the WiFi/BT coexistence that destabilised the AP.
// ---------------------------------------------------------------------------
bool ingestBleDevice(const String& macIn, const String& name,
                     const String& vendor, int rssi, uint32_t hits) {
    String mac = macIn; mac.toLowerCase();
    if (mac.length() != 17) return false;            // basic "aa:bb:.." sanity
    uint32_t now = millis();

    lockData();
    BLEDev& d = g_ble[mac];
    bool isNew = (d.timesSeen == 0);
    d.address = mac;
    if (name.length())   d.name   = name;
    if (vendor.length()) d.vendor = vendor;
    else if (d.vendor.isEmpty()) d.vendor = "Unknown";
    d.addrType  = (vendor == "Randomized/Local") ? "random" : "public";
    d.rssi      = rssi;
    d.rssiBest  = std::max(d.rssiBest, (int32_t)rssi);
    d.timesSeen = hits ? hits : d.timesSeen + 1;
    if (isNew) { d.firstSeenMs = now; g_state.bleSeenTotal++; }
    d.lastSeenMs = now;

    // Newly-seen devices get logged once. Open/close per device is fine here:
    // ingest batches are small (<= ~40) and arrive only every few seconds.
    if (isNew && LOG_TO_FLASH) {
        rollIfTooBig(BLE_LOG_PATH);
        File lf = LittleFS.open(BLE_LOG_PATH, "a");
        if (lf) { writeBleLine(lf, d); lf.close(); }
    }

    // Enforce the in-RAM cap by evicting the single oldest entry if needed.
    if (g_ble.size() > MAX_BLE_DEVS) {
        auto oldest = g_ble.begin();
        for (auto it = g_ble.begin(); it != g_ble.end(); ++it)
            if (it->second.lastSeenMs < oldest->second.lastSeenMs) oldest = it;
        if (oldest->first != mac) g_ble.erase(oldest);
    }
    unlockData();
    return isNew;
}

// ---------------------------------------------------------------------------
//  JSON builders
// ---------------------------------------------------------------------------
static void buildStatusDoc(JsonDocument& doc) {
    doc["fw"]            = FW_NAME;
    doc["version"]       = FW_VERSION;
    doc["uptime_s"]      = millis() / 1000UL;
    doc["scanning"]      = g_state.scanning;
    doc["phase"]         = g_state.phase;
    doc["wifi_enabled"]  = g_state.wifiScanEnabled;
    doc["ble_enabled"]   = g_state.bleScanEnabled;
    doc["cycles"]        = g_state.cycles;
    doc["wifi_total"]    = g_state.wifiSeenTotal;
    doc["ble_total"]     = g_state.bleSeenTotal;
    doc["rogue_count"]   = g_state.rogueCount;
    lockData();
    doc["wifi_live"]       = g_wifi.size();
    doc["ble_live"]        = g_ble.size();
    doc["whitelist_ssids"] = (int)g_whitelist.size();
    unlockData();
    doc["heap_free"]     = ESP.getFreeHeap();
    doc["psram_free"]    = ESP.getFreePsram();
    doc["ap_ip"]         = WiFi.softAPIP().toString();
    doc["sta_ip"]        = WiFi.localIP().toString();
    doc["sta_connected"] = WiFi.isConnected();
    doc["clients"]       = WiFi.softAPgetStationNum();
}

static void buildWifiDoc(JsonDocument& doc) {
    JsonArray arr = doc.to<JsonArray>();
    uint32_t now = millis();
    lockData();
    for (auto& kv : g_wifi) {
        const WiFiAP& a = kv.second;
        JsonObject o = arr.add<JsonObject>();
        o["ssid"]    = a.hidden ? "<hidden>" : a.ssid;
        o["bssid"]   = a.bssid;
        o["rssi"]    = a.rssi;
        o["rssi_best"] = a.rssiBest;
        o["quality"] = rssiToQuality(a.rssi);
        o["channel"] = a.channel;
        o["enc"]     = a.encryption;
        o["vendor"]  = a.vendor;
        o["hidden"]  = a.hidden;
        o["seen"]    = a.timesSeen;
        o["age_s"]   = (now - a.lastSeenMs) / 1000UL;
        o["sev"]     = (int)a.severity;
        o["reason"]  = a.reason;
    }
    unlockData();
}

static void buildBleDoc(JsonDocument& doc) {
    JsonArray arr = doc.to<JsonArray>();
    uint32_t now = millis();
    lockData();
    for (auto& kv : g_ble) {
        const BLEDev& d = kv.second;
        JsonObject o = arr.add<JsonObject>();
        o["address"] = d.address;
        o["name"]    = d.name.length() ? d.name : String("");
        o["type"]    = d.addrType;
        o["rssi"]    = d.rssi;
        o["rssi_best"] = d.rssiBest;
        o["quality"] = rssiToQuality(d.rssi);
        o["vendor"]  = d.vendor;
        o["mfg"]     = d.manufacturer;
        o["appearance"] = d.appearance;
        o["services"]   = d.services;
        o["seen"]    = d.timesSeen;
        o["age_s"]   = (now - d.lastSeenMs) / 1000UL;
    }
    unlockData();
}

static void buildRoguesDoc(JsonDocument& doc) {
    JsonArray arr = doc.to<JsonArray>();
    lockData();
    for (auto& kv : g_wifi) {
        const WiFiAP& a = kv.second;
        if (a.severity == SEV_NONE) continue;
        JsonObject o = arr.add<JsonObject>();
        o["ssid"]   = a.hidden ? "<hidden>" : a.ssid;
        o["bssid"]  = a.bssid;
        o["rssi"]   = a.rssi;
        o["channel"]= a.channel;
        o["enc"]    = a.encryption;
        o["vendor"] = a.vendor;
        o["sev"]    = (int)a.severity;
        o["reason"] = a.reason;
    }
    unlockData();
}

// Stream a JsonDocument as a chunked HTTP response (low heap usage).
static void sendJson(AsyncWebServerRequest* req, JsonDocument& doc) {
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    serializeJson(doc, *res);
    req->send(res);
}

// ---------------------------------------------------------------------------
//  WebSocket — pushes the lightweight status object each cycle
// ---------------------------------------------------------------------------
static void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
                      AwsEventType type, void*, uint8_t*, size_t) {
    if (type == WS_EVT_CONNECT) {
        JsonDocument doc(&g_psram);
        buildStatusDoc(doc);
        String out; serializeJson(doc, out);
        client->text(out);
    }
}

static void pushStatusWs() {
    if (ws.count() == 0) return;
    JsonDocument doc(&g_psram);
    buildStatusDoc(doc);
    String out; serializeJson(doc, out);
    ws.textAll(out);
}

// ---------------------------------------------------------------------------
//  HTTP routes
// ---------------------------------------------------------------------------
static void setupRoutes() {
    // --- Dashboard (served from PROGMEM, gzip-free for simplicity) ----------
    // Use the explicit-length send_P overload (available in every version) to
    // avoid overload ambiguity. On the ESP32 flash is memory-mapped, so the
    // content is streamed straight from PROGMEM without copying into RAM.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html; charset=utf-8", (const uint8_t*)INDEX_HTML, strlen_P(INDEX_HTML));
    });
    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "application/javascript; charset=utf-8", (const uint8_t*)APP_JS, strlen_P(APP_JS));
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/css; charset=utf-8", (const uint8_t*)STYLE_CSS, strlen_P(STYLE_CSS));
    });

    // --- Data APIs ----------------------------------------------------------
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc(&g_psram); buildStatusDoc(doc); sendJson(req, doc);
    });
    server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc(&g_psram); buildWifiDoc(doc); sendJson(req, doc);
    });
    server.on("/api/ble", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc(&g_psram); buildBleDoc(doc); sendJson(req, doc);
    });
    server.on("/api/rogues", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc(&g_psram); buildRoguesDoc(doc); sendJson(req, doc);
    });

    // --- Scan control -------------------------------------------------------
    server.on("/api/control", HTTP_POST, [](AsyncWebServerRequest* req) {
        auto val = [&](const char* k, bool def) {
            if (!req->hasParam(k, true)) return def;
            String v = req->getParam(k, true)->value();
            return v == "1" || v == "true" || v == "on";
        };
        if (req->hasParam("scanning", true)) g_state.scanning        = val("scanning", g_state.scanning);
        if (req->hasParam("wifi", true))     g_state.wifiScanEnabled = val("wifi", g_state.wifiScanEnabled);
        if (req->hasParam("ble", true))      g_state.bleScanEnabled  = val("ble", g_state.bleScanEnabled);
        JsonDocument doc(&g_psram); buildStatusDoc(doc); sendJson(req, doc);
    });

    // --- Clear in-memory tables --------------------------------------------
    server.on("/api/clear", HTTP_POST, [](AsyncWebServerRequest* req) {
        String what = req->hasParam("what", true) ? req->getParam("what", true)->value() : "all";
        lockData();
        if (what == "wifi" || what == "all") g_wifi.clear();
        if (what == "ble"  || what == "all") g_ble.clear();
        g_state.rogueCount = 0;
        unlockData();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // --- Whitelist management ----------------------------------------------
    server.on("/api/whitelist", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc(&g_psram);
        JsonObject root = doc.to<JsonObject>();
        lockData();
        for (auto& kv : g_whitelist) {
            JsonArray a = root[kv.first].to<JsonArray>();
            for (auto& b : kv.second) a.add(b);
        }
        unlockData();
        sendJson(req, doc);
    });
    server.on("/api/whitelist", HTTP_POST, [](AsyncWebServerRequest* req) {
        // params: action=add|remove, ssid=..., bssid=...
        String action = req->hasParam("action", true) ? req->getParam("action", true)->value() : "";
        String ssid   = req->hasParam("ssid", true)   ? req->getParam("ssid", true)->value()   : "";
        String bssid  = req->hasParam("bssid", true)  ? req->getParam("bssid", true)->value()  : "";
        bssid.toLowerCase();
        if (ssid.isEmpty()) { req->send(400, "application/json", "{\"error\":\"ssid required\"}"); return; }
        lockData();
        if (action == "add" && bssid.length()) {
            auto& v = g_whitelist[ssid];
            if (std::find(v.begin(), v.end(), bssid) == v.end()) v.push_back(bssid);
        } else if (action == "remove") {
            auto it = g_whitelist.find(ssid);
            if (it != g_whitelist.end()) {
                if (bssid.isEmpty()) g_whitelist.erase(it);
                else {
                    auto& v = it->second;
                    v.erase(std::remove(v.begin(), v.end(), bssid), v.end());
                    if (v.empty()) g_whitelist.erase(it);
                }
            }
        }
        saveWhitelist();
        evaluateRogues();
        unlockData();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    // --- Log download / clear ----------------------------------------------
    server.on("/api/logs/wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (LittleFS.exists(WIFI_LOG_PATH))
            req->send(LittleFS, WIFI_LOG_PATH, "text/csv", true);
        else req->send(404, "text/plain", "no log");
    });
    server.on("/api/logs/ble", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (LittleFS.exists(BLE_LOG_PATH))
            req->send(LittleFS, BLE_LOG_PATH, "text/csv", true);
        else req->send(404, "text/plain", "no log");
    });
    server.on("/api/logs", HTTP_DELETE, [](AsyncWebServerRequest* req) {
        LittleFS.remove(WIFI_LOG_PATH);
        LittleFS.remove(BLE_LOG_PATH);
        ensureLogHeaders();
        req->send(200, "application/json", "{\"ok\":true}");
    });

    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "404");
    });

    // BlueDriver link: POST /ingest (device uplink + command channel) and the
    // /api/blue/* control endpoints. Also initialises the BlueLink singleton.
    registerBlueRoutes(server);

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
}

// ---------------------------------------------------------------------------
//  Network bring-up
// ---------------------------------------------------------------------------
static void startNetwork() {
    bool useSta = strlen(STA_SSID) > 0;
    WiFi.mode(useSta ? WIFI_AP_STA : WIFI_AP);
    WiFi.setSleep(false);

    WiFi.softAP(AP_SSID, strlen(AP_PASSWORD) ? AP_PASSWORD : nullptr, AP_CHANNEL, 0, AP_MAX_CLIENTS);
    Serial.printf("[NET] SoftAP  '%s'  ->  http://%s/\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    if (useSta) {
        WiFi.begin(STA_SSID, STA_PASSWORD);
        Serial.printf("[NET] Joining '%s' ...\n", STA_SSID);
        uint32_t t0 = millis();
        while (!WiFi.isConnected() && millis() - t0 < 12000) { delay(250); Serial.print('.'); }
        Serial.println();
        if (WiFi.isConnected())
            Serial.printf("[NET] STA IP   ->  http://%s/\n", WiFi.localIP().toString().c_str());
    }
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[NET] mDNS     ->  http://%s.local/\n", MDNS_HOSTNAME);
    }
}

// ---------------------------------------------------------------------------
//  Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n=== %s v%s ===\n", FW_NAME, FW_VERSION);
    Serial.printf("[SYS] PSRAM: %u bytes free\n", ESP.getFreePsram());

    setLed(0, 0, 16);   // dim blue = booting

    g_mutex = xSemaphoreCreateMutex();

    if (!LittleFS.begin(true)) Serial.println("[FS] LittleFS mount failed!");
    else { ensureLogHeaders(); loadWhitelist(); }

    startNetwork();

    // BLE comes from the companion ESP32-BlueDriver over the /ingest link
    // (registered inside setupRoutes); this board runs WiFi only.
    setupRoutes();

    g_state.scanning = true;
    Serial.println("[SYS] Ready. Scanning engaged.");
    setLed(0, 16, 0);   // green = ready
}

// ---------------------------------------------------------------------------
//  Main loop — sequential WiFi then BLE scan cycles
// ---------------------------------------------------------------------------
void loop() {
    ws.cleanupClients();

    if (!g_state.scanning) {
        g_state.phase = "paused";
        setLed(8, 8, 0);          // amber = paused
        pushStatusWs();
        delay(400);
        return;
    }

    setLed(16, 0, 0);             // red flash = active scan

    // ---- WiFi phase (non-blocking) ----
    if (g_state.wifiScanEnabled) {
        startWifiScan();
        uint32_t t0 = millis();
        while (g_wifiScanInFlight && millis() - t0 < 8000) {
            collectWifiScan();
            ws.cleanupClients();
            delay(40);
        }
        if (g_wifiScanInFlight) { WiFi.scanDelete(); g_wifiScanInFlight = false; } // timeout guard
    }

    // BLE devices stream in asynchronously from the companion BlueDriver via
    // POST /ingest, so there is no local BLE scan phase in this loop.

    g_state.cycles++;
    g_state.lastCycleMs = millis();
    g_state.phase = "idle";
    setLed(0, 16, 0);             // green between cycles

    pushStatusWs();
    delay(CYCLE_PAUSE_MS);
}

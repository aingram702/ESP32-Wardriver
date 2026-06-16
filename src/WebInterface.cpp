#include "WebInterface.h"
#include "web_assets.h"
#include "config.h"
#include "types.h"
#include "Modes.h"
#include "NetworkStore.h"
#include "GpsModule.h"
#include "PacketSniffer.h"
#include "WifiScanner.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

WebInterface g_web;
static AsyncWebServer s_server(80);
static DNSServer      s_dns;
static const IPAddress AP_IP(192, 168, 4, 1);

// ---- helpers ---------------------------------------------------------------
static bool authOK(AsyncWebServerRequest* req) {
#if WEB_AUTH_ENABLED
    if (!req->authenticate(WEB_AUTH_USER, WEB_AUTH_PASS)) return false;
#endif
    return true;
}
#define REQUIRE_AUTH(req) do { if(!authOK(req)){ req->requestAuthentication(); return; } } while(0)

// Read a POST/GET param as String (POST body takes priority).
static String param(AsyncWebServerRequest* req, const char* name) {
    if (req->hasParam(name, true))  return req->getParam(name, true)->value();
    if (req->hasParam(name, false)) return req->getParam(name, false)->value();
    return String();
}

static void sendJson(AsyncWebServerRequest* req, const String& body, int code = 200) {
    AsyncWebServerResponse* r = req->beginResponse(code, "application/json", body);
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
}

// ---- endpoints -------------------------------------------------------------
static void handleStatus(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    // Count wifi/ble cheaply by streaming once would be heavy; recompute small.
    size_t total = g_store.count();
    size_t threats = g_store.dangerousCount();
    GpsFix gx = g_gps.fix();

    // Cheap O(n) wifi/ble split (n is capped by MAX_DEVICES).
    size_t nWifi = 0, nBle = 0; g_store.typeCounts(nWifi, nBle);

    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->addHeader("Cache-Control", "no-store");
    res->print("{\"mode\":\""); res->print(currentModeName());
    res->print("\",\"devices\":"); res->print(total);
    res->print(",\"wifi\":"); res->print(nWifi);
    res->print(",\"ble\":");  res->print(nBle);
    res->print(",\"threats\":"); res->print(threats);
    res->print(",\"uptime\":"); res->print(millis() / 1000);
    res->print(",\"heap\":");   res->print(ESP.getFreeHeap());
    res->print(",\"ssid\":\""); res->print(AP_SSID);
    res->print("\",\"ip\":\""); res->print(WiFi.softAPIP().toString());
    res->print("\",\"apChannel\":"); res->print(AP_CHANNEL);
    res->print(",\"lastScan\":"); res->print(g_wifiScanner.lastScanMs());
    res->print(",\"gps\":{\"present\":"); res->print(g_gps.present() ? "true":"false");
    res->print(",\"fix\":"); res->print(gx.valid ? "true":"false");
    res->print(",\"sats\":"); res->print(gx.satellites);
    res->print(",\"lat\":"); res->print(gx.lat, 6);
    res->print(",\"lon\":"); res->print(gx.lon, 6);
    res->print("}}");
    req->send(res);
}

static void handleDevices(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->addHeader("Cache-Control", "no-store");
    g_store.streamJson(*res);
    req->send(res);
}

static void handleExport(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    AsyncResponseStream* res = req->beginResponseStream("text/csv");
    res->addHeader("Content-Disposition", "attachment; filename=wardrive.csv");
    g_store.streamCsv(*res);
    req->send(res);
}

static void handleClear(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    g_store.clear();
    sendJson(req, "{\"ok\":true}");
}

static void handleMode(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    String m = param(req, "mode");
    if (m == "attack")        requestMode(MODE_ATTACK);
    else if (m == "wardrive") requestMode(MODE_WARDRIVE);
    else { sendJson(req, "{\"error\":\"bad mode\"}", 400); return; }
    sendJson(req, String("{\"mode\":\"") + currentModeName() + "\"}");
}

static void handlePackets(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    PacketStats st = g_sniffer.stats();
    static PacketRecord recs[PACKET_LOG_DEPTH];
    size_t n = g_sniffer.recent(recs, PACKET_LOG_DEPTH);

    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->addHeader("Cache-Control", "no-store");
    res->print("{\"active\":"); res->print(g_sniffer.active() ? "true":"false");
    res->print(",\"channel\":"); res->print(g_sniffer.channel());
    res->print(",\"stats\":{\"total\":"); res->print(st.total);
    res->print(",\"mgmt\":"); res->print(st.mgmt);
    res->print(",\"ctrl\":"); res->print(st.ctrl);
    res->print(",\"data\":"); res->print(st.data);
    res->print(",\"beacon\":"); res->print(st.beacon);
    res->print(",\"probe\":"); res->print(st.probe);
    res->print(",\"deauth\":"); res->print(st.deauth);
    res->print(",\"eapol\":"); res->print(st.eapol);
    res->print("},\"recent\":[");
    char sm[18], dm[18];
    for (size_t i = 0; i < n; i++) {
        if (i) res->print(',');
        macToStr(recs[i].src, sm); macToStr(recs[i].dst, dm);
        res->print("{\"t\":"); res->print(recs[i].tMs);
        res->print(",\"kind\":\""); res->print(recs[i].kind);
        res->print("\",\"src\":\""); res->print(sm);
        res->print("\",\"dst\":\""); res->print(dm);
        res->print("\",\"ch\":"); res->print(recs[i].channel);
        res->print(",\"rssi\":"); res->print(recs[i].rssi);
        res->print('}');
    }
    res->print("]}");
    req->send(res);
}

static void handleSniff(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    if (currentMode() != MODE_ATTACK) {
        sendJson(req, "{\"error\":\"enter attack mode first\"}", 409); return;
    }
    String action = param(req, "action");
    long ch = param(req, "channel").toInt();
    if (action == "start")      g_sniffer.start((uint8_t)ch);
    else if (action == "stop")  g_sniffer.stop();
    else if (action == "reset") g_sniffer.resetStats();
    else if (action == "channel") {
        if (ch < 1 || ch > 14) { sendJson(req,"{\"error\":\"bad channel\"}",400); return; }
        g_sniffer.setChannel((uint8_t)ch);
    } else { sendJson(req, "{\"error\":\"bad action\"}", 400); return; }
    sendJson(req, "{\"ok\":true}");
}

static void handleDeauth(AsyncWebServerRequest* req) {
    REQUIRE_AUTH(req);
    if (currentMode() != MODE_ATTACK || !g_sniffer.active()) {
        sendJson(req, "{\"error\":\"start the sniffer in attack mode first\"}", 409);
        return;
    }
    uint8_t ap[6], cl[6] = {0};
    if (!strToMac(param(req, "ap"), ap)) {
        sendJson(req, "{\"error\":\"invalid AP BSSID\"}", 400); return;
    }
    String c = param(req, "client");
    bool haveClient = c.length() && strToMac(c, cl);
    long bursts = param(req, "bursts").toInt();
    if (bursts < 1)   bursts = 1;
    if (bursts > 500) bursts = 500;   // clamp to keep the radio sane

    uint32_t sent = g_sniffer.deauth(ap, haveClient ? cl : nullptr, (uint16_t)bursts);
    sendJson(req, String("{\"sent\":") + sent + "}");
}

// ---- public ----------------------------------------------------------------
void WebInterface::begin() {
    if (_started) return;

    // Pin an explicit SoftAP subnet, then bring up the AP (WIFI_AP_STA was set
    // by the WiFi scanner, which also disabled modem sleep for AP stability).
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN, AP_MAX_CLIENTS);
    WiFi.setSleep(false);          // belt-and-braces: keep the radio awake
    delay(100);
    Serial.printf("[WEB] SoftAP \"%s\" up at http://%s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());

    // Captive portal: answer every DNS query with our own IP so phones pop the
    // sign-in page and the UI is reliably reachable without typing the IP.
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", AP_IP);

    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
        REQUIRE_AUTH(req);
        // INDEX_HTML lives in flash; on the ESP32 flash is memory-mapped so it
        // can be read as a normal C-string. Stream it to avoid a large heap copy.
        AsyncResponseStream* res = req->beginResponseStream("text/html");
        res->print(INDEX_HTML);
        req->send(res);
    });

    s_server.on("/api/status",     HTTP_GET,  handleStatus);
    s_server.on("/api/devices",    HTTP_GET,  handleDevices);
    s_server.on("/api/export.csv", HTTP_GET,  handleExport);
    s_server.on("/api/packets",    HTTP_GET,  handlePackets);
    s_server.on("/api/clear",      HTTP_POST, handleClear);
    s_server.on("/api/mode",       HTTP_POST, handleMode);
    s_server.on("/api/sniff",      HTTP_POST, handleSniff);
    s_server.on("/api/deauth",     HTTP_POST, handleDeauth);

    // Unknown paths (incl. OS captive-portal probes like /generate_204,
    // /hotspot-detect.html) redirect to the dashboard so the portal triggers.
    s_server.onNotFound([](AsyncWebServerRequest* req){
        req->redirect("http://192.168.4.1/");
    });

    s_server.begin();
    _started = true;
}

void WebInterface::loop() {
    if (_started) s_dns.processNextRequest();
}

String WebInterface::apIp() { return WiFi.softAPIP().toString(); }

// ============================================================================
//  blue_routes.cpp
//
//  All HTTP glue for the BlueDriver link lives here so it can be dropped into
//  the WarDriver project with a single #include + single call (see header).
//
//  Endpoints
//  ---------
//   POST /ingest              (no auth -- only reachable on the SoftAP; the
//                             headless BlueDriver cannot do HTTP basic-auth)
//       Body  : BlueDriver's JSON envelope (status + device batch + optional
//               gattResult).  See docs/wardriver-ingest.md.
//       Reply : {"ok":1,"added":N,"epoch":E,"commands":[...]}
//
//   GET  /api/blue/status     -> BlueLink::writeStatusJson()
//   POST /api/blue/scan       on=1|0|toggle
//   POST /api/blue/clear
//   POST /api/blue/config     activeScan=1|0
//   POST /api/blue/beacon     action=start|stop  type= name= arg1= arg2=
//   POST /api/blue/gatt       mac=  type=0|1   (address type)
//
//  The control endpoints just enqueue a command; it rides back to BlueDriver on
//  the response to its next /ingest POST (so command latency is up to one
//  upload interval, ~4 s by default).  Each returns {"queued":1,"id":N}.
// ============================================================================
#include "blue_routes.h"
#include "BlueLink.h"

// ---------------------------------------------------------------------------
//  OPTIONAL AUTH HOOK
//  These control endpoints are only reachable by clients already associated
//  with the WarDriver SoftAP. If you protect the rest of your UI with HTTP
//  basic-auth and want the same gate here, wire it in below, e.g.:
//      return req->authenticate(WEB_USER, WEB_PASS);
//  Returning true (the default) leaves them open on the AP.
// ---------------------------------------------------------------------------
static bool blueAuthOK(AsyncWebServerRequest* /*req*/) {
  return true;
}

// ---- helpers ---------------------------------------------------------------
static void sendJson(AsyncWebServerRequest* req, int code, const String& body) {
  AsyncWebServerResponse* r = req->beginResponse(code, "application/json", body);
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

// Read a POST (urlencoded) parameter, or "" if absent. ESPAsyncWebServer parses
// urlencoded bodies automatically, so getParam(name, /*post=*/true) works.
static String P(AsyncWebServerRequest* req, const char* name) {
  if (req->hasParam(name, true)) return req->getParam(name, true)->value();
  return String();
}

// Escape a user-supplied string for safe inclusion as a JSON *value*.
static String jstr(const String& s) {
  String o; o.reserve(s.length() + 2);
  o += '"';
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n";  break;
      case '\r': o += "\\r";  break;
      case '\t': o += "\\t";  break;
      default:
        if ((uint8_t)c >= 0x20) o += c;   // drop other control chars
    }
  }
  o += '"';
  return o;
}

static void replyQueued(AsyncWebServerRequest* req, uint32_t id) {
  sendJson(req, 200, String("{\"queued\":1,\"id\":") + id + "}");
}

// ---- /ingest body accumulation (manual; body is JSON, not urlencoded) ------
// Refuse absurd payloads so a misbehaving/hostile client on the AP can't make
// us allocate unbounded memory. An over-limit body is left truncated, so the
// JSON parse fails and handleIngest() returns a clean error.
static const size_t INGEST_MAX_BYTES = 96 * 1024;

static void onIngestBody(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                         size_t index, size_t total) {
  if (index == 0) {
    if (req->_tempObject) { delete reinterpret_cast<String*>(req->_tempObject); }
    String* buf = new String();
    buf->reserve((total < INGEST_MAX_BYTES ? total : INGEST_MAX_BYTES) + 1);
    req->_tempObject = buf;
  }
  if (req->_tempObject) {
    String* buf = reinterpret_cast<String*>(req->_tempObject);
    if (buf->length() + len <= INGEST_MAX_BYTES)
      buf->concat((const char*)data, len);
  }
}

static void onIngestDone(AsyncWebServerRequest* req) {
  String out;
  if (req->_tempObject) {
    String* buf = reinterpret_cast<String*>(req->_tempObject);
    g_blue.handleIngest(*buf, out);
    delete buf;
    req->_tempObject = nullptr;
  } else {
    // No body (or a zero-length POST): still produce a valid envelope.
    g_blue.handleIngest(String("{}"), out);
  }
  sendJson(req, 200, out);
}

// ---------------------------------------------------------------------------
void registerBlueRoutes(AsyncWebServer& server) {
  g_blue.begin();

  // ---- data + command channel (BlueDriver -> WarDriver) --------------------
  // Signature: on(uri, method, onRequest, onUpload, onBody)
  server.on("/ingest", HTTP_POST, onIngestDone, nullptr, onIngestBody);

  // ---- control plane (web UI -> WarDriver -> queued for BlueDriver) --------
  server.on("/api/blue/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!blueAuthOK(req)) { req->requestAuthentication(); return; }
    String out; g_blue.writeStatusJson(out);
    sendJson(req, 200, out);
  });

  server.on("/api/blue/scan", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!blueAuthOK(req)) { req->requestAuthentication(); return; }
    String on = P(req, "on");
    String inner;
    if (on.length() == 0 || on == "toggle") {
      inner = "\"cmd\":\"scan\"";                    // omit "on" => toggle
    } else {
      bool en = (on == "1" || on == "true" || on == "on");
      inner = String("\"cmd\":\"scan\",\"on\":") + (en ? "1" : "0");
    }
    replyQueued(req, g_blue.enqueue(inner));
  });

  server.on("/api/blue/clear", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!blueAuthOK(req)) { req->requestAuthentication(); return; }
    replyQueued(req, g_blue.enqueue("\"cmd\":\"clear\""));
  });

  server.on("/api/blue/config", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!blueAuthOK(req)) { req->requestAuthentication(); return; }
    String a = P(req, "activeScan");
    bool en = (a == "1" || a == "true" || a == "on");
    replyQueued(req, g_blue.enqueue(String("\"cmd\":\"config\",\"activeScan\":") + (en ? "1" : "0")));
  });

  server.on("/api/blue/beacon", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!blueAuthOK(req)) { req->requestAuthentication(); return; }
    String action = P(req, "action");
    String inner;
    if (action == "stop") {
      inner = "\"cmd\":\"beacon\",\"action\":\"stop\"";
    } else {
      // start: BlueDriver treats any beacon command without action=="stop" as a
      // start, using type/name/arg1/arg2 (see pentest.startBeacon()).
      inner  = "\"cmd\":\"beacon\"";
      inner += ",\"type\":" + jstr(P(req, "type"));
      inner += ",\"name\":" + jstr(P(req, "name"));
      inner += ",\"arg1\":" + jstr(P(req, "arg1"));
      inner += ",\"arg2\":" + jstr(P(req, "arg2"));
    }
    replyQueued(req, g_blue.enqueue(inner));
  });

  server.on("/api/blue/gatt", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!blueAuthOK(req)) { req->requestAuthentication(); return; }
    String mac = P(req, "mac");
    String t   = P(req, "type");                     // address type 0|1
    uint8_t at = (t == "1") ? 1 : 0;
    String inner = String("\"cmd\":\"gatt\",\"mac\":") + jstr(mac) +
                   ",\"addrType\":" + at;
    replyQueued(req, g_blue.enqueue(inner));
  });
}

// ============================================================================
//  BlueLink.cpp
// ============================================================================
#include "BlueLink.h"
#include <ArduinoJson.h>
#include <esp_system.h>     // esp_random()
#include "types.h"
#include "NetworkStore.h"

BlueLink g_blue;

void BlueLink::begin() {
  if (!mtx_) mtx_ = xSemaphoreCreateMutex();
  queue_.reserve(16);
  // Non-zero boot nonce. If WE reboot, BlueDriver sees a new epoch and resets
  // its executed-command-id counter, so freshly-issued commands (ids 1,2,3…)
  // are honoured instead of being mistaken for already-run ones.
  epoch_ = esp_random();
  if (epoch_ == 0) epoch_ = 1;
}

uint32_t BlueLink::enqueue(const String& innerFields) {
  lock();
  uint32_t id = nextId_++;
  BlueCommand c;
  c.id   = id;
  c.json = String("{\"id\":") + id + "," + innerFields + "}";
  queue_.push_back(c);
  // Bound the queue so an absent BlueDriver can't make it grow without limit.
  if (queue_.size() > 32) queue_.erase(queue_.begin());
  unlock();
  return id;
}

// Minimal JSON-string escaper for our own short status strings.
static void escTo(String& out, const String& s) {
  out += '"';
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n')        { out += "\\n"; }
    else if ((uint8_t)c >= 0x20) out += c;
  }
  out += '"';
}

void BlueLink::handleIngest(const String& body, String& out) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) { out = "{\"error\":\"bad json\"}"; return; }

  uint32_t postedEpoch = doc["linkEpoch"] | 0;
  uint32_t ackId       = doc["lastCmdId"] | 0;

  // ---- status mirror + command pruning (under our lock) --------------------
  lock();
  st_.lastSeenMs = millis();

  JsonObjectConst s = doc["status"].as<JsonObjectConst>();
  if (!s.isNull()) {
    st_.scanning     = ((int)(s["scanning"]     | 0)) != 0;
    st_.devices      =        s["devices"]       | 0;
    st_.newDevices   =        s["newDevices"]    | 0;
    st_.uptime       =        s["uptime"]        | 0;
    st_.heap         =        s["heap"]          | 0;
    st_.psram        =        s["psram"]         | 0;
    st_.activeScan   = ((int)(s["activeScan"]    | 1)) != 0;
    st_.beaconActive = ((int)(s["beaconActive"]  | 0)) != 0;
    st_.beaconInfo   = String((const char*)(s["beaconInfo"] | ""));
  }
  JsonObjectConst g = doc["gps"].as<JsonObjectConst>();
  if (!g.isNull()) {
    st_.gpsValid = ((int)(g["valid"] | 0)) != 0;
    st_.gpsSats  =        g["sats"]  | 0;
    st_.gpsLat   =        g["lat"]   | 0.0;
    st_.gpsLon   =        g["lon"]   | 0.0;
  }
  if (!doc["gattResult"].isNull()) {
    String gj; serializeJson(doc["gattResult"], gj);
    lastGatt_ = gj;
  }
  // Only honour the device's command-ack if it belongs to OUR current epoch.
  if (postedEpoch == epoch_ && ackId) {
    for (size_t i = 0; i < queue_.size(); ) {
      if (queue_[i].id <= ackId) queue_.erase(queue_.begin() + i);
      else ++i;
    }
  }
  unlock();

  // ---- ingest devices into the shared store (store has its own lock) -------
  uint32_t added = 0;
  JsonArrayConst devs = doc["devices"].as<JsonArrayConst>();
  for (JsonObjectConst d : devs) {
    const char* macs = d["mac"] | "";
    uint8_t mac[6];
    if (!strToMac(String(macs), mac)) continue;

    String   name   = String((const char*)(d["name"]   | ""));
    String   vendor = String((const char*)(d["vendor"] | ""));
    int8_t   rssi   = (int8_t)(int)(d["rssiBest"] | (int)(d["rssi"] | 0));
    uint16_t hits   = (uint16_t)(uint32_t)(d["count"] | 1);
    bool     hasGps = ((int)(d["gps"] | 0)) != 0;
    double   lat    = d["lat"]   | 0.0;
    double   lon    = d["lon"]   | 0.0;
    uint32_t first  = d["first"] | 0;
    // BlueDriver's "first" is a GPS epoch only when it had a time fix; treat a
    // plausible epoch as UTC, otherwise leave it blank (it was an uptime value).
    time_t   firstUtc = (first > 1000000000UL) ? (time_t)first : 0;

    if (g_store.observeBle(mac, name, vendor, rssi, hits, firstUtc, hasGps, lat, lon))
      added++;
  }

  // ---- build response: ack + epoch + queued commands -----------------------
  lock();
  st_.totalIngested += added;
  String cmds = "[";
  for (size_t i = 0; i < queue_.size(); i++) {
    if (i) cmds += ",";
    cmds += queue_[i].json;
  }
  cmds += "]";
  uint32_t ep = epoch_;
  unlock();

  out  = "{\"ok\":1,\"added\":" + String(added);
  out += ",\"epoch\":" + String(ep);
  out += ",\"commands\":" + cmds + "}";
}

void BlueLink::writeStatusJson(String& out) {
  lock();
  uint32_t now = millis();
  long age = st_.lastSeenMs ? (long)(now - st_.lastSeenMs) : -1;
  bool online = (st_.lastSeenMs != 0) && (age >= 0) && (age < 15000);

  out  = "{\"online\":" + String(online ? 1 : 0);
  out += ",\"ageMs\":" + String(age);
  out += ",\"scanning\":" + String(st_.scanning ? 1 : 0);
  out += ",\"devices\":" + String(st_.devices);
  out += ",\"newDevices\":" + String(st_.newDevices);
  out += ",\"uptime\":" + String(st_.uptime);
  out += ",\"heap\":" + String(st_.heap);
  out += ",\"psram\":" + String(st_.psram);
  out += ",\"activeScan\":" + String(st_.activeScan ? 1 : 0);
  out += ",\"beaconActive\":" + String(st_.beaconActive ? 1 : 0);
  out += ",\"beaconInfo\":"; escTo(out, st_.beaconInfo);
  out += ",\"ingested\":" + String(st_.totalIngested);
  out += ",\"queued\":" + String((uint32_t)queue_.size());
  out += ",\"gps\":{\"valid\":" + String(st_.gpsValid ? 1 : 0);
  out += ",\"sats\":" + String(st_.gpsSats);
  out += ",\"lat\":" + String(st_.gpsLat, 6);
  out += ",\"lon\":" + String(st_.gpsLon, 6) + "}";
  out += ",\"gatt\":" + (lastGatt_.length() ? lastGatt_ : String("null"));
  out += "}";
  unlock();
}

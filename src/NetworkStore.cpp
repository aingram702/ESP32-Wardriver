#include "NetworkStore.h"
#include "FlockDetector.h"
#include "config.h"
#include <Print.h>

// Replace tab/newline so a value can live safely in one TSV field.
static String tsvClean(const String& in) {
    String s = in;
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '\t' || c == '\n' || c == '\r') s.setCharAt(i, ' ');
    }
    return s;
}

NetworkStore g_store;

void NetworkStore::begin() {
    if (!_mtx) _mtx = xSemaphoreCreateRecursiveMutex();
    _devices.reserve(512);
}

uint64_t NetworkStore::key(const uint8_t* mac) {
    uint64_t k = 0;
    for (int i = 0; i < 6; i++) k = (k << 8) | mac[i];
    return k;
}

bool NetworkStore::observe(const uint8_t* mac, const String& name, NetType type,
                           int8_t rssi, uint8_t channel, uint8_t encryption,
                           const GpsFix& gps) {
    lock();
    bool isNew = false;
    uint64_t k = key(mac);
    auto it = _index.find(k);

    if (it == _index.end()) {
        // New device - respect the hard cap to bound memory use.
        if (_devices.size() >= MAX_DEVICES) { unlock(); return false; }

        Device d;
        memcpy(d.mac, mac, 6);
        d.ssid        = name;
        d.type        = type;
        d.bestRssi    = rssi;
        d.lastRssi    = rssi;
        d.channel     = channel;
        d.encryption  = encryption;
        d.firstSeenMs = millis();
        d.lastSeenMs  = d.firstSeenMs;
        d.firstSeenUtc = gps.valid ? gps.utc : 0;
        d.hitCount    = 1;
        if (gps.valid) { d.hasGps = true; d.lat = gps.lat; d.lon = gps.lon; }

        // Threat / vendor classification.
        String label, vendor;
        if (FlockDetector::classify(mac, name, label, vendor)) {
            d.dangerous = true; d.threatLabel = label; d.vendor = vendor;
        } else {
            d.vendor = FlockDetector::vendorHint(mac);
        }

        _index[k] = _devices.size();
        _devices.push_back(d);
        isNew = true;
    } else {
        // Known device - update in place.
        Device& d = _devices[it->second];
        d.lastSeenMs = millis();
        d.lastRssi   = rssi;
        d.hitCount++;
        if (channel) d.channel = channel;
        if (name.length() && d.ssid.length() == 0) d.ssid = name;  // fill blanks
        // Keep the location where the signal was strongest.
        if (rssi > d.bestRssi) {
            d.bestRssi = rssi;
            if (gps.valid) { d.hasGps = true; d.lat = gps.lat; d.lon = gps.lon; }
        }
        // Late name match may reveal a threat we missed when the name was empty.
        if (!d.dangerous && name.length()) {
            String label, vendor;
            if (FlockDetector::classify(d.mac, name, label, vendor)) {
                d.dangerous = true; d.threatLabel = label; d.vendor = vendor;
            }
        }
    }
    unlock();
    return isNew;
}

size_t NetworkStore::count() {
    lock(); size_t n = _devices.size(); unlock(); return n;
}

size_t NetworkStore::dangerousCount() {
    lock();
    size_t n = 0;
    for (auto& d : _devices) if (d.dangerous) n++;
    unlock();
    return n;
}

void NetworkStore::typeCounts(size_t& wifi, size_t& ble) {
    lock();
    wifi = 0; ble = 0;
    for (auto& d : _devices) (d.type == NET_WIFI ? wifi : ble)++;
    unlock();
}

void NetworkStore::clear() {
    lock(); _devices.clear(); _index.clear(); unlock();
}

// --- JSON helpers -----------------------------------------------------------
// SSIDs and names are attacker-controlled, so every string is escaped before
// being written into JSON. The web UI additionally renders via textContent.
static void writeJsonString(Print& out, const String& s) {
    out.print('"');
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
            case '"':  out.print("\\\""); break;
            case '\\': out.print("\\\\"); break;
            case '\n': out.print("\\n");  break;
            case '\r': out.print("\\r");  break;
            case '\t': out.print("\\t");  break;
            default:
                if ((uint8_t)c < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
                    out.print(buf);
                } else {
                    out.print(c);
                }
        }
    }
    out.print('"');
}

static const char* authName(uint8_t mode) {
    switch (mode) {
        case 0: return "OPEN";
        case 1: return "WEP";
        case 2: return "WPA-PSK";
        case 3: return "WPA2-PSK";
        case 4: return "WPA/WPA2-PSK";
        case 5: return "WPA2-ENT";
        case 6: return "WPA3-PSK";
        case 7: return "WPA2/WPA3-PSK";
        default: return "UNKNOWN";
    }
}

void NetworkStore::streamJson(Print& out) {
    lock();
    char macStr[18];
    out.print('[');
    for (size_t i = 0; i < _devices.size(); i++) {
        Device& d = _devices[i];
        if (i) out.print(',');
        macToStr(d.mac, macStr);
        out.print("{\"mac\":\""); out.print(macStr);
        out.print("\",\"ssid\":");  writeJsonString(out, d.ssid);
        out.print(",\"type\":\"");  out.print(d.type == NET_WIFI ? "WiFi" : "BLE");
        out.print("\",\"rssi\":");  out.print(d.bestRssi);
        out.print(",\"lastRssi\":"); out.print(d.lastRssi);
        out.print(",\"channel\":"); out.print(d.channel);
        out.print(",\"enc\":\"");   out.print(d.type == NET_WIFI ? authName(d.encryption) : "");
        out.print("\",\"vendor\":"); writeJsonString(out, d.vendor);
        out.print(",\"dangerous\":"); out.print(d.dangerous ? "true" : "false");
        out.print(",\"threat\":");  writeJsonString(out, d.threatLabel);
        out.print(",\"hits\":");    out.print(d.hitCount);
        out.print(",\"first\":");   out.print((uint32_t)d.firstSeenMs);
        out.print(",\"last\":");    out.print((uint32_t)d.lastSeenMs);
        out.print(",\"utc\":");     out.print((uint32_t)d.firstSeenUtc);
        out.print(",\"hasGps\":");  out.print(d.hasGps ? "true" : "false");
        out.print(",\"lat\":");     out.print(d.lat, 6);
        out.print(",\"lon\":");     out.print(d.lon, 6);
        out.print('}');
    }
    out.print(']');
    unlock();
}

// --- CSV --------------------------------------------------------------------
// Guards against CSV-injection: any field beginning with = + - @ (or a control
// char) is prefixed with a single quote, and quotes/commas are escaped.
static void writeCsvField(Print& out, const String& in) {
    String s = in;
    bool needQuote = false;
    if (s.length()) {
        char c0 = s[0];
        if (c0 == '=' || c0 == '+' || c0 == '-' || c0 == '@') {
            s = "'" + s;        // neutralise spreadsheet formula injection
        }
    }
    if (s.indexOf(',') >= 0 || s.indexOf('"') >= 0 ||
        s.indexOf('\n') >= 0 || s.indexOf('\r') >= 0) {
        needQuote = true;
    }
    if (needQuote) {
        out.print('"');
        for (size_t i = 0; i < s.length(); i++) {
            char c = s[i];
            if (c == '"') out.print("\"\"");
            else if (c == '\r' || c == '\n') out.print(' ');
            else out.print(c);
        }
        out.print('"');
    } else {
        out.print(s);
    }
}

void NetworkStore::streamCsv(Print& out) {
    lock();
    out.print("SSID,MAC,SignalStrength(dBm),Type,Channel,Encryption,"
              "Vendor,Dangerous,Threat,Hits,FirstSeenUTC,Latitude,Longitude\r\n");
    char macStr[18];
    for (auto& d : _devices) {
        macToStr(d.mac, macStr);
        writeCsvField(out, d.ssid);                 out.print(',');
        out.print(macStr);                          out.print(',');
        out.print(d.bestRssi);                      out.print(',');
        out.print(d.type == NET_WIFI ? "WiFi" : "BLE"); out.print(',');
        out.print(d.channel);                       out.print(',');
        writeCsvField(out, d.type == NET_WIFI ? String(authName(d.encryption)) : String("")); out.print(',');
        writeCsvField(out, d.vendor);               out.print(',');
        out.print(d.dangerous ? "YES" : "no");      out.print(',');
        writeCsvField(out, d.threatLabel);          out.print(',');
        out.print(d.hitCount);                      out.print(',');

        // ISO-8601 UTC if we had a GPS time fix, else blank.
        if (d.firstSeenUtc) {
            struct tm t;
            time_t ts = d.firstSeenUtc;
            gmtime_r(&ts, &t);
            char tbuf[25];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", &t);
            out.print(tbuf);
        }
        out.print(',');
        if (d.hasGps) { out.print(d.lat, 6); } out.print(',');
        if (d.hasGps) { out.print(d.lon, 6); } out.print("\r\n");
    }
    unlock();
}

// --- Persistence (tab-delimited, one device per line) -----------------------
// Field order: mac,type,bestRssi,channel,enc,dangerous,hits,firstUtc,lat,lon,
//              hasGps,vendor,threat,ssid
bool NetworkStore::save(fs::FS& fsys, const char* path) {
    File f = fsys.open(path, "w");
    if (!f) return false;
    lock();
    char macStr[18];
    for (auto& d : _devices) {
        macToStr(d.mac, macStr);
        f.printf("%s\t%u\t%d\t%u\t%u\t%u\t%u\t%ld\t%.6f\t%.6f\t%u\t%s\t%s\t%s\n",
                 macStr, (unsigned)d.type, d.bestRssi, d.channel, d.encryption,
                 d.dangerous ? 1u : 0u, d.hitCount, (long)d.firstSeenUtc,
                 d.lat, d.lon, d.hasGps ? 1u : 0u,
                 tsvClean(d.vendor).c_str(), tsvClean(d.threatLabel).c_str(),
                 tsvClean(d.ssid).c_str());
    }
    unlock();
    f.close();
    return true;
}

bool NetworkStore::load(fs::FS& fsys, const char* path) {
    File f = fsys.open(path, "r");
    if (!f) return false;
    lock();
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.length() < 17) continue;

        // Split into exactly 14 tab fields (ssid is the remainder).
        String fld[14];
        int idx = 0, start = 0;
        for (int i = 0; i < (int)line.length() && idx < 13; i++) {
            if (line[i] == '\t') { fld[idx++] = line.substring(start, i); start = i + 1; }
        }
        fld[idx++] = line.substring(start);
        if (idx < 14) continue;

        uint8_t mac[6];
        if (!strToMac(fld[0], mac)) continue;
        if (_devices.size() >= MAX_DEVICES) break;

        Device d;
        memcpy(d.mac, mac, 6);
        d.type        = (NetType)fld[1].toInt();
        d.bestRssi    = (int8_t)fld[2].toInt();
        d.lastRssi    = d.bestRssi;
        d.channel     = (uint8_t)fld[3].toInt();
        d.encryption  = (uint8_t)fld[4].toInt();
        d.dangerous   = fld[5].toInt() != 0;
        d.hitCount    = (uint16_t)fld[6].toInt();
        d.firstSeenUtc= (time_t)fld[7].toInt();
        d.lat         = fld[8].toFloat();
        d.lon         = fld[9].toFloat();
        d.hasGps      = fld[10].toInt() != 0;
        d.vendor      = fld[11];
        d.threatLabel = fld[12];
        d.ssid        = fld[13];
        d.firstSeenMs = millis();
        d.lastSeenMs  = d.firstSeenMs;

        _index[key(mac)] = _devices.size();
        _devices.push_back(d);
    }
    unlock();
    f.close();
    return true;
}

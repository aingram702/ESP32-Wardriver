// ============================================================================
//  PacketSniffer  -  802.11 promiscuous capture, frame counting and targeted
//  deauthentication. Active only in ATTACK mode (it monopolises the radio's
//  monitor capability, which is why it is kept separate from wardrive mode).
//
//  LEGAL / ETHICAL NOTE: transmitting deauthentication frames against networks
//  or clients you do not own or lack written authorisation to test is illegal
//  in most jurisdictions (e.g. US CFAA, UK Computer Misuse Act). This exists
//  for authorised security testing, lab use and education ONLY.
// ============================================================================
#pragma once
#include "types.h"

class PacketSniffer {
public:
    void begin();                       // one-time setup
    void start(uint8_t channel);        // enter promiscuous mode on a channel
    void stop();                        // leave promiscuous mode
    bool active() const { return _active; }

    void     setChannel(uint8_t ch);
    uint8_t  channel() const { return _channel; }

    PacketStats stats();
    void        resetStats();

    // Copy up to PACKET_LOG_DEPTH most-recent frames into `out` (newest last).
    // Returns the number copied.
    size_t recent(PacketRecord* out, size_t maxOut);

    // Send `bursts` rounds of deauth frames. `apMac` is the BSSID to spoof as
    // source; `clientMac` is the victim (nullptr/zeros => broadcast = all
    // clients of that AP). Returns frames transmitted.
    uint32_t deauth(const uint8_t* apMac, const uint8_t* clientMac, uint16_t bursts);

private:
    bool    _active  = false;
    uint8_t _channel = 1;
};

extern PacketSniffer g_sniffer;

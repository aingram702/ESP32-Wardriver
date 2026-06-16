# ESP32-S3 Wardriver

A WiFi + BLE recon platform for the **ESP32-S3-DevKitC-1 (N16R8)**. It scans for
nearby access points and Bluetooth devices, de-duplicates them by MAC, tags each
with signal strength / encryption / a timestamp (and GPS location when a module
is attached), flags likely **Flock Safety surveillance cameras** as threats, and
serves everything through a hacker-themed web interface hosted from the device's
own WiFi access point. It also includes an 802.11 packet monitor and a targeted
deauthentication tool for **authorised** security testing.

```
        ((o))      ESP32-S3 // RF RECON UNIT
       /  |  \     WiFi + BLE  •  GPS-tagged  •  CSV export
      WiFi BLE GPS  AP-hosted control plane
```

---

## ⚖️ Legal & ethical use — read first

This project can **transmit deauthentication frames** and **passively capture
802.11 traffic**. Doing either against networks or devices you do not own, or do
not have **explicit written authorisation** to test, is illegal in most
jurisdictions (e.g. the US Computer Fraud and Abuse Act, the UK Computer Misuse
Act, and equivalents worldwide).

Passive *wardriving* (logging that networks exist) is generally legal; **active
attacks are not**. Use the attack features only on your own lab equipment or
under a signed engagement. You are responsible for how you use this.

---

## Features

| Feature | Status |
|---|---|
| WiFi AP scanning (SSID, BSSID, RSSI, channel, encryption) | ✅ |
| BLE device scanning (name, MAC, RSSI) | ✅ |
| De-duplication by MAC (strongest-RSSI sighting kept) | ✅ |
| CSV export (SSID, MAC, signal, type, timestamp, GPS, …) | ✅ |
| Web UI with color-coded WiFi / BLE / threat rows | ✅ |
| Flock camera detection → flagged **DANGEROUS** (red) | ✅ (heuristic, see notes) |
| Packet monitor with per-type counters | ✅ |
| Targeted / broadcast deauthentication | ✅ (attack mode) |
| GPS location logging (ATGM336H / NEO-M8N / NEO-6M) | ✅ (hooks ready, plug & play) |
| Capture persistence across reboot (LittleFS) | ✅ |
| Single-binary flash (UI embedded, no filesystem upload) | ✅ |

---

## Hardware

- **Board:** ESP32-S3-DevKitC-1, **N16R8** variant (16 MB flash, 8 MB OPI PSRAM).
- **GPS (optional, add later):** ATGM336H GPS+BDS module (NEO-M8N/NEO-6M
  replacement), NMEA over UART @ 9600 baud.

### GPS wiring (when you add the module)

| ATGM336H pin | ESP32-S3 pin | Notes |
|---|---|---|
| VCC | 3V3 | 3.3 V |
| GND | GND | |
| TX  | GPIO **18** | module TX → ESP RX (`GPS_RX_PIN`) |
| RX  | GPIO **17** | module RX ← ESP TX (`GPS_TX_PIN`, often unused) |

Pins are configurable in [`src/config.h`](src/config.h). The firmware runs fine
**without** GPS and starts logging coordinates automatically the moment a module
is detected.

---

## Quick start (PlatformIO — recommended)

1. Install [PlatformIO](https://platformio.org/install) (VS Code extension or
   `pip install platformio`).
2. Clone this repo and open the folder.
3. Connect the board's **UART** port via USB.
4. Build & flash (libraries download automatically on first build):

   ```bash
   pio run -t upload
   pio device monitor        # optional: watch the boot log @115200
   ```

That's it — the web UI is embedded in the firmware, so there is **no separate
filesystem upload step**.

### Arduino IDE (alternative)

Prefer PlatformIO, but if you must use the Arduino IDE:

1. Install the **esp32 by Espressif** board package (Boards Manager).
2. Install libraries: *ESPAsyncWebServer* + *AsyncTCP* (the
   [ESP32Async](https://github.com/ESP32Async) forks), *NimBLE-Arduino*,
   *TinyGPSPlus*.
3. Select **ESP32S3 Dev Module**, set Flash 16 MB, **PSRAM: OPI PSRAM**,
   Partition Scheme with ≥4 MB app.
4. Move the `src/*` files into a sketch folder (rename `main.cpp` → the sketch
   `.ino`) and upload.

---

## First-time setup

1. After flashing, the device hosts a WiFi network:
   - **SSID:** `Wardriver`
   - **Password:** `wardrive-me`  ← **change this** in `src/config.h`
2. Connect your phone/laptop to it.
3. Open **http://192.168.4.1** in a browser.

### Security hardening (do this)

Defaults are set up to be safe-ish, but before real use, in `src/config.h`:

- **Change `AP_PASSWORD`** (WPA2, 8–63 chars). The default lets anyone nearby
  control the radio.
- Optionally enable HTTP login: set `WEB_AUTH_ENABLED true` and a strong
  `WEB_AUTH_PASS`.

---

## Using the web interface

- **WARDRIVE / ATTACK** toggle (top right) switches operating mode. They are
  mutually exclusive because monitor mode and active scanning contend for the
  radio.
- **DISCOVERED tab** — live, de-duplicated device list. Filter/sort, color key:
  - <span>🟢</span> **WiFi**  •  <span>🔵</span> **BLE**  •  <span>🔴</span> **THREAT** (e.g. Flock camera, blinking)
  - **EXPORT CSV** downloads everything; **CLEAR** wipes the store.
- **PACKET MONITOR tab** (ATTACK mode) — per-type frame counters
  (mgmt/data/ctrl/beacon/probe/deauth/EAPOL), a live recent-frames table, a
  channel selector, and the deauth tool.
- **SYSTEM tab** — GPS state, AP details, and notes.

### CSV columns

`SSID, MAC, SignalStrength(dBm), Type, Channel, Encryption, Vendor, Dangerous,
Threat, Hits, FirstSeenUTC, Latitude, Longitude`

Timestamps are ISO-8601 UTC when a GPS time fix is available. Fields are guarded
against CSV/formula injection.

---

## Flock camera detection — how it works (and its limits)

Detection lives in [`src/flock_data.h`](src/flock_data.h) /
[`src/FlockDetector.cpp`](src/FlockDetector.cpp) and uses two signals:

1. **MAC OUI prefixes** — highest confidence, but you must populate the list
   with prefixes you have **verified** in the field (the shipped list is an
   empty framework, because publishing unverified OUIs causes false alarms).
2. **SSID / BLE-name heuristics** — matches tokens like `flock`, `falcon`,
   `penguin` seen during install/maintenance modes. These are flagged as
   `FLOCK CAMERA?` (the `?` denotes a heuristic, not an OUI-confirmed hit).

**Honest caveat:** Flock ALPR cameras are mostly solar + LTE devices and don't
reliably broadcast WiFi during normal operation, so purely passive RF detection
is **not guaranteed**. Treat flags as leads to verify, and contribute confirmed
signatures back into `flock_data.h`. See the community
[DeFlock](https://deflock.me) project for crowd-sourced camera locations.

---

## Troubleshooting: unstable AP / can't load the page

In `WIFI_AP_STA` mode the AP and the scanner share one radio, so a naive full
channel sweep drags the AP off its channel for >1 s and clients drop. This
firmware avoids that:

- **Modem power-save is disabled** (`WiFi.setSleep(false)`) so the AP stays
  responsive — the single biggest fix for a hanging web UI.
- **Scans hop one channel per cycle** (passive), keeping the AP on its home
  channel ~90% of the time instead of blacking out during a full sweep.
- **A captive-portal DNS server** answers all lookups with `192.168.4.1`, so
  phones pop the sign-in page and the UI loads without typing the IP.
- **BLE scans are staggered** behind WiFi scans so the radios don't contend.

If you still see drops: keep `AP_CHANNEL` (default 1) on a quiet channel, and if
you want maximum AP stability over scan coverage, raise `WIFI_SCAN_INTERVAL_MS`
in `src/config.h`. On phones, choose "stay connected / use without internet" if
prompted, so the handset doesn't fall back to mobile data.

## How modes work (radio constraints)

The ESP32-S3 cannot cleanly run WiFi promiscuous/monitor mode *and* normal
AP + active scanning simultaneously. So:

- **WARDRIVE:** SoftAP + WiFi scanning + BLE scanning + GPS + full UI.
- **ATTACK:** BLE controller released; promiscuous capture / deauth on a chosen
  channel. If the sniff channel differs from the AP channel, your browser may
  briefly disconnect while the radio is on the other channel — reconnect and the
  captured data is waiting.

---

## Security notes (firmware itself)

- WPA2-protected AP by default; optional HTTP basic-auth.
- All web inputs validated (MAC format, channel range, burst clamp).
- Attacker-controlled SSIDs/names are JSON-escaped server-side and rendered with
  `textContent` (never `innerHTML`) to prevent stored XSS in the dashboard.
- CSV/TSV outputs are injection-guarded.
- Shared device store is mutex-protected; packet counters use a critical
  section. Device count is hard-capped (`MAX_DEVICES`) to bound memory.

---

## Project layout

```
platformio.ini        Board + library config (N16R8: 16 MB flash, 8 MB PSRAM)
partitions.csv        16 MB partition table
src/
  config.h            Pins, AP creds, tunables  ← edit me
  types.h             Device model + helpers
  flock_data.h        Flock OUI / name signatures (update with field data)
  FlockDetector.*     Threat classification
  NetworkStore.*      De-dup store + JSON/CSV/persistence
  GpsModule.*         NMEA GPS (optional hardware)
  WifiScanner.*       Non-blocking WiFi scanning
  BleScanner.*        NimBLE scanning
  PacketSniffer.*     Promiscuous capture, counters, deauth
  WebInterface.*      Async HTTP UI + JSON/CSV API
  web_assets.h        Embedded single-page UI
  Modes.h             Operating-mode contract
  main.cpp            Orchestration + mode manager
```

---

## Roadmap ideas

- Reconstruct AP↔client associations to enable smarter targeted deauth.
- WiGLE-compatible CSV export.
- On-board OLED status display.
- Channel-hopping capture with a ring buffer of full frames (PCAP export).

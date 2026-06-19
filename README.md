# ESP32-S3 WarDriver &mdash; WiFi + BLE Recon & Rogue-AP Detector

A self-contained wireless survey appliance for the **ESP32-S3-DevKitC-1 N16R8**
(16&nbsp;MB flash / 8&nbsp;MB PSRAM). It continuously enumerates nearby **WiFi
access points** and **Bluetooth LE devices**, logs full details to on-board
flash, and serves a **hacker-themed web dashboard** straight from the board's
own WiFi access point &mdash; no internet, app, cloud, or driver required on the
client side.

It was built for a specific defensive use case: **walking your workplace to hunt
for rogue / "evil-twin" access points** that impersonate your corporate SSIDs.

> ⚠️ **Authorised use only.** Scan only networks and premises you have
> permission to assess. Passive WiFi/BLE scanning is generally lawful, but you
> are responsible for complying with local law and your employer's policies.
> This tool only *listens* &mdash; it does not transmit deauth frames, capture
> traffic, or attack anything.

---

## Table of contents

- [Features](#features)
- [Hardware](#hardware)
- [Quick start (TL;DR)](#quick-start-tldr)
- [Installation](#installation)
  - [Prerequisites](#prerequisites)
  - [Method A — PlatformIO CLI (recommended)](#method-a--platformio-cli-recommended)
  - [Method B — PlatformIO in VS Code](#method-b--platformio-in-vs-code)
  - [Method C — Arduino IDE](#method-c--arduino-ide)
  - [Putting the board into the bootloader](#putting-the-board-into-the-bootloader)
- [First use](#first-use)
- [Configuring rogue detection for your site](#configuring-rogue-detection-for-your-site)
- [Configuration reference](#configuration-reference)
- [Dashboard tour](#dashboard-tour)
- [Log files (CSV)](#log-files-csv)
- [REST / WebSocket API](#rest--websocket-api)
- [How it works](#how-it-works)
- [Troubleshooting](#troubleshooting)
- [Limitations & notes](#limitations--notes)
- [Project layout](#project-layout)
- [License](#license)

---

## Features

- **WiFi survey** &mdash; SSID, BSSID (MAC), RSSI (live + best seen), channel,
  encryption (OPEN/WEP/WPA/WPA2/WPA3/Enterprise), vendor (OUI lookup),
  hidden-SSID detection, first/last-seen and hit counts.
- **Bluetooth LE survey** &mdash; address (public/random), advertised name, RSSI,
  vendor, manufacturer company ID, appearance, and advertised service UUIDs.
  *(The ESP32-S3 radio supports Bluetooth **LE** only — not Bluetooth Classic.)*
- **Rogue-AP / evil-twin detection** &mdash; maintain a whitelist of your
  legitimate APs (SSID → trusted BSSIDs). Any device broadcasting a trusted SSID
  from an **unknown BSSID**, or a trusted SSID seen **OPEN/WEP**, raises a red
  **ALERT**. Heuristics also flag suspicious same-SSID / mismatched-encryption
  clones as **WATCH**.
- **Hacker-style web GUI** &mdash; matrix-rain background, glitch title,
  neon-green terminal aesthetic, live stat counters, sortable/filterable tables,
  signal bars, severity badges, and a full control panel. Served entirely
  offline from the device.
- **Logging** &mdash; every new observation is appended to CSV files on flash
  (`/wifi_log.csv`, `/ble_log.csv`), auto-rolled at 6&nbsp;MB, downloadable from
  the dashboard for import into a spreadsheet / SIEM.
- **Status LED** &mdash; the onboard RGB LED shows state at a glance
  (green = ready, red = WiFi scan, blue = BLE scan, amber = paused).
- **Zero client install** &mdash; works from any phone or laptop browser.

---

## Hardware

| Item | Detail |
|------|--------|
| Board | ESP32-S3-DevKitC-1 **N16R8** |
| Flash | 16 MB (QIO) |
| PSRAM | 8 MB (Octal / OPI) |
| Radios | WiFi 802.11 b/g/n + Bluetooth **LE** 5.0 |
| Power | USB-C, or any USB power bank for portable field use |

No extra wiring is required. Connect to the board's **USB port** (the firmware
uses native USB-CDC for the serial console). For walk-arounds, a small USB power
bank turns it into a pocket scanner.

> **Other ESP32-S3 boards** work too — if your board has a different flash/PSRAM
> size (e.g. N8R2) you must edit `platformio.ini` (`board_build.flash_size`,
> `memory_type`) and possibly the partition table. See
> [Configuration reference](#configuration-reference).

---

## Quick start (TL;DR)

```bash
git clone https://github.com/aingram702/esp32-s3-wardriver.git
cd esp32-s3-wardriver
pip install -U platformio          # if you don't already have PlatformIO
pio run -t upload                  # compile + flash over USB
pio device monitor                 # watch the serial log (115200 baud)
```

Then join WiFi **`WARDRIVER`** (password **`roguehunter`**) and browse to
**http://192.168.4.1/**.

---

## Installation

### Prerequisites

- A **USB-C data cable** (not a charge-only cable) between the board and your
  computer.
- One toolchain from the methods below. **PlatformIO is strongly recommended** —
  it pins the exact board settings, partition table, and library versions
  automatically, so the build "just works".
- **USB drivers:** the DevKitC-1 exposes a *native USB* port and a *UART* port
  (via a CH343/CP210x bridge, depending on revision).
  - On **Linux**, native USB works out of the box; add yourself to the
    `dialout` group (`sudo usermod -aG dialout $USER`, then log out/in) to access
    serial ports without `sudo`.
  - On **macOS**, native USB works out of the box; for the UART port install the
    [CP210x](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers)
    or [CH34x](https://www.wch-ic.com/downloads/CH341SER_MAC_ZIP.html) driver.
  - On **Windows**, install the matching CP210x/CH34x driver if the port doesn't
    enumerate. PlatformIO can also auto-install drivers.

> **Which USB port?** The DevKitC-1 has two USB-C jacks labelled **USB** (native,
> used by this firmware for the console) and **UART** (the serial bridge). Either
> can be used to flash. If flashing fails on one, try the other.

---

### Method A — PlatformIO CLI (recommended)

1. **Install PlatformIO Core** (needs Python 3.6+):

   ```bash
   pip install -U platformio
   # or, isolated:  pipx install platformio
   ```

   Verify: `pio --version`.

2. **Clone and enter the project:**

   ```bash
   git clone https://github.com/aingram702/esp32-s3-wardriver.git
   cd esp32-s3-wardriver
   ```

3. **(Optional) edit settings** in [`src/config.h`](src/config.h) — AP name /
   password, optional home-WiFi join, scan timing. See
   [Configuration reference](#configuration-reference).

4. **Build** (downloads the ESP32 toolchain + libraries on first run — this can
   take a few minutes):

   ```bash
   pio run
   ```

5. **Flash over USB.** Plug the board in, then:

   ```bash
   pio run -t upload
   ```

   PlatformIO auto-detects the port. To force one:
   `pio run -t upload --upload-port /dev/ttyACM0` (Linux),
   `... --upload-port COM5` (Windows), or `... --upload-port /dev/cu.usbmodem*`
   (macOS). If upload doesn't start, see
   [Putting the board into the bootloader](#putting-the-board-into-the-bootloader).

6. **Open the serial console** to watch it boot and print its IP:

   ```bash
   pio device monitor
   ```

   You should see the SoftAP address (`http://192.168.4.1/`) and "Scanning
   engaged."

> The web UI is embedded in the firmware (PROGMEM), so there is **no separate
> filesystem-upload step**. The LittleFS partition is used only for CSV logs and
> the whitelist, and is formatted automatically on first boot.

#### Useful CLI commands

| Command | Purpose |
|---------|---------|
| `pio run` | Compile only |
| `pio run -t upload` | Compile + flash |
| `pio device monitor` | Serial console (115200 baud) |
| `pio run -t upload -t monitor` | Flash then immediately monitor |
| `pio run -t clean` | Clean build artifacts |
| `pio run -t erase` | Full chip erase (wipes logs + whitelist) |
| `pio check` | Static analysis (cppcheck) |

---

### Method B — PlatformIO in VS Code

1. Install **[VS Code](https://code.visualstudio.com/)**.
2. In the Extensions panel, install **"PlatformIO IDE"**. Let it finish its
   first-run setup.
3. **File → Open Folder…** and select the cloned `esp32-s3-wardriver` directory.
4. Click the **PlatformIO alien icon** in the sidebar to see project tasks, or
   use the bottom toolbar:
   - **✓ (Build)** — compile.
   - **→ (Upload)** — compile + flash.
   - **🔌 (Serial Monitor)** — open the console.
5. The default environment (`esp32-s3-devkitc-1-n16r8`) is already selected via
   `platformio.ini`; no manual board configuration is needed.

---

### Method C — Arduino IDE

PlatformIO is recommended, but if you prefer the Arduino IDE:

1. Install the **ESP32 board package** (Boards Manager → "esp32" by Espressif,
   v3.x).
2. Install these libraries via **Library Manager**:
   - `ArduinoJson` (v7.x)
   - `NimBLE-Arduino` (v1.4.x)
   - `ESPAsyncWebServer` and `AsyncTCP` — install the **ESP32Async** forks
     (the most actively maintained; from their GitHub releases if not in the
     Library Manager).
3. Copy the contents of `src/` into a sketch folder and rename `main.cpp` →
   `esp32-s3-wardriver.ino` (keep the other `.h` files alongside it).
4. Select **Tools → Board → ESP32S3 Dev Module**, then set:
   - **Flash Size:** `16MB (128Mb)`
   - **PSRAM:** `OPI PSRAM`
   - **Partition Scheme:** a 16 MB scheme with a large SPIFFS/app split (or add
     `partitions_16mb.csv` as a custom partition table)
   - **USB CDC On Boot:** `Enabled`
   - **Upload Speed:** `921600`
5. Select the port and click **Upload**.

> Arduino-IDE builds are less reproducible than PlatformIO because board options
> and library versions are set by hand. If something behaves oddly, prefer
> Method A.

---

### Putting the board into the bootloader

Most uploads work automatically. If `pio run -t upload` stalls at
*"Connecting…"*:

1. Hold the **BOOT** button.
2. Tap (press and release) the **RESET/EN** button.
3. Release **BOOT**.
4. Re-run the upload. After flashing, press **RESET** once to run the firmware.

If the port never appears, try the other USB-C jack and confirm your cable
carries data.

---

## First use

1. Flash the board and power it on (USB or power bank).
2. On your phone/laptop, join the WiFi network:
   - **SSID:** `WARDRIVER`
   - **Password:** `roguehunter`
3. Browse to **http://192.168.4.1/** (some phones auto-open a captive-portal
   prompt — choose "use this network as-is" / open the browser manually).
4. Scanning starts automatically. Watch the **WIFI** and **BLUETOOTH** tabs
   populate within a few seconds.

If you set `STA_SSID` in `config.h` to join your LAN, you can instead browse to
**http://wardriver.local/** from a device on that network.

---

## Configuring rogue detection for your site

The detector knows nothing about *your* legitimate APs until you tell it. Build
the whitelist once:

1. While on-site near a **known-good** corporate AP, open the **WIFI** tab.
2. Click the **★** next to each legitimate AP row to mark its exact BSSID as
   trusted. Do this for every real AP broadcasting each SSID you want to
   protect (enterprise SSIDs are usually served by many BSSIDs — add them all).
   - Or add them by hand under the **WHITELIST** tab (`SSID` + `BSSID`).
3. From then on, any AP broadcasting one of those SSIDs from a **different
   BSSID** lights up as a red **ALERT** in the **ROGUE** tab &mdash; that's your
   evil twin. A trusted SSID appearing **OPEN/WEP** also alerts.

The whitelist persists across reboots (stored in `/whitelist.json` on flash).

> **Tip:** capture a clean baseline first. Walk the building once with no
> alerts expected, whitelist everything legitimate, *then* the alerts you see
> afterwards are the ones worth investigating.

---

## Configuration reference

Edit [`src/config.h`](src/config.h) before building. Key settings:

| Setting | Default | Meaning |
|---------|---------|---------|
| `AP_SSID` / `AP_PASSWORD` | `WARDRIVER` / `roguehunter` | The dashboard's own WiFi name & password (≥ 8 chars, or `""` for an open AP). |
| `AP_CHANNEL` | `6` | SoftAP channel. |
| `STA_SSID` / `STA_PASSWORD` | `""` | Optionally also join an existing WiFi so you can reach the dashboard over your LAN. Empty = pure Access-Point mode. |
| `MDNS_HOSTNAME` | `wardriver` | `http://<name>.local/` when on STA. |
| `BLE_SCAN_SECONDS` | `5` | BLE listen window per cycle. |
| `WIFI_SCAN_CHANNEL_DWELL_MS` | `120` | Per-channel dwell time for the WiFi sweep. |
| `CYCLE_PAUSE_MS` | `250` | Pause between full scan cycles. |
| `MAX_WIFI_APS` / `MAX_BLE_DEVS` | `300` | In-RAM table caps (oldest dropped first). |
| `LOG_TO_FLASH` | `true` | Enable/disable CSV logging. |
| `LOG_MAX_BYTES` | `6 MB` | Roll the log to `*.1` once it exceeds this. |
| `STATUS_LED_PIN` / `STATUS_LED_ON` | `48` / `true` | Onboard RGB LED. |

**Adapting to a different board:** change `board`, `board_build.flash_size`,
and `board_build.arduino.memory_type` in `platformio.ini`. For non-16 MB flash,
also adjust `partitions_16mb.csv` (the data partition must be **labelled
`spiffs`** for LittleFS to mount it).

---

## Dashboard tour

| Area | What it does |
|------|--------------|
| **Header** | Live status dot (red = scanning, green = idle between cycles, amber = paused), AP/STA IP, uptime. |
| **Stat cards** | Live counts of WiFi APs, BLE devices, rogue alerts, scan cycles, connected clients, free PSRAM. |
| **Control bar** | Start/stop scanning, toggle WiFi/BLE phases, live text filter, clear tables. |
| **WIFI tab** | Sortable table with signal bars, encryption badges, vendor, hit count, status, and a ★ to trust an AP. |
| **ROGUE tab** | Only flagged APs, with severity and a plain-English reason. |
| **BLUETOOTH tab** | BLE devices: name, address, type, RSSI, vendor, manufacturer, services. |
| **WHITELIST tab** | Add/remove trusted SSID→BSSID pairs. |
| **LOGS tab** | Download the WiFi/BLE CSV logs, or wipe them. |

Tables sort by clicking column headers; the filter box matches any field.

---

## Log files (CSV)

Stored on the LittleFS partition and downloadable from the **LOGS** tab (or
`/api/logs/wifi`, `/api/logs/ble`). Timestamps are seconds since boot (the S3 has
no real-time clock).

**`/wifi_log.csv`**

```
uptime_s,ssid,bssid,rssi,channel,encryption,vendor,hidden,severity,reason
```

**`/ble_log.csv`**

```
uptime_s,address,addr_type,name,rssi,vendor,manufacturer,appearance,services
```

By default each device is logged on its **first sighting**; flagged APs are
re-logged on every cycle they're seen so you get a timeline of rogue activity.
Files auto-roll to `*.csv.1` past `LOG_MAX_BYTES`.

---

## REST / WebSocket API

The dashboard is just a client of a small JSON API you can script against:

| Method | Path | Purpose |
|--------|------|---------|
| GET  | `/api/status` | device + scan stats |
| GET  | `/api/wifi` | current WiFi AP list |
| GET  | `/api/ble` | current BLE device list |
| GET  | `/api/rogues` | flagged APs only |
| POST | `/api/control` | `scanning` / `wifi` / `ble` = `0`/`1` |
| POST | `/api/clear` | `what=wifi\|ble\|all` |
| GET  | `/api/whitelist` | trusted SSID→BSSID map |
| POST | `/api/whitelist` | `action=add\|remove&ssid=…&bssid=…` |
| GET  | `/api/logs/wifi` · `/api/logs/ble` | download CSV |
| DELETE | `/api/logs` | wipe logs |
| WS   | `/ws` | live status push (one message per scan cycle) |

Example:

```bash
curl http://192.168.4.1/api/rogues
curl -X POST http://192.168.4.1/api/whitelist \
     -d 'action=add&ssid=BankCorp-WiFi&bssid=aa:bb:cc:dd:ee:ff'
```

---

## How it works

WiFi and BLE share a single radio on the ESP32-S3, so the firmware scans them
**sequentially** in a loop: a non-blocking WiFi channel sweep, then a timed BLE
advertisement scan, then repeat. Results are de-duplicated by BSSID/address in
PSRAM-backed maps, evaluated by the rogue detector, pushed to any connected
dashboards over WebSocket, and appended to the CSV logs. A FreeRTOS mutex guards
the shared tables between the scan loop, the BLE callback task, and the async
web server.

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Upload stalls at *"Connecting…"* | Enter the bootloader manually (hold **BOOT**, tap **RESET**, release **BOOT**), then retry. Try the other USB-C port / a data cable. |
| Port not found | Install the CP210x/CH34x driver (Windows/macOS); add yourself to `dialout` (Linux). |
| Dashboard won't load | Confirm you're on the `WARDRIVER` network and using `http://` (not `https://`) at `192.168.4.1`. Disable mobile data so the phone doesn't route around the AP. |
| Client briefly drops during scans | Expected — the radio leaves the AP channel during each WiFi sweep. The UI auto-reconnects. |
| `LittleFS mount failed` on first boot | Normal once; it auto-formats and works on the next boot. If persistent, run `pio run -t erase` and re-flash. |
| Build error on `ESPAsyncWebServer` | Ensure the **ESP32Async** forks are used (pinned in `platformio.ini`); run `pio pkg update`. |
| Few/no BLE devices | Many phones randomise their MAC and advertise rarely — that's normal. Beacons, wearables and BLE peripherals show up reliably. |

---

## Limitations & notes

- **Bluetooth LE only.** The ESP32-S3 has no Bluetooth Classic radio, so
  Classic-only devices (some headsets, older peripherals) won't appear.
- **No GPS/RTC on board.** Timestamps are uptime-relative. For true wardriving
  with coordinates, wire up a UART GPS module and extend `writeWifiLine()` —
  a natural next step.
- **Scanning, not monitor mode.** WiFi data comes from `scanNetworks()`
  (beacons/probe responses). It does **not** do raw 802.11 monitor-mode capture;
  for deauth-attack detection you'd switch to `esp_wifi` promiscuous mode.
- **Channel hopping disrupts the SoftAP** briefly during each WiFi sweep —
  harmless, the dashboard reconnects automatically.

---

## Project layout

```
platformio.ini        build config (board, partitions, libraries, pio check)
partitions_16mb.csv   16 MB partition table (2×3 MB OTA + ~9.8 MB LittleFS)
src/
  config.h            user-editable settings
  models.h            WiFi/BLE data structures + helpers
  oui.h               built-in MAC → vendor lookup
  web_assets.h        the dashboard (HTML/CSS/JS) in PROGMEM
  main.cpp            scanners, rogue detector, web server, main loop
```

---

## License

MIT &mdash; provided as-is for authorised security testing and education.

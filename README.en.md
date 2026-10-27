[中文](README.md) · [**English**](README.en.md)

# Cloud-Connected Smart Alarm Clock and Local Control Gateway Based on ESP32-S3

> BSc Final Year Project · SWJTU-Leeds Joint School · 2026–2027
> 基于 ESP32-S3 的云端互联智能闹钟与本地控制网关

In one sentence: **a desktop alarm clock you configure from your phone, that still rings when the internet is down, and doubles as an infrared / lighting gateway for the room.**

---

## Display and Typography

- Typeface: STHeiti Medium, converted offline into a C bitmap font with **4 bits per pixel coverage**. The renderer alpha-blends each pixel against the background colour, so edges are smoothly graded instead of jagged (true anti-aliasing).
- Three sizes — `cn28 / cn18 / cn15` — cover all Chinese UI strings; `cn64` contains only digits and the colon and is used for the clock.
- The clock renders into a 254×56 diff sprite whose width is measured from the actual glyph advance, so large type is never clipped; only columns whose pixels changed are pushed to the panel.

## Pages and Interaction

The screen has several pages; the main master–detail flows are:

```
Home ──tap weather card──▶ Weather detail ──tap blank/back──▶ Home
  ├──tap alarm card──▶ Alarm list ──tap a row──▶ Edit ──save/delete──▶ List
  └──tap AC/lamp──▶ Smart home ──tap a card to toggle──▶ Smart home
```

| Page | Content | Touch hot zones (`ui::tap` coordinates) |
|------|---------|------------------------------------------|
| Home | Date, clock, weather card, next alarm, AC/lamp entries | Weather (8,90,150×74), alarm (166,90,146×74), AC (8,170,148×34), lamp (164,170,148×34) |
| Weather detail | Current conditions, feels-like, humidity, 3-day high/low | Back (0,0,60×26), anywhere below y>210 |
| Alarm list | Up to 5 alarms: time / repeat / toggle; tap a row to edit | Row (8,32+i×30,304×28), toggle (250,32+i×30,54×22), add (8,208) |
| Edit | Hour/minute +/−, repeat, enable, delete, save | Four stepper keys (8+i×80,96,72×30) etc. |
| Smart home | AC and lamp cards, tap to toggle | Card 1 (8,40,304×74), card 2 (8,122,304×74) |

Before the touch panel is fitted, single characters typed into the serial console simulate taps (`debugKeys()` in `main.cpp`):

```
w  home → weather detail    a  home → alarm list    e  list → edit first alarm
c  home → smart home        b  go back
```

Once the XPT2046 is installed, pass the converted screen coordinates to `ui::tap(x, y)`; no page logic needs to change.

## 1. Introduction

Conventional alarm clocks can only be set with the buttons on the device, and smart speakers depend on the cloud — they stop working the moment the network does.
This project uses an ESP32-S3 to build an alarm clock that is **configured in the cloud and executes locally**:

- a mobile H5 page to add, edit and delete alarms with weekly repeat rules remotely;
- the schedule lives in both the cloud and the device's NVS, so **it still rings offline**;
- one button on top handles cancel / snooze / night light;
- onboard IR transmitter and a 433 MHz relay turn the clock into a local controller and gateway for the room;
- a 2.4" colour display shows time and weather, and the speaker supports custom ringtones with a gradual volume ramp.

## 2. Features

| Module | Function | Status |
|---|---|---|
| Remote alarms | Create / edit / delete in the cloud, weekly repeat (including workdays), public-holiday skipping | ✅ Done |
| Local alarms | Persisted in NVS, ring offline, restored on boot, synced automatically with cloud edits | ✅ Done |
| Display | Time, date, weather, air quality, next alarm, Wi-Fi name, world clock, day/night map | ✅ Done |
| Web app | Responsive H5 (two columns on desktop, single column on mobile), login, iOS-style alarm editor, settings applied instantly | ✅ Done |
| Lighting | Relay switches a 12 V LED lamp on/off, state reported back | ✅ Done |
| Cloud | Tencent Cloud lightweight server: REST + MQTT + login + voice intents | ✅ Done |
| Weather proxy | Cloud merges weather and air quality, caches by coordinates for 30 min, device fetches once; falls back to direct access when the cloud is unavailable | ✅ Done |
| Button | Single click cancel / double click night light / long press snooze (pins defined, state machine pending) | ⏳ In progress |
| Audio | WAV ringtone, volume ramps 20% → 100% over 30 s | ⏳ Not started |
| Infrared | Learn and replay air-conditioner remote codes (Coolix / Gree) | ⏳ Not started |
| Voice | Button-recorded audio → Tencent Cloud ASR → intent → MQTT command (cloud and simulator only) | ⏳ In progress |
| Device shadow | `desired / reported / version`, reconnect converges via delta; the page only writes desired, the device only writes reported | 📋 Planned |
| Scheduling semantics | Day rollover / time zones / missed-alarm replay within 60 s / SNTP drift compensation | 📋 Planned |
| OTA update | A/B partitions, self-test confirmation on first boot, automatic rollback on failure | 📋 Planned |

## 3. Architecture

```
Mobile H5 ──HTTPS/REST──> Nginx ──> FastAPI ──> MySQL
                                     │
                                     └──MQTT(TLS 8883)──> EMQX
                                                           │
                                               ┌───────────┴───────────┐
                                               │      ESP32-S3         │
                                               │  net / core / ui / bsp │
                                               └───────────┬───────────┘
                                                           │
                         ┌──────────┬──────────┬───────────┼──────────┬──────────┐
                       TFT      button     speaker      IR Tx/Rx    relay    12 V lamp
```

Design principles:

1. **Two data sources**: the cloud owns configuration, the device owns execution; every downstream MQTT message carries a version and the device converges on it.
2. **Offline first**: alarm evaluation happens entirely on the device. The network only synchronises; it never participates in real-time triggering.
3. **Single source of truth**: pins live in `include/pins.h`, the protocol in `docs/protocol.md`, wiring in `docs/hardware.md`.

## 4. Bill of Materials

| Item | Model / spec | Qty | Price (CNY) | Purpose |
|---|---|---|---|---|
| MCU | ESP32-S3 DevKitC-1 N16R8 (16 MB flash / 8 MB PSRAM) | 1 | 55 | Main controller + Wi-Fi |
| Display | 2.4" ILI9341 240×320 SPI | 1 | 35 | Time / weather |
| Audio amp | MAX98357A I2S | 1 | 12 | Ringtone |
| Speaker | 4 Ω 3 W | 1 | 8 | Sound |
| Relay | 1-channel 5 V, active-high, 10 A contacts | 1 | 10 | Lamp switching |
| LED lamp | 12 V 3 W E27 bulb + holder | 1 | 15 | Controlled load |
| IR receiver | VS1838B | 1 | 1.5 | Learning remote codes |
| IR emitter | 940 nm LED + S8050 | 1 | 3.5 | Air-conditioner control |
| Button | 6×6 tactile | 1 | 1 | Top button |
| Power | 5 V/3 A + 12 V/1 A | 2 | 37 | Logic / lamp |
| Perfboard, terminals, jumpers | — | 1 set | 45 | Assembly |
| Enclosure | 3D printed or acrylic | 1 | 50 | Mechanical |

### Pin assignment

| Function | GPIO | Notes |
|---|---|---|
| TFT CS / DC / RST | 10 / 8 / 9 | SPI |
| TFT MOSI / SCK / MISO | 11 / 12 / 13 | 40 MHz |
| TFT backlight | 7 | Via S8050, or tied to 3.3 V |
| Top button | 1 | Internal pull-up, active low |
| IR receive / transmit | 2 / 21 | Transmit via S8050 |
| Relay | 14 | Active high |
| Buzzer (optional) | 15 | — |

> Reserved / forbidden: GPIO 26–37 (flash/PSRAM), 0/3/45/46 (strapping), 19/20 (USB), 43/44 (UART).

## 5. Repository Layout

```
alarm-clock/
├── README.md                     # Chinese README
├── README.en.md                  # this file
├── .gitignore
├── .github/workflows/build.yml   # runs pio run on every push
│
├── docs/                         # documentation and thesis material
│   ├── scoping_en.docx           # project scoping report (EN) ✅
│   ├── scoping_cn.docx           # project scoping report (ZH) ✅
│   ├── protocol.md               # MQTT topics and JSON fields (single source)
│   ├── hardware.md               # wiring tables, power budget, photos
│   ├── figures/                  # architecture, state machines, Gantt
│   └── notes/                    # weekly notes, reused when writing the thesis
│
├── firmware/                     # PlatformIO project ✅
│   ├── platformio.ini
│   ├── boards/                   # custom N16R8 board definition
│   ├── include/
│   │   ├── pins.h                # single source for pin numbers
│   │   ├── config.h              # constants and defaults
│   │   ├── secrets.h             # Wi-Fi / MQTT credentials (git-ignored)
│   │   └── app_state.h           # state shared across tasks
│   ├── src/
│   │   ├── main.cpp              # init order + task creation
│   │   ├── bsp/                  # drivers: tft / button / audio / ir / relay
│   │   ├── core/                 # business logic: alarms / settings / clock
│   │   ├── net/                  # connectivity: wifi / mqtt / ota
│   │   └── ui/                   # presentation: pages / widgets
│   ├── data/                     # LittleFS: ringtones, icons, fonts
│   ├── lib/                      # reusable libraries written for this project
│   └── test/                     # unit tests for pure logic (alarm engine)
│
├── server/                       # Tencent Cloud backend ✅
│   ├── app.py                    # FastAPI: devices, alarms, auth, weather proxy
│   ├── asr.py                    # Tencent Cloud ASR client
│   ├── intent.py                 # rule-based intent parser
│   └── ...
│
├── web/                          # mobile H5 ✅
│   ├── index.html
│   ├── login.html
│   ├── app.js
│   └── style.css
│
└── deploy/                       # server deployment ✅
    ├── alarm.service             # systemd unit (uvicorn on 127.0.0.1:8001)
    ├── deploy.sh                 # release + symlink + health check + rollback
    ├── install_mosquitto.sh      # broker, password file, listener
    ├── install_ops.sh            # backup timer, log rotation
    └── nginx-alarm.conf          # BT panel reverse-proxy snippet
```

**Dependencies flow one way only**: `main → ui / net / core → bsp`. Nothing under `bsp/` may include an upper-layer header.

## 6. Getting Started

### Requirements

- VS Code + PlatformIO IDE (or PlatformIO Core ≥ 6.2)
- Python 3.10+ (bundled with PlatformIO)
- In mainland China a proxy is required; this machine uses **HTTP proxy 127.0.0.1:7897**

```bash
export HTTPS_PROXY=http://127.0.0.1:7897
export HTTP_PROXY=http://127.0.0.1:7897
```

### Build and flash

```bash
cd firmware
pio run                    # build
pio run -t upload          # flash (hold BOOT if it cannot connect)
pio device monitor         # serial console, 115200
```

The first build downloads the toolchain (~300 MB) and the Arduino framework (~250 MB) and takes 5–10 minutes.

### Configuration

```bash
cp include/secrets.example.h include/secrets.h   # Wi-Fi / MQTT credentials
```

Key entries in `platformio.ini`:

```ini
platform = espressif32@6.13.0
board = esp32-s3-n16r8
framework = arduino
board_build.partitions = partitions.csv
```

## 7. Protocol (Summary)

Topic prefix `alarm/<device_id>/`, QoS 1, TLS 8883, last-will topic `state`.

| Topic | Direction | Purpose |
|---|---|---|
| `cmd` | cloud → device | Alarm CRUD, lamp, air conditioner, time sync |
| `state` | device → cloud (retained) | Alarm table, lamp state, online status |
| `event` | device → cloud | Ringing, button presses, errors |
| `ack` | device → cloud | Command result, keyed by command id |

```json
{
  "id": "c-8f3a1",
  "op": "alarm.set",
  "ts": 1793000000,
  "alarm": { "h": 7, "m": 30, "mask": 31, "on": true, "label": "Wake up" }
}
```

`mask` is 7 bits wide; bit 0 = Monday.

## 8. Roadmap

| Phase | Content | Status |
|---|---|---|
| P0 | Toolchain, project skeleton, Git | ✅ Done |
| P1 | ILI9341 bring-up and self-test | ✅ Done |
| P2 | Button debounce and gesture state machine | ⏳ In progress (pins defined) |
| P3 | MAX98357A audio and volume ramp | Not started |
| P4 | IR learning and AC code replay | Not started |
| P5 | Relay and 12 V lamp | ✅ Done |
| P6 | SNTP, alarm engine, NVS | ✅ Done |
| P7 | Cloud REST + MQTT + H5 | ✅ Done |
| P8 | Integration, offline testing, enclosure | ⏳ In progress |
| P9 | Device shadow (desired/reported/version + delta convergence) | 📋 Planned |
| P10 | Scheduling semantics (day rollover, missed-alarm replay, drift) | 📋 Planned |
| P11 | OTA update and rollback (A/B partitions + self-test confirmation) | 📋 Planned |

## Two remotes

| Name | URL | Purpose |
|---|---|---|
| `origin` | `git@github.com:LiuWTQAQ/alarm-clock.git` | GitHub, backup and showcase |
| `gitee` | `git@gitee.com:LiuWT-Yoshino/alarm-clock.git` | the Tencent Cloud server pulls from here |

Both must stay in sync, otherwise you get commits that exist on Gitee but not on GitHub. Daily workflow:

```bash
git add -A && git commit -m "..." && git sync   # pushes to both remotes
```

`git sync` is a repository-local alias (push origin, then push gitee); the nightly auto-commit script pushes to both as well.

## 9. Conventions

- **Commits**: one logical change per commit, message `feat/fix/docs/chore(scope): description`; tag milestones (`v0.2-display`).
- **Branches**: `main` always compiles; feature work in `feat/xxx`, self-tested before merge.
- **Code**: snake_case file names, `#pragma once` in headers, constants in `config.h`, no magic numbers.
- **Dependencies**: after adding a `lib_deps` entry run `PlatformIO: Rebuild IntelliSense Index`.
- **Secrets**: `secrets.h` and `.env` never enter the repository.

## 10. FAQ

**Q1 White or garbled screen?**
Check that CS/DC/RST are not swapped, then lower `SPI_FREQUENCY` from 40 MHz to 20 MHz.

**Q2 `#include <Arduino.h>` is underlined in VS Code but `pio run` succeeds?**
IntelliSense is stale: run `PlatformIO: Rebuild IntelliSense Index` from the command palette, then `Developer: Reload Window` if it persists.

**Q3 `pio run` hangs while downloading the toolchain?**
Check the proxy port (7897 here) and export it in the same terminal; `pio run -v` shows the exact URL being fetched.

**Q4 The board is detected as N8 without PSRAM?**
PlatformIO's built-in board definition defaults to N8. This project ships `boards/esp32-s3-n16r8.json` (16 MB flash, `qio_opi` PSRAM), so `board = esp32-s3-n16r8` is the correct setting.

## 11. Documentation Index

| Document | Description |
|---|---|
| `docs/scoping_cn.docx` | Project scoping report (Chinese) |
| `docs/scoping_en.docx` | Project scoping report (English) |
| `docs/protocol.md` | MQTT and REST contract |
| `docs/hardware.md` | Wiring tables and power budget |

---

© 2026 Wentao Liu · SWJTU–Leeds Joint School · Supervisor: Huiping Huang

## Device Configuration Page (no re-flashing)

Once the clock is on Wi-Fi it serves its own configuration page; open it from any phone or laptop on the same LAN:

```
http://192.168.31.173/          # the actual IP is printed on the serial console
http://alarm.local/             # mDNS name (some routers do not forward .local, the IP is safer)
```

The page offers two things:

| Action | Stored in | Takes effect |
|---|---|---|
| Pick a weather city (40 cities) | NVS `cfg/city` | Weather is fetched for the new coordinates immediately |
| Upload a background image (JPEG) | LittleFS `/bg.jpg` | Redrawn on upload, still there after reboot |

Constraints: JPEG only (there is no PNG decoder on the ESP32); 320×240 or a multiple of it works best.
The image is downscaled to fit and dimmed by 50% so the clock and labels stay readable.

Relevant source: `src/core/settings.cpp` (city table + NVS), `src/net/webcfg.cpp` (HTTP server and upload),
and `drawBgImg()` in `src/ui/ui.cpp` (TJpg_Decoder + dimming + push).

## Partition Table

`partitions.csv` splits the 16 MB flash into 3 MB for the app and 10 MB for LittleFS (label `spiffs`).
After changing the partition table, run `pio run -t erase` before flashing, otherwise the old filesystem data will not line up.

## Fonts

The Chinese glyphs on screen come from `include/ui_font_cn.h`, a bitmap font generated by `tools/gen_cnfont.py`
from STHeiti Medium (bundled with macOS) at 28/18/15 px. Microsoft YaHei was rejected: at 15 px its strokes are too thin
and break apart once binarised, whereas STHeiti Medium keeps an even stroke weight in a 1-bit bitmap.

## UI and Interaction (multi-page)

| Page | Entry | Content |
|------|-------|---------|
| Home | boot | Date / clock (12 or 24 h) / progress bar / weather card / alarm card / Home and Settings tiles |
| Weather detail | tap weather card | Temperature, condition, feels-like, humidity, 3-day high/low |
| Alarm list | tap alarm card | Up to 5 alarms: time + repeat + toggle; tap a row to edit, "Add alarm" at the bottom |
| Edit alarm | tap a row | Hour/minute steppers, repeat cycle, enable, delete, save |
| Smart home | tap Home tile | Two large cards: air conditioner (IR) and lamp (relay), tap to toggle |
| Settings | tap Settings tile | 12/24-hour clock, language (Chinese/English), last weather update time |

Interaction is implemented with touch coordinates via `ui::tap(x, y)`. Without a touch panel, single serial characters simulate taps:

| Key | Action |
|-----|--------|
| `w` / `a` / `c` | Home → weather detail / alarm list / smart home |
| `e` | Alarm list → edit first alarm |
| `s` | Home → settings |
| `t` / `l` | Toggle 12/24-hour clock or language on the settings page |
| `b` | Back to home |
| `q` | Dump the 320×100 screen region as hex to the serial console (debug) |

With an XPT2046 fitted, call `ui::tap(x, y)` from the touch callback; no page code needs to change.

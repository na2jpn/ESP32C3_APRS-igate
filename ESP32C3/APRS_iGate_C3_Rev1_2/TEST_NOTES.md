# v1.2 validation — 2026-11-02

## Target build and image

Arduino CLI 1.3.1 / ESP32 Core 3.3.12 / ESP-IDF v5.5.5.
FQBN: esp32:esp32:XIAO_ESP32C3:CDCOnBoot=cdc, default 4MB OTA layout.
Build exit 0: sketch 1,254,277 / 1,310,720 bytes (95%); static RAM 68,500 / 327,680 bytes.
Application bin: 1,254,432 bytes, leaving 56,288 bytes in the app slot.
SHA256: df854d308ff16f49f4b1ec42f4d543c1191ddc375bd7157ac3b53382b4c79fb9
esptool 5.3.1 confirms ESP32-C3 image, valid checksum and validation hash.
Exact generated scope/OTA template chunks and version/eight frequency/UI strings verified in that image.
See validation/compile_1_2.txt, esptool_1_2.txt and image_1_2.txt.

## Executed regressions

`bash tests/run_all.sh`: exit 0, 19 PASS lines. Requirements: C++17/C++20 compiler,
Python3 with beautifulsoup4 (`python3 -m pip install beautifulsoup4`), Node, OpenSSL.

- Actual SettingsStore/AppConfig: old NVS keys, default 144.6600, all eight presets saved/reloaded,
  missing frequency preserves selection, unsupported persisted value falls back, bad Web input rejected,
  credentials retained, settings clear and NVS-open error handling.
- Actual DRA UART setup and retry commands for all eight presets, required TX field matching RX;
  unchanged pins and UART9600. Real /save handler auth, OTA exclusion, invalid values and reboot semantics.
- Actual LCD service: each of eight frequency labels and 1.2 fallback title; 15-minute/two-cycle timing,
  manual/AP priority, missing display and rollover.
- Actual management/reboot rendering plus actual HTTP output of audio/OTA/log/RX pages in both languages:
  12 HTML pages checked for resolved keys, valid JS syntax, language attributes and English visible labels;
  eight unique options and correct default; literal user data containing translation markers preserved.
- Actual OTA uploader tests in Japanese and English: SHA256 vectors, lost ACKs before/after commit,
  start/finish/reboot loss, status retries/503, cancel, resume after reload, wrong identity/offset/reset,
  auth/verification errors, bounded retry and successful progress lasting over five minutes.
- Actual audio monitor UI tests in Japanese and English: level/peak/dBFS/scale, pause/freeze races,
  no overlap, retry/backoff, OTA pause, stale-data clearing, hidden/pagehide suspension.
- Inherited DSP/FIX3 equivalence, tone discrimination, HDLC/FCS, ADC ownership/recovery/freshness/errors,
  scope/meter continuous peak hold, RF diagnostics, NTP/JST/clock correction, bounded log/CSV,
  position beacon, Wi-Fi reconnect and OTA session/SHA256/RTC/auth regressions.

## Changes and limits

Presentation language uses the URL only: explicit lang=en is English; other/missing values are Japanese.
Known navigation/forms/return links preserve language. No new NVS language key. Raw logs, CSV fields,
API keys, technical state codes, user input and JST timing are preserved. Unsaved edits are lost on switch;
active OTA blocks the language switch link. Settings apply RF only after reboot.

The following remain byte-identical to v1.1: afsk1200.h, audio_adc.h, audio_scope.h, ax25.h,
aprsis.h, dra818v.h, position_beacon.h, rx_diagnostics.h, app_log.h, device_clock.h and ntp_clock.h.
OTA session/backend logic retains the existing protocol; web headers/frontend add localization and a
language-navigation guard. No change to RF sensitivity, AF gain, filters, baud or ADC sampling.

Japanese setup PDF: five pages rendered and visually checked. English README included.
Prior user v0.12 testing confirmed three decoded/gated RF status transmissions, then one RF position
transmission and aprs.fi display via qAO,JH1HST-10. This environment has no attached target device.
The new presets, weak-signal performance, heap use under real HTTP traffic and long-run behavior still
need field verification. No automatic rollback for an unbootable app; USB recovery remains necessary.

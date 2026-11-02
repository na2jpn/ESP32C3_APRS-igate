# v0.10 validation — 2026-11-01

Target: XIAO ESP32C3, ESP32 Core 3.3.12 / ESP-IDF 5.5.5, CDC enabled,
Default 4MB with spiffs (1,310,720-byte OTA app slot).
Arduino CLI 1.3.1. Compile completed with exit 0.
Sketch: 1,222,641 bytes (93%); static RAM: 65,420 bytes.
App bin: 1,222,800 bytes; see image_0_10.txt and SHA256SUMS.txt.
CLI printed failed optional discovery/monitor/library-index initialization messages
because its downloader could not resolve the network; target tools were installed
separately with archive SHA checks. Actual compilation/link/image generation succeeded.

Run: bash tests/run_all.sh (g++, Python3, Node, OpenSSL development headers/libcrypto).
All checks passed; validation/host_0_10.txt contains full output.

New tests execute production methods/function/HTTP callback source:
- Queued recovery makes no driver call from HTTP side. ADC owner performs all calls.
- Maintenance defers recovery; pending request remains until maintenance ends.
- stop/flush/start API success does not count as data recovery; actual samples do.
- 3-second cadence, 3-attempt cap/FAULT, manual replenishment, stable-run replenishment.
- Stop failure never flushes or restarts a running driver; flush/start failure paths.
- Unsigned timer crossing; invalid ADC row; bounded 32-block drain with DSP skipped.
- Actual Wi-Fi service waits 60 seconds of continuous outage, resets on reconnection.
- Actual ADC health labels and HTTP authentication/token/OTA/uninitialized handling.

Inherited regressions retained and passed:
- FIX3 strong-signal DSP state/tone/peak comparison, synthetic Bell 202 waveform,
  HDLC destuffing/FCS acceptance and corrupted-frame rejection.
- ADC initialization cleanup, bounded task scheduling, read timeout/error counters.
- LED/LCD timing and wrap; diagnostic rings/events/CSV (now 27 columns), NTP/JST.
- OTA SHA256/session/idempotence/replay/offset/auth/cancel/timeout/RTC receipt,
  generated browser JS fault/retry/resume tests including >5-minute transfer.
- Position beacon, log capacity/auth and OTA-specific Wi-Fi reconnection.

Limits:
No physical device was attached to this environment. Runtime heap/stack after increasing
DMA storage, actual CPU savings, weak-signal RF behavior, long-duration recovery,
Wi-Fi coexistence and OTA transfer on the installed unit require field confirmation.
No claim that the observed ADC stall's underlying cause has been proven.
The pool consumes approximately 8KiB more heap than v0.9; static RAM alone does not
include it. RF gain and signal thresholds remain the same as v0.9.

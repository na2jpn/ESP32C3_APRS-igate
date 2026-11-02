# v0.11 validation — 2026-11-02

Target: XIAO ESP32C3, ESP32 Core3.3.12 / ESP-IDF5.5.5, CDC enabled,
default 4MB OTA layout. Arduino CLI1.3.1.
Build exit0: sketch 1,235,041 / 1,310,720 bytes (94%);
static RAM 67,612 / 327,680 bytes; app image 1,235,200 bytes.
Optional discovery/monitor/index initialization warnings do not prevent build.
The compiler/SDK/esptool archives were installed separately with SHA256 checks.
See validation/compile_0_11.txt and image_0_11.txt.

Run bash tests/run_all.sh (C++17, Python3, Node, OpenSSL headers/libcrypto).
New audio tests execute production code:
- AudioScope fixed storage 2184 bytes. No-request/off, 3s lease expiration,
  millis wrap, OTA pause, invalidation/no stitching across missing samples.
- Constant DC, known AC waveform extrema/RMS, high DC +0.5-code AC precision,
  full-range clipping and actual ADC task integration without extra decoder samples.
- Real HTTP callbacks: configured/unconfigured auth, OTA/maintenance denial,
  fresh/invalid/stale/fault/recovering states and exact 264-pair JSON payload.
- Actual emitted browser JS in Node VM: dBFS/percent/meter/scale, pause/freeze,
  late-response race, resume, single in-flight request, retry/backoff/OTA,
  stale-data clearing, hidden and pagehide suspension.
- Generated header must contain the exact source JS.

All inherited regressions retained: FIX3 strong-signal DSP comparison and synthetic
Bell202/HDLC/FCS, ADC initialization/scheduling/recovery, Wi-Fi outage timer,
LED/LCD, receiver event rings/CSV/NTP, OTA session/SHA256/idempotent retries/resume/
RTC receipt/>5-minute transfer and browser error paths, log and position beacon.

Limits: no device attached. No real RF/audio/OTA/CPU/long-run test was performed.
The UI was exercised by its real JS with DOM/canvas mocks; a real browser visual
check was unavailable because browser package download was blocked. Frames are
40ms glimpses, not a continuous recording. The 5s peak is over received glimpses.

Final image checked to contain the exact audio HTML/JS from the tested source and Rev0.11.

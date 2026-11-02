# v0.12 validation — 2026-11-02

Target: XIAO ESP32C3 / ESP32 Core3.3.12 / ESP-IDF5.5.5, CDC enabled,
default 4MB OTA layout, Arduino CLI1.3.1.
Build exit0: sketch 1,235,523 / 1,310,720 bytes (94%);
static RAM 68,668 / 327,680; OTA app image 1,235,680 bytes.
Optional discovery/index warnings did not prevent compilation/link/image generation.
Target image was verified to contain the exact final tested audio HTML/JS and Rev0.12.

Run: bash tests/run_all.sh (C++17, Python3, Node, OpenSSL headers/libcrypto).
New assertions execute actual AudioScope code:
- A 40ms pulse placed between HTTP polls remains in device peak even after the
  latest waveform and 200ms meter are quiet. 25 consecutive windows counted.
- Reading a second copy does not consume peak history.
- Hold boundary at exactly 5000ms, expiry after that boundary, timestamp wrap.
- Correct 200ms steady-state AC RMS, invalidation/reset of peak on discontinuity.
- Production HTTP emits independent meter_rms/meter_peak5s/meter_windows fields.
- Exact emitted UI uses server peak, including unseen burst, rather than computing
  a maximum only from polled frames. Freeze/resume/stale/OTA/retry tests retained.

All inherited regressions passed: ADC task/lease/RMS/clipping/gaps, FIX3 strong-signal
DSP and synthetic Bell202/HDLC/FCS, ADC recovery, Wi-Fi outage timer, LCD/LED,
RX diagnostic records/CSV/NTP, logger/position beacon and OTA SHA256/session/
retry/resume/RTC/>5min transfer/browser error paths.
See validation/host_0_12.txt, compile_0_12.txt and image_0_12.txt.

Limits: no attached hardware. Actual RF decoding, CPU load, weak Wi-Fi operation,
calibrated input voltage and amplifier necessity remain unverified. The fixed
AudioScope host object is 3232B (1048B more than v0.11); HTTP uses temporary heap.
Browser JS was tested with DOM/canvas mocks, not a physical-device browser session.
Meter/peak accumulation runs during the short viewer lease (3s after last request).
It resets across missing data, recovery, maintenance or inactive lease. UI refresh
is still about 1s; green averages last ~200ms and red is the maximum ~40ms AC RMS
in the last ~5s. It is not an RF strength meter or a sample-amplitude peak detector.

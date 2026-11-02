# v1.1 validation - 2026-11-02

## Target build

Arduino CLI 1.3.1 / ESP32 Core 3.3.12 / ESP-IDF v5.5.5.
FQBN: esp32:esp32:XIAO_ESP32C3:CDCOnBoot=cdc, default 4MB OTA layout.
Build exit 0: sketch 1,230,901 / 1,310,720 bytes (93%); static RAM 68,500 / 327,680.
Application image: 1231056 bytes, under the 1,310,720-byte app slot.
SHA256: 2fc22568befa07cc45b245c533386e539936bd48a8aa63c66cc89359d8955b59
esptool 5.3.1 confirms ESP32-C3 image/checksum and digest valid.
See validation/compile_1_1.txt, image_1_1.txt and esptool_1_1.txt.

## New RF setting

- Production SettingsStore and AppConfig default to 144.6600 MHz.
- NVS string key rx_freq stores only 144.6600 or 144.6400.
- Missing key from older firmware uses 144.6600 without clearing Wi-Fi/APRS/beacon/OTA settings.
- Unsupported stored value falls back to 144.6600. Web rejects invalid input before saving.
- Save & Reboot applies the selection after reboot; missing rxfreq on an old client preserves the current choice.
- DRA initial setup and 30-second retry both use config.rxFrequency; required TX field follows RX.
- Web RF label, LCD, boot log and /status use the saved selection. /status adds rf_mhz and afsk_baud.
- Both selections use 1200bps AFSK. UI/PDF/README explicitly distinguish 144.640MHz's usual 9600bps operation.
- RF/AF gain, filters, DSP thresholds, sample rate and receive-only hardware mode are unchanged.

## Executed host regressions

bash tests/run_all.sh: exit 0. C++17/C++20, Python3, Node, OpenSSL required.
The new tests/test_rf_settings.py compiles and executes actual settings.h,
web_rf_settings.h, dra818v.h, the extracted production /save route and DRA retry block:
- Old-key migration/default, both channel save/reload, setting reset, corrupted stored value fallback;
- Credentials retained; selected option unique for both channels; warning remains visible;
- Actual UART command includes the selected frequency in both required fields;
- UART stays 9600bps on the original pins; the DRA retry uses the same selected frequency;
- Save route authentication, OTA exclusion, missing field preservation, invalid input rejection,
  failed NVS open, deferred application until reboot and success response.
The existing LCD test now executes the production frequency-label method and checks
both 144.660 RX ONLY and 144.640 RX ONLY, retaining timing/rollover/manual/AP tests.
All inherited DSP/FIX3 equivalence, HDLC/FCS, ADC/overflow/recovery, scope/meter/peak,
Wi-Fi, NTP, log/CSV, beacon, OTA SHA256/session/retry/resume/auth/RTC and browser tests passed.
See validation/host_1_1.txt. Exact final audio monitor HTML/JS verified in the target image.

## Scope and limits

DSP/ADC/OTA/APRS/beacon/diagnostic/clock/logger implementation files remain byte-identical to v1.0.
Settings, RF selection/DRA parameters, display version/labels and tests/docs are the scoped changes.
The regenerated four-page v1.1 PDF was rendered and visually checked.

Prior user evidence on v0.12: three counted RF status transmissions decoded/gated, then one
RF position transmission decoded/gated and visible on aprs.fi via qAO,JH1HST-10.
That test does not validate the new v1.1 radio selection on real hardware. No attached device
was flashed here. After OTA, check Rev1.1, selected RF channel, 1200bps, VERIFIED and Samples;
then Save & Reboot for each desired channel and test a matching AFSK1200 transmitter.
Normal-station receive rate, weak-signal performance and long-run stability remain field checks.

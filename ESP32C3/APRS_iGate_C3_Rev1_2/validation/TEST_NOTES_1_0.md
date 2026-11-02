# v1.0 validation - 2026-11-02

## Target build

XIAO ESP32C3 / ESP32 Core 3.3.12 / ESP-IDF v5.5.5 / Arduino CLI 1.3.1.
FQBN: esp32:esp32:XIAO_ESP32C3:CDCOnBoot=cdc
Default 4MB OTA layout. Build exit 0 (validation/compile_1_0.txt).
Sketch: 1,229,383 / 1,310,720 bytes (93%). Static RAM: 68,484 / 327,680 bytes.
OTA application image: 1,229,536 bytes, under the 1,310,720-byte app slot.
Image SHA256: c47f2506cf14c6a2aa2ff37470e51fec4f36245295b00d91914c40eaa2e89edf
esptool 5.3.1 confirms ESP32-C3 chip ID 5, DIO/80MHz/4MB, valid checksum and image digest.
App descriptor IDF build metadata is inherited from the official SDK; user-facing firmware version is FW_VERSION 1.0.

## Change and inspection

- DRA_RX_FREQ and required module TX field: 144.6600 MHz. RF decoder remains 1200bps Bell202 AFSK.
- Web and LCD use DRA_RX_LABEL 144.660; Web also explicitly shows 1200 bps AFSK.
- Boot log shows RF RX frequency, baud, receive-only mode and DRA group setup command.
- UART control remains 9600bps. This release does not add a 9600bps RF decoder.
- FW_VERSION / Web / LCD / audio monitor / RX diagnostic CSV are updated to 1.0.
- Image contains the new RF/version/log strings and the exact final tested audio HTML/JS.
  Old 144.6400, old LCD frequency, Rev0.12 and the old 0.10 LCD title are absent.
- DSP, ADC, meter data processing, OTA, NVS settings, APRS forwarding, beacon,
  RX diagnostics, NTP/clock and app logger implementations are byte-identical to v0.12.
- No change to DRA_VOLUME=5, filter command, DSP thresholds or sample rate.
- Full v1.0 setup guide regenerated and all 4 rendered pages visually checked.

## Host tests

Run: bash tests/run_all.sh. Exit 0. See validation/host_1_0.txt.
All existing actual-code regressions pass: FIX3 sample-by-sample equivalence;
synthetic Bell202 HDLC/FCS; bounded ADC work and DMA errors; ADC recovery ownership,
retry caps and data freshness; settings/logger/beacon; NTP/RX records/CSV;
OTA SHA256/session/retry/resume/RTC/auth/cancel and >5min transfers; Wi-Fi reconnect;
LCD/LED timing and millis rollover; audio scope lease, DC/RMS/clipping, 200ms meter,
unseen-pulse 5s peak/expiry, multi-reader state, HTTP auth/status and browser UI.
Frequency/version assertions validate production config; LCD tests use the new production RF label.

The existing position WAV was resampled 48kHz -> 26.4kHz and decoded through the
actual v1.0 Afsk1200Decoder on the host: all 5 payloads/FCS matched at 0.25x/1x/2x gain,
with zero CRC errors. This is a host DSP test, not a new over-the-air hardware test.

## Hardware evidence and limits

User-confirmed on 2026-11-02 with v0.12 and known 1200bps WAV audio via an FM radio:
3 counted status transmissions -> Decoded/Gated 3;
1 position transmission -> Decoded/Gated 4;
Last RX matched JH1HST-7>APRS:!3549.52N/13947.94E-APRS POSITION TEST 01/05.
aprs.fi displayed that position with qAO,JH1HST-10. Displayed DMA overflow/read
errors/timeouts were zero. This confirms that test's full RF-to-map path.

The v0.12 test used matching 144.640MHz test TX/RX. It does not validate the newly
corrected v1.0 144.660MHz hardware reception or prove the cause of all prior failures.
After OTA, check Rev1.0, RF 144.660MHz/1200bps, VERIFIED and increasing Samples;
then test a known normal AFSK1200 station on 144.660MHz (e.g. the user's FT1XD).
Normal-station receive rate, weak-signal performance, long-term stability, calibrated
AF voltage, live ADC/monitor CPU load and NTP synchronization still need field checks.
No connected device was flashed by this build process.

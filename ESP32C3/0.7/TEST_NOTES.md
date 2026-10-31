# v0.7 validation - 2026-10-31 JST

## Scope and known limitation

The v0.5 device reset its upload connection after about 3.5 seconds and stayed on
v0.5 without reboot. No device-side trace was captured; the root cause is NOT
established. v0.7 changes the OTA transport and adds diagnostics. This is not a
claim of a proven hardware fix. Initial v0.7 installation is by USB.

## Target compile

Core 3.3.12 / ESP-IDF 5.5.5 / XIAO ESP32C3, CDC enabled, default 4MB OTA scheme.
See validation/compile_0_7.txt for the final build sizes/result.
The CLI's missing discovery/monitor warnings concern device enumeration, not the
successful target compilation. No device USB upload was performed here.

## Host tests

- tests/run_host.sh: real decoder/capture regression against FIX3, Bell 202/HDLC/FCS,
  ADC failures, bounded work and diagnostic math. Added maintenance drain mode:
  samples are read but not decoded; normal mode flag can be restored.
- tests/test_led_cycle.py and tests/test_lcd_cycle.py: real main-sketch functions,
  LED duty cycle, LCD 15-minute wake / two 12-second cycles, manual/AP priority,
  rollover. No sensitivity/normal DSP changes.
- tests/test_v07.py: real logger, OTA and log HTTP callbacks with API mocks.
  Fixed 4096-byte log/whole-line eviction, high-volume suppression, sanitization,
  auth, token, clear and snapshot. Coordinates/rate/partial-TCP-write regressions.
  OTA auth/session/slot/declared size/order/duplicate offsets/invalid hex/truncation,
  cancellation, 120-second timeout and wrap, write failure, verification failure,
  byte-exact multi-chunk binary transfer including zeros, response before deferred
  reboot, retained status/checksum and boot log.
- tests/test_ota_ui.js: generated real update-page script in Node VM; exact binary
  chunking, ACK progress, HTTP failure, connection failure and cancel. Does not
  claim success on a disconnected transfer. JavaScript syntax also checked.

Commands: bash tests/run_all.sh (g++, python3 and node required).
Mocks do not emulate RF, DMA hardware, Flash stall timing, HTTP/TCP networking,
real browser authentication, RTC retention across actual resets, or power failure.

## Design evidence

Official Core 3.3.12 Parsing.cpp converts plain HTTP body to String(plainBuf),
which cannot preserve arbitrary binary NUL bytes. Therefore the bounded OTA chunk
body is ASCII hex; the C3 decodes it to bytes before Update.write. Each chunk
is acknowledged before the browser sends the next. We do not retry writes after
an uncertain ACK. Core Update.end(false) remains responsible for image validation
and changing boot slot. ADC DMA is not stopped/restarted; the owning ADC task
checks a synchronized maintenance flag and drains samples without DSP.

The event log has no periodic Flash writes. A small RTC_NOINIT record uses a magic
and checksum for last-OTA state/byte count only. Power-loss retention and automatic
rollback of a bootable but faulty new image are NOT provided.

## Device acceptance before reinstalling

USB install -> check v0.7/settings/VERIFIED/log -> same-v0.7 OTA -> progress/verify/
reboot/reconnect/uptime/PREVIOUS OTA -> cancel test -> abandoned session timeout ->
RF decoding resumes. Save the Web log on any failure before power cycling.

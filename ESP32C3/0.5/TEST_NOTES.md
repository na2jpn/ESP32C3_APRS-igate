# v0.5 validation — 2026-10-31 JST

`python3 tests/test_lcd_cycle.py` compiles actual serviceLcd() and LCD constants
with host display/network stubs. PASS: first wake at 15 minutes, pages 0/1/2
at 4-second intervals over two complete cycles, off at 24 seconds, next wake,
manual illumination priority without shortening its deadline, setup AP priority,
uint32 millis rollover, absent LCD behavior. No new blocking delay is introduced.

Existing decoder/ADC, LED, position/OTA and TCP partial-write tests also pass.
The ADC, decoder, AX.25, DRA, LCD driver, settings, APRS-IS, position beacon and
OTA headers are byte-identical to v0.4. Only main sketch/LCD constants change.
Target compile PASS: Core 3.3.12, XIAO ESP32C3, USB CDC Enabled, default 4MB OTA partitions.
Sketch 1,145,297 / 1,310,720 bytes; global RAM 47,748 bytes.
See validation/compile_0_5.txt. v0.5 hardware behavior is untested.
Older target-build sizes below describe v0.4, not v0.5.

---

# v0.4 validation — 2026-10-31 JST

New tests: `python3 tests/test_v04.py` compiles the real position_beacon.h and
web_ota.h with host mocks. Checks decimal coordinate validation, APRS position
rounding including minute/degree carry, hemispheres and geographic limits,
newline sanitation, disabled/unverified states, first send, failed-send retry,
30-minute cadence and millis rollover. OTA checks password-disabled/auth failure,
invalid token, missing OTA slot, oversized/truncated/overlong uploads, cancellation,
timeout, flash-write failure, image-validation failure, and successful response/reboot.

Existing decoder/ADC tests (`bash tests/run_host.sh`) and actual LED function test
(`python3 tests/test_led_cycle.py`) also pass. The decoder, ADC, LCD and DRA
headers remain byte-identical to v0.3.

These mocks do not establish real flash-update or power-failure behavior, NVS
persistence, RF performance, HTTP browser interoperability, APRS-IS acceptance,
or aprs.fi map appearance. v0.4 needs a USB-accessible hardware test before
reinstallation: verify existing settings, set coordinates/password, see VERIFIED,
confirm position in server raw packets/map, upload same v0.4 over OTA, verify
reboot/version/settings, then test a disconnected upload and USB recovery.
No automatic rollback from a bootable but broken new application is implemented.

Target compile PASS: ESP32 Core 3.3.12 / ESP-IDF v5.5.5,
`esp32:esp32:XIAO_ESP32C3:CDCOnBoot=default,PartitionScheme=default`.
USB CDC On Boot default=Enabled (verified in this core's boards.txt).
Sketch 1,145,107 / 1,310,720 bytes; global RAM 47,740 / 327,680 bytes.
Application .bin 1,145,264 bytes. esptool image-info: ESP32-C3, 4MB/DIO,
checksum valid and validation hash valid. This verifies image structure, not boot.
See validation/compile_0_4.txt. The CLI emitted missing discovery/monitor download
warnings due to restricted network access but compilation exited 0 and linked
successfully; USB discovery/upload was not exercised.
Actual sendRfPacket() body is also tested: complete CRLF write succeeds;
partial write disconnects and does not increment the successful-send counter.

---

# Historical baseline test notes (through v0.3)

Current validation: 2026-10-27 JST. This file supersedes the earlier Rev0.1 test claims.

## Host tests (tests/run_host.sh)

Tests compile the actual v0.2 decoder/capture headers with mock ADC/RTOS APIs.
The FIX3 decoder is included verbatim as tests/reference/FIX3_afsk1200.h.

Passed:

- 158,400 samples: FIX5 demodulator state compared with FIX3, sample by sample.
- RMS and 20-bin peak hold compared against FIX4's full-rate square-root method.
- All-sample ADC min/max and 250 ms publication/sample counts.
- 1200 Hz and 2200 Hz detector discrimination.
- New-handle/config/callback/start/task-creation failure paths and resource cleanup.
- Duplicate begin rejection; configured priority 1 and 6144-byte task stack.
- A continuously available mock DMA stream forces a 1-tick sleep after at most 4 reads.
- Timeout/read-error counters, decoder resets and overflow ISR counter.
- HDLC destuffing, valid FCS acceptance, invalid FCS rejection.
- Synthetic continuous-phase Bell 202 at 26,400 samples/s: both FIX3 and FIX5 decode
  one AX.25 UI frame, with bytes exactly matching the generated payload.
- AddressSanitizer and UndefinedBehaviorSanitizer run passed with leak detection disabled.
  LeakSanitizer cannot inspect process threads in this host; no leak-check success is claimed.

The mocks do NOT emulate Wi-Fi/ADC DMA hardware, actual task scheduling, ISR latency,
CPU load on a C3, NVS persistence or an APRS-IS server.

## Target build

See validation/compile_FIX3.txt and compile_FIX5.txt for actual build output.
Board: esp32:esp32:XIAO_ESP32C3:CDCOnBoot=cdc, ESP32 Core 3.3.12 (ESP-IDF 5.5.5).
The bundled core, C3 SDK libraries and tool archives were verified against package-index SHA-256 values.
FIX3 build: 1,103,839 bytes flash; 46,916 bytes global RAM.
FIX5 build: 1,107,501 bytes flash; 47,132 bytes global RAM.
These RAM figures exclude dynamically allocated DMA pools, task stacks and networking buffers.

Compilation uses Arduino CLI; Arduino IDE 2.3.10 itself was not executed.

## User-reported hardware validation of the FIX5 baseline

2026-10-27 JST: XIAO ESP32C3 + DRA818V + LCD, Arduino IDE 2.3.10 / Core 3.3.12.

- Boot completed; saved station settings used, Wi-Fi connected and APRS-IS VERIFIED.
- IC-7100 transmitted the supplied Bell 202 test WAV in FM through its microphone input.
- With mic gain 50% and PC volume 30%, UI reported Decoded=2, Gated=1, Dup=1, Blocked=0.
- ADC DMA overflow/read errors remained zero; stack watermark 4164 bytes in the reported screen.
- Gated means the client's TCP write succeeded; upstream server acceptance/display was not observed.
- LCD initially remained at Booting; a short SETUP press restored the normal screen.
  The exact cause was not isolated. v0.2 explicitly requests the normal screen at setup completion.
- Supplied 48 kHz mono WAV, resampled to 26.4 kHz and passed to the FIX5 decoder:
  3 good frames, 0 CRC errors at four amplitude scales; source N0CALL, info >APRS iGate test.

## v0.2 validation

The decoder, ADC capture, AX.25 parser, APRS-IS client and NVS settings implementation
are byte-identical to FIX5. Source changes are version text and the LCD boot handoff.
Run sh tests/run_host.sh. Target build result: validation/compile_0_2.txt.

## Still requires hardware

- v0.2 startup LCD handoff and backlight timer, cold starts and repeated power cycles.
- Real other-station packet reception, weak-signal/level/distortion performance.
- Upstream APRS-IS acceptance/display.
- Setup AP boot/fallback, reconnect/recovery and overnight/long-duration operation.

This is the canonical baseline for installation testing, not a claim of full production qualification.


## v0.3 validation (2026-10-27 JST)

- tests/test_led_cycle.py extracts the actual LED function and configuration constants,
  compiles them on the host, and verifies cycle boundaries, repeated cycles,
  independent disconnected/unverified states and the RX blanking override.
- Existing decoder/capture host regression: PASS.
- AFSK/ADC/AX.25/APRS-IS/settings/LCD-driver files are byte-identical to v0.2.
- This turn's workspace has no ESP32 cross toolchain after workspace maintenance.
  No v0.3 ESP32 target compilation success is claimed; prior compile logs apply only to their named versions.
- Hardware LED timing and the Arduino IDE target build need confirmation on the user's machine.

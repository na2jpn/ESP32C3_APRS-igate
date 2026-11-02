# APRS iGate ESP32C3 v1.2

PS Ham-ware / Akihabara-Giken. Receive-only APRS iGate for XIAO ESP32C3 and DRA818V.
**Bell 202 AFSK, 1200bps, 1200/2200Hz tones, AX.25 FCS validation, APRS-IS qAO.**
There is no RF transmit function and no 9600bps RF decoder.

## Frequency selection

| MHz | Region |
| --- | --- |
| 144.640 | — |
| 144.660 | JA (default) |
| 144.390 | US |
| 144.800 | EU |
| 145.175 | AU |
| 144.575 | NZ |
| 144.930 | AR |
| 145.570 | BR |

Choose a preset in the management page, then **Save and reboot**. It is stored in NVS and applied
at boot and DRA initialization retries. Region labels are examples; use the channel appropriate
to your local APRS network. Every preset uses AFSK1200. In Japan, 144.660MHz commonly uses 1200bps,
while 144.640MHz commonly uses 9600bps; choosing it does not add 9600bps support.
Existing v1.1 frequency selections and Wi-Fi/APRS/beacon/OTA settings are retained.
A missing or invalid stored frequency falls back to 144.660MHz.
RF sensitivity, AF gain, sample rate and decoder thresholds are unchanged from v1.1.

## Setup and language

Open the `.ino` in this directory with all accompanying headers. Tested build environment:
Arduino IDE 2.3.10 / ESP32 Core 3.3.12, XIAO_ESP32C3, USB CDC enabled, default 4MB OTA partition
layout (1,310,720-byte app slots). Do not select Huge APP or No OTA. Normal updates retain NVS.

On an unconfigured unit, join Wi-Fi `APRS-iGate-XXXX`, password `aprssetup`, then open
`http://192.168.4.1/`. During normal operation use the device IP or `http://aprs-igate.local/`.
Set your 2.4GHz Wi-Fi, callsign-SSID, APRS-IS passcode and server (default rotate.aprs2.net:14580).
An empty password field keeps its stored value. The comment limit is 40 bytes, not 40 Unicode characters.
For an own-position Internet beacon, enable it and enter decimal latitude/longitude (south/west negative).
The Grid field is stored only; it does not supply beacon coordinates. Own position is sent to APRS-IS
after verification and every 30 minutes. It is never transmitted on RF.

Japanese is the default UI. Select **English** at the top of a page, or open `http://DEVICE_IP/?lang=en`.
Settings, audio monitor, RX diagnostics, logs and OTA pages support both languages.
Navigation, forms and the reboot return URL keep the language parameter. No parameter means Japanese.
Changing language does not require saving or rebooting. Unsaved form edits are lost when switching.
Language switching is blocked while the OTA uploader is running. Raw logs, CSV headers, API keys,
user-entered values and technical state codes retain their original form. Diagnostic wall time remains JST.

## OTA updates

First set an 8-63 character OTA password. Authentication uses username `admin` and that password.
Upload only `firmware/APRS_iGate_C3_v1.2_OTA.bin` through the firmware update page.
Do not upload the source ZIP, merged image, bootloader or partition image.
Web OTA from v0.9 onward supports 1KB chunks, confirmed-offset retries, SHA-256 and image validation.
Progress may continue beyond five minutes. A session expires after 10 minutes without new device-side data.
Reloading can resume the same live session by selecting the exact same file. Device restart or power loss
cannot resume an incomplete session. Keep power connected. Reception and gating pause during updates.
After restart, check Rev1.2, RF frequency, 1200bps, APRS-IS VERIFIED and increasing Samples.
There is no automatic rollback for an unbootable new app; USB recovery is required.

## Diagnosis and controls

- `/audio`: about 200ms AC RMS level; device-held five-second maximum 40ms RMS; latest 40ms waveform.
  It measures ADC audio, not RF signal strength. FM idle hiss can exceed received voice level.
- `/rxdiag`: bounded RAM records of significant reception changes, up to four events, CSV export.
- `/logs`: maximum 4KB RAM log, overwritten oldest first. Download before a reboot or power loss.
- ADC freshness, overflow/errors and recovery counts matter in addition to the startup status.
  ADC recovery automatically retries up to three times after three seconds without valid data.
- LCD: three four-second screens, two cycles every 15 minutes; short SETUP press lights it for 60 seconds.
- SETUP held 5 to <15 seconds enters setup AP on release; 15 seconds or more clears stored settings.
- PTT must remain physically fixed to receive-only. DRA818V AF_OUT pin 3 feeds the audio input circuit
  and XIAO D1/GPIO3 (ADC1 channel 3). UART control remains 9600bps, independent of RF baud rate.

## Validation scope

See TEST_NOTES.md for target build/image checks and executable host regressions.
Prior user testing with v0.12 confirmed three decoded/gated status packets and one position packet,
including aprs.fi mapping via qAO,JH1HST-10. That is not a field test of every v1.2 preset or proof
of weak-signal performance or long-term stability. No real hardware is attached to this build environment.

[Repository](https://github.com/na2jpn/ESP32C3_APRS-igate) · [Japanese setup PDF](../../Initial_Setup_Guide_PDF/APRS_iGate_C3_v1_2_Initial_Setup_Guide_JA.pdf)

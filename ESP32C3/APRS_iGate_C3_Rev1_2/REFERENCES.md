# v1.2 frequency preset references

- KENWOOD TH-D74A/E User Manual, APRS data-band frequency table:
  North America 144.390, Europe 144.800, Australia 145.175, New Zealand 144.575,
  Argentina 144.930, Brazil 145.570 MHz; Japan 144.660/1200bps and 144.640/9600bps.
  https://www.kenwood.com/usa/Support/pdf/TH-D74A_D74E.pdf
- Region labels are representative presets. All selections in this firmware use Bell 202 AFSK1200.
  Selection does not add other modem formats.

# v1.0 国内APRS周波数の確認

- 八重洲無線 FTM-300D/DS 取扱説明書 APRS編、5ページ「APRSの運用周波数を設定する」：
  国内では144.64MHz / 9600bps、144.66MHz / 1200bps（資料記載時点2020年5月）。
  https://www.yaesu.com/jp/manuals/yaesu_m/FTM-300D_APRS_Manual_JPN_2112-B.pdf
- KENWOOD TH-D75 User Manual, Setting the Data Band Frequency：
  Japan 144.640MHz (9600bps) / 144.660MHz (1200bps)。
  https://kasc.kenwood.com/files/prod/3692/5/THD75E_User_manuals_English.pdf
- APRS Protocol Reference 1.0.1：1200bps試験WAVのAX.25位置・ステータス形式の確認。
  https://www.aprs.org/doc/APRS101.PDF

# v0.10 ADC recovery references

- ESP-IDF 5.5.5 ADC continuous API, including non-thread-safe API and stopped-state pool flush:
  https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/peripherals/adc_continuous.html
- Exact pinned driver source, stop/start reset and EOF clear:
  https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_adc/adc_continuous.c
- ESP32-C3 ADC-270 erratum:
  https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32c3/03-errata-description/esp32c3/sar-adc-continous-insufficient-read.html
- Core3.3.12 C3 HAL header: ADC_LL_WORKAROUND_CLEAR_EOF_COUNTER=1 confirmed locally.

The existing driver already implements the workaround. Its existence does not prove
that this erratum caused the observed v0.9 stall. Software recovery is performed by
the single ADC owner task, and success requires real samples, not just successful API returns.

# References used for Rev0.1

- Seeed Studio XIAO ESP32C3 pin map / power / ADC notes:
  https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/

- DRA818V datasheet / UART command set:
  https://www.dorji.com/docs/data/DRA818V.pdf

- APRS-IS Connecting:
  https://www.aprs-is.net/Connecting.aspx

- APRS-IS IGate details / qAO for receive-only IGate:
  https://www.aprs-is.net/IGateDetails.aspx
  https://www.aprs-is.net/q.aspx

Rev0.1's Bell 202 demodulator code is original for this project; no external AFSK library is vendored.


## FIX5 investigation (checked 2026-10-27 JST)

- ESP-IDF 5.5.5 ADC continuous mode (read blocking semantics, ISR callbacks, thread safety):
  https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/peripherals/adc_continuous.html
- ESP-IDF 5.5.5 FreeRTOS (priority scheduling, stack size/watermark units):
  https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/system/freertos_idf.html
- Espressif FPU explanation (C3 floating point is software):
  https://developer.espressif.com/blog/2025/10/cores_with_fpu/
- Arduino ESP32 3.3.12 sources downloaded from the official package archive:
  cores/esp32/main.cpp (loopTask priority 1), libraries/WiFi/src/WiFiGeneric.cpp (mode/init path).
  https://github.com/espressif/arduino-esp32/tree/3.3.12


## v0.4 primary sources

- ESP32 Arduino Core 3.3.12 Update library and WebServer multipart parsing, inspected locally from official release archive.
- https://github.com/espressif/arduino-esp32/tree/3.3.12/libraries/Update
- https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/WebServer/src/Parsing.cpp
- https://github.com/espressif/arduino-esp32/blob/3.3.12/tools/partitions/default.csv
- https://aprs-is.net/Connecting.aspx (originating packets: TCPIP* only)
- https://www.aprs-is.net/ (fixed station beacon rates)


## v0.7
- Official Arduino-ESP32 3.3.12 WebServer Parsing.cpp and Update Updater.cpp inspected locally.
- https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/WebServer/src/Parsing.cpp
- https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/Update/src/Updater.cpp
- https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/peripherals/adc_continuous.html

## v0.8 NTP / timing
- Local Core 3.3.12 cores/esp32/esp32-hal-time.c: configTime starts SNTP; getLocalTime wait loop is not used.
- https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/system/system_time.html
- Local SDK esp_sntp.h notification callback and esp_timer_get_time for monotonic 64-bit time.

## v0.9 OTA
- Local Core 3.3.12 libraries/Update/src/Updater.cpp: Update.write, end(false), abort.
- Local SDK mbedtls/sha256.h: init/starts/update/finish/free (mbedTLS 3 API).
- Local Core WiFiSTA implementation: setAutoReconnect and reconnect, without OTA loop mode-off resets.

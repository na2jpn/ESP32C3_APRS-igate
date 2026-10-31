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

#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "afsk1200.h"
#include "esp_adc/adc_continuous.h"
#include "hal/adc_types.h"
#include "soc/soc_caps.h"

struct AfMonitorSnapshot {
  uint16_t adcMin = 0;
  uint16_t adcMax = 0;
  float rms = 0;
  float peak5s = 0;
  float mark1200 = 0;
  float space2200 = 0;
  uint32_t sampleCount = 0;
  uint32_t crcBad = 0;
  uint32_t framesGood = 0;
  uint32_t frameErrors = 0;
  uint32_t readErrors = 0;
  uint32_t readTimeouts = 0;
  uint32_t poolOverflows = 0;
  uint32_t maxBlockUs = 0;
  uint32_t processingUs = 0;
  uint32_t stackFreeBytes = 0;
  uint32_t updatedAtMs = 0;
};

class AudioAdcCapture {
public:
  explicit AudioAdcCapture(Afsk1200Decoder &decoder) : _decoder(decoder) {}

  AfMonitorSnapshot monitor() {
    AfMonitorSnapshot copy;
    portENTER_CRITICAL(&_monitorMux);
    copy = _monitor;
    portEXIT_CRITICAL(&_monitorMux);
    return copy;
  }

  bool begin() {
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    return false;
#else
    if (_handle || _task) return false; // begin is setup-only, never race a live reader
    adc_continuous_handle_cfg_t handleCfg = {};
    handleCfg.max_store_buf_size = 8192; // ~77 ms at 26400 x 4 bytes
    handleCfg.conv_frame_size = 512;
    esp_err_t err = adc_continuous_new_handle(&handleCfg, &_handle);
    if (err != ESP_OK) {
      DBG_PRINTF("[ADC] new_handle failed: %s\n", esp_err_to_name(err));
      return false;
    }
    DBG_PRINTLN("[ADC] new_handle OK");

    adc_digi_pattern_config_t pattern = {};
#if defined(ADC_ATTEN_DB_12)
    pattern.atten = ADC_ATTEN_DB_12;
#elif defined(ADC_ATTEN_DB_11)
    pattern.atten = ADC_ATTEN_DB_11;
#else
    pattern.atten = static_cast<adc_atten_t>(3);
#endif
    pattern.channel = ADC_CHANNEL_3; // GPIO3 = ADC1_CH3 on ESP32-C3
    pattern.unit = ADC_UNIT_1;
    pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    adc_continuous_config_t cfg = {};
    cfg.sample_freq_hz = AFSK_SAMPLE_RATE;
    cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
    cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    cfg.pattern_num = 1;
    cfg.adc_pattern = &pattern;
    err = adc_continuous_config(_handle, &cfg);
    if (err != ESP_OK) {
      DBG_PRINTF("[ADC] config failed: %s\n", esp_err_to_name(err));
      adc_continuous_deinit(_handle);
      _handle = nullptr;
      return false;
    }
    DBG_PRINTLN("[ADC] config OK");
    adc_continuous_evt_cbs_t callbacks = {};
    callbacks.on_pool_ovf = poolOverflow;
    err = adc_continuous_register_event_callbacks(_handle, &callbacks, this);
    if (err != ESP_OK) {
      DBG_PRINTF("[ADC] callbacks failed: %s\n", esp_err_to_name(err));
      adc_continuous_deinit(_handle);
      _handle = nullptr;
      return false;
    }
    err = adc_continuous_start(_handle);
    if (err != ESP_OK) {
      DBG_PRINTF("[ADC] start failed: %s\n", esp_err_to_name(err));
      adc_continuous_deinit(_handle);
      _handle = nullptr;
      return false;
    }
    DBG_PRINTLN("[ADC] start OK");

    BaseType_t ok = xTaskCreate(taskThunk, "afsk_adc", 6144, this, 1, &_task);
    if (ok != pdPASS) {
      DBG_PRINTLN("[ADC] task creation failed");
      adc_continuous_stop(_handle);
      adc_continuous_deinit(_handle);
      _handle = nullptr;
      return false;
    }
    DBG_PRINTLN("[ADC] task created: priority=1 stack=6144 pool=8192, bounded work + 1 tick sleep");
    return true;
#endif
  }

private:
  Afsk1200Decoder &_decoder;
  adc_continuous_handle_t _handle = nullptr;
  TaskHandle_t _task = nullptr;
  portMUX_TYPE _monitorMux = portMUX_INITIALIZER_UNLOCKED;
  AfMonitorSnapshot _monitor{};
  float _peakBins[20]{}; // 20 x 250 ms = 5 seconds
  uint8_t _peakPos = 0;
  uint32_t _windowSamples = 0;
  uint16_t _windowMin = 4095;
  uint16_t _windowMax = 0;
  float _windowPeak = 0;
  uint32_t _totalSamples = 0;
  uint32_t _readErrors = 0;
  uint32_t _readTimeouts = 0;
  uint32_t _poolOverflows = 0;
  portMUX_TYPE _overflowMux = portMUX_INITIALIZER_UNLOCKED;
  uint32_t _maxBlockUs = 0;
  uint32_t _processingUs = 0;

  // ISR: counter only, protected by an ISR-safe critical section.
  static bool IRAM_ATTR poolOverflow(adc_continuous_handle_t,
                                    const adc_continuous_evt_data_t *, void *arg) {
    auto *self = static_cast<AudioAdcCapture *>(arg);
    portENTER_CRITICAL_ISR(&self->_overflowMux);
    ++self->_poolOverflows;
    portEXIT_CRITICAL_ISR(&self->_overflowMux);
    return false;
  }

  uint32_t overflowCount() {
    portENTER_CRITICAL(&_overflowMux);
    const uint32_t count = _poolOverflows;
    portEXIT_CRITICAL(&_overflowMux);
    return count;
  }

  void publishHealth() {
    const uint32_t overflow = overflowCount();
    const uint32_t stack = uxTaskGetStackHighWaterMark(nullptr);
    portENTER_CRITICAL(&_monitorMux);
    _monitor.readErrors = _readErrors;
    _monitor.readTimeouts = _readTimeouts;
    _monitor.poolOverflows = overflow;
    _monitor.stackFreeBytes = stack;
    portEXIT_CRITICAL(&_monitorMux);
  }

  void observe(uint16_t raw) {
    if (raw < _windowMin) _windowMin = raw;
    if (raw > _windowMax) _windowMax = raw;
    // sqrt is monotonic: hold squared values, take roots only on publication.
    const float energy = _decoder.meanSquare();
    if (energy > _windowPeak) _windowPeak = energy;
    ++_windowSamples;
    ++_totalSamples;
    if (_windowSamples >= AFSK_SAMPLE_RATE / 4) {
      _peakBins[_peakPos] = _windowPeak;
      _peakPos = (_peakPos + 1) % 20;
      float peak = 0;
      for (float v : _peakBins) if (v > peak) peak = v;
      AfMonitorSnapshot next;
      next.adcMin = _windowMin;
      next.adcMax = _windowMax;
      next.rms = sqrtf(energy);
      next.peak5s = sqrtf(peak);
      next.mark1200 = _decoder.markLevel();
      next.space2200 = _decoder.spaceLevel();
      next.sampleCount = _totalSamples;
      next.crcBad = _decoder.crcBad();
      next.framesGood = _decoder.framesGood();
      next.frameErrors = _decoder.frameErrors();
      next.readErrors = _readErrors;
      next.readTimeouts = _readTimeouts;
      next.poolOverflows = overflowCount();
      next.maxBlockUs = _maxBlockUs;
      next.processingUs = _processingUs;
      // ESP-IDF's stack watermark is bytes (not vanilla FreeRTOS words).
      next.stackFreeBytes = uxTaskGetStackHighWaterMark(nullptr);
      next.updatedAtMs = millis();
      // Only a fixed-size copy with interrupts disabled. All math is above.
      portENTER_CRITICAL(&_monitorMux);
      _monitor = next;
      portEXIT_CRITICAL(&_monitorMux);
      _windowSamples = 0;
      _windowMin = 4095;
      _windowMax = 0;
      _windowPeak = 0;
    }
  }

  static void taskThunk(void *arg) {
    static_cast<AudioAdcCapture *>(arg)->taskLoop();
  }

  void taskLoop() {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    alignas(4) uint8_t buffer[512];
    unsigned blocks = 0;
    uint32_t seenOverflows = 0;
    while (true) {
      uint32_t outLen = 0;
      esp_err_t err = adc_continuous_read(_handle, buffer, sizeof(buffer), &outLen, 1000);
      const uint32_t overflows = overflowCount();
      if (overflows != seenOverflows) {
        seenOverflows = overflows;
        _decoder.resetAll();
      }
      if (err == ESP_OK) {
        const uint32_t started = micros();
        for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= outLen; i += SOC_ADC_DIGI_RESULT_BYTES) {
          const adc_digi_output_data_t *p = reinterpret_cast<const adc_digi_output_data_t *>(&buffer[i]);
          if (p->type2.channel == ADC_CHANNEL_3 && p->type2.unit == 0) {
            const uint16_t raw = static_cast<uint16_t>(p->type2.data);
            _decoder.processRaw(raw);
            observe(raw);
          }
        }
        const uint32_t elapsed = micros() - started;
        _processingUs += elapsed;
        if (elapsed > _maxBlockUs) _maxBlockUs = elapsed;
      } else if (err == ESP_ERR_TIMEOUT) {
        ++_readTimeouts;
        _decoder.resetAll();
        publishHealth();
      } else {
        ++_readErrors;
        // A discontinuity must never join two unrelated HDLC fragments.
        _decoder.resetAll();
        publishHealth();
      }
      // yield() alone does not let lower-priority tasks run. A real blocked
      // interval also allows IDLE/WDT to run when a DMA backlog never empties.
      if (err != ESP_OK || ++blocks >= 4) {
        blocks = 0;
        vTaskDelay(1);
      }
    }
#endif
  }
};

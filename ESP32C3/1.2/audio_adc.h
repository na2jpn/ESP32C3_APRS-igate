#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "afsk1200.h"
#include "rx_diagnostics.h"
#include "audio_scope.h"
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
  uint32_t capturedSamples=0,discardedSamples=0,lastDataAtMs=0;
  uint32_t recoveryAttempts=0,recoveries=0;
  float captureRate=0;
  bool dataSeen=false,driverRunning=false,recovering=false,fault=false;
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

  void setMaintenance(bool enabled) {
    portENTER_CRITICAL(&_monitorMux); _maintenance=enabled; portEXIT_CRITICAL(&_monitorMux);
  }
  bool maintenance() {
    portENTER_CRITICAL(&_monitorMux); bool enabled=_maintenance; portEXIT_CRITICAL(&_monitorMux); return enabled;
  }

  // HTTP snapshots are copied into heap storage supplied by the caller.
  void copyDiagnostics(RxDiagnostics &out) {
    portENTER_CRITICAL(&_diagMux);out=_diagnostics;portEXIT_CRITICAL(&_diagMux);
  }
  void controlDiagnostics(unsigned action) {
    portENTER_CRITICAL(&_diagMux);
    if(action==0) _diagnostics.frozen=true;
    if(action==1) { _diagnostics.pause(); _diagnostics.frozen=false; }
    if(action==2) _diagnostics.clear();
    if(action==3) _diagnostics.manual();
    portEXIT_CRITICAL(&_diagMux);
  }

  void copyScope(AudioScopeFrame &out){_scope.requestAndCopy(out,millis());}

  bool requestRecovery() {
    if(!_task||!_handle)return false;
    portENTER_CRITICAL(&_monitorMux);_requestRecovery=true;portEXIT_CRITICAL(&_monitorMux);return true;
  }

  bool begin() {
#if !defined(CONFIG_IDF_TARGET_ESP32C3)
    return false;
#else
    if (_handle || _task) return false; // begin is setup-only, never race a live reader
    adc_continuous_handle_cfg_t handleCfg = {};
    handleCfg.max_store_buf_size = 16384; // ~155 ms at 26400 x 4 bytes
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

    _driverRunning=true;_lastDataMs=millis();_lastRecoveryMs=millis();
    BaseType_t ok = xTaskCreate(taskThunk, "afsk_adc", 6144, this, 2, &_task);
    if (ok != pdPASS) {
      DBG_PRINTLN("[ADC] task creation failed");
      adc_continuous_stop(_handle);
      _driverRunning=false;
      adc_continuous_deinit(_handle);
      _handle = nullptr;
      return false;
    }
    DBG_PRINTLN("[ADC] task created: priority=2 stack=6144 pool=16384, bounded work + 1 tick sleep");
    return true;
#endif
  }

private:
  Afsk1200Decoder &_decoder;
  AudioScope _scope;
  adc_continuous_handle_t _handle = nullptr;
  TaskHandle_t _task = nullptr;
  portMUX_TYPE _monitorMux = portMUX_INITIALIZER_UNLOCKED;
  AfMonitorSnapshot _monitor{};
  bool _maintenance=false,_requestRecovery=false;
  bool _driverRunning=false,_recovering=false,_fault=false,_dataSeen=false,_validRun=false;
  uint32_t _capturedSamples=0,_discardedSamples=0,_lastDataMs=0,_lastRecoveryMs=0;
  uint32_t _recoveryAttempts=0,_recoveries=0,_stableSince=0;
  uint32_t _rateAt=0,_rateSamples=0,_healthAt=0;
  unsigned _stallAttempts=0;
  float _captureRate=0;
  bool takeRecoveryRequest(){portENTER_CRITICAL(&_monitorMux);bool r=_requestRecovery;_requestRecovery=false;portEXIT_CRITICAL(&_monitorMux);return r;}
  void resetWindows(bool pauseDiagnostic=true){
    _scope.invalidate();
    _decoder.resetAll();_windowSamples=0;_windowMin=4095;_windowMax=0;_windowPeak=0;
    for(float &v:_peakBins)v=0;
    _diagSamples=0;_diagEnergy=0;_diagMin=4095;_diagMax=0;
    if(pauseDiagnostic){portENTER_CRITICAL(&_diagMux);_diagnostics.pause();portEXIT_CRITICAL(&_diagMux);}
  }
  void captured(unsigned n){
    if(!n)return;
    uint32_t now=millis();_capturedSamples+=n;_lastDataMs=now;
    if(!_validRun){_validRun=true;_stableSince=now;}
    _dataSeen=true;
    if(_recovering||_fault){++_recoveries;_recovering=false;_fault=false;
      DBG_PRINTF("[ADC] data resumed captured=%lu recovery=%lu\n",(unsigned long)_capturedSamples,(unsigned long)_recoveries);}
    if(_stallAttempts&&uint32_t(now-_stableSince)>=30000)_stallAttempts=0;
    if(uint32_t(now-_rateAt)>=1000){_captureRate=float(uint32_t(_capturedSamples-_rateSamples))*1000.0f/uint32_t(now-_rateAt);_rateAt=now;_rateSamples=_capturedSamples;}
  }
  void recover(const char *reason){
    (void)reason; // Logging may be compiled out in host regression builds.
    ++_recoveryAttempts;++_stallAttempts;_lastRecoveryMs=millis();_validRun=false;_recovering=true;
    DBG_PRINTF("[ADC] recovery attempt=%lu burst=%u reason=%s\n",(unsigned long)_recoveryAttempts,_stallAttempts,reason);
    // Only this ADC-owning task may touch driver state. The pinned 5.5.5 driver
    // handles C3 EOF-counter reset/clear in stop/start; never reset registers here.
    esp_err_t err=ESP_OK;
    if(_driverRunning){err=adc_continuous_stop(_handle);if(err==ESP_OK)_driverRunning=false;}
    if(err==ESP_OK)err=adc_continuous_flush_pool(_handle);
    resetWindows();
    if(err==ESP_OK){err=adc_continuous_start(_handle);if(err==ESP_OK)_driverRunning=true;}
    if(err!=ESP_OK){DBG_PRINTF("[ADC] recovery API failed: %s\n",esp_err_to_name(err));}
    else DBG_PRINTLN("[ADC] restarted; waiting for actual samples");
    publishHealth();
  }
  void checkRecovery(bool paused){
    if(paused)return; // OTA keeps exclusive control of its maintenance interval.
    bool manual=takeRecoveryRequest();uint32_t now=millis();
    if(manual){_stallAttempts=0;_fault=false;recover("administrator request");return;}
    if(uint32_t(now-_lastDataMs)<3000||uint32_t(now-_lastRecoveryMs)<3000)return;
    if(_stallAttempts<3){recover("no valid ADC samples for 3 s");return;}
    if(!_fault){_fault=true;_recovering=false;DBG_PRINTLN("[ADC] FAULT: 3 attempts without stable recovery; Web recovery or Reboot required");publishHealth();}
  }

  portMUX_TYPE _diagMux=portMUX_INITIALIZER_UNLOCKED;
  RxDiagnostics _diagnostics;
  unsigned _diagSamples=0;
  float _diagEnergy=0;
  uint16_t _diagMin=4095,_diagMax=0;
  void diagnosticReadFailure() {
    RxDiagnosticRow r;
    r.time=deviceClock.now();r.samples=_totalSamples;r.crc=_decoder.crcBad();r.good=_decoder.framesGood();
    r.errors=_decoder.frameErrors();r.overflows=overflowCount();r.readErrors=_readErrors;r.timeouts=_readTimeouts;
    r.adcValid=false;
    r.adcMin=r.adcMax=2048; // No valid samples in this row; sample count stays unchanged.
    portENTER_CRITICAL(&_diagMux);_diagnostics.feed(r,RX_ADC);portEXIT_CRITICAL(&_diagMux);
    _diagSamples=0;_diagEnergy=0;_diagMin=4095;_diagMax=0;
  }
  void observeDiagnostic(uint16_t raw,float energy) {
    _diagEnergy+=energy;
    if(raw<_diagMin)_diagMin=raw;
    if(raw>_diagMax)_diagMax=raw;
    if(++_diagSamples<AFSK_SAMPLE_RATE/50)return;
    RxDiagnosticRow r;
    r.time=deviceClock.now();r.rms=sqrtf(_diagEnergy/_diagSamples);
    r.mark=_decoder.markLevel();r.space=_decoder.spaceLevel();
    r.samples=_totalSamples;r.crc=_decoder.crcBad();r.good=_decoder.framesGood();
    r.errors=_decoder.frameErrors();r.overflows=overflowCount();
    r.readErrors=_readErrors;r.timeouts=_readTimeouts;
    r.adcMin=_diagMin;r.adcMax=_diagMax;
    portENTER_CRITICAL(&_diagMux);_diagnostics.feed(r);portEXIT_CRITICAL(&_diagMux);
    _diagSamples=0;_diagEnergy=0;_diagMin=4095;_diagMax=0;
  }

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
    _monitor.capturedSamples=_capturedSamples;_monitor.discardedSamples=_discardedSamples;
    _monitor.lastDataAtMs=_lastDataMs;_monitor.dataSeen=_dataSeen;
    _monitor.driverRunning=_driverRunning;_monitor.recovering=_recovering;_monitor.fault=_fault;
    _monitor.recoveryAttempts=_recoveryAttempts;_monitor.recoveries=_recoveries;
    _monitor.captureRate=_captureRate;
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
    observeDiagnostic(raw,energy);
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
      next.capturedSamples=_capturedSamples;next.discardedSamples=_discardedSamples;
      next.lastDataAtMs=_lastDataMs;next.dataSeen=_dataSeen;
      next.driverRunning=_driverRunning;next.recovering=_recovering;next.fault=_fault;
      next.recoveryAttempts=_recoveryAttempts;next.recoveries=_recoveries;next.captureRate=_captureRate;
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
    unsigned blocks=0;uint32_t seenOverflows=overflowCount();bool wasMaintenance=false;
    while(true){
      bool paused=maintenance();
      if(paused!=wasMaintenance){resetWindows();_validRun=false;wasMaintenance=paused;}
      checkRecovery(paused);
      uint32_t outLen=0;
      esp_err_t err;
      if(_driverRunning)err=adc_continuous_read(_handle,buffer,sizeof(buffer),&outLen,100);
      else {vTaskDelay(pdMS_TO_TICKS(100));err=ESP_ERR_TIMEOUT;}
      // Maintenance may have changed while read was blocked.
      paused=maintenance();
      if(paused!=wasMaintenance){resetWindows();_validRun=false;wasMaintenance=paused;}
      uint32_t overflow=overflowCount();bool gap=overflow!=seenOverflows;
      if(gap){seenOverflows=overflow;resetWindows();_validRun=false;}
      const uint32_t scopeNow=millis();
      _scope.beginBlock(scopeNow,paused||gap||err!=ESP_OK);
      if(err==ESP_OK&&outLen&&outLen<=sizeof(buffer)&&outLen%SOC_ADC_DIGI_RESULT_BYTES==0){
        uint32_t started=micros();unsigned valid=0;
        for(uint32_t i=0;i<outLen;i+=SOC_ADC_DIGI_RESULT_BYTES){
          const auto *v=reinterpret_cast<const adc_digi_output_data_t *>(&buffer[i]);
          if(v->type2.channel==ADC_CHANNEL_3&&v->type2.unit==0){
            ++valid;
            if(!paused&&!gap){_decoder.processRaw(v->type2.data);observe(v->type2.data);_scope.feed(v->type2.data,scopeNow);}
          }
        }
        captured(valid);
        if(paused||gap)_discardedSamples+=valid;
        if(!valid){++_readErrors;_validRun=false;diagnosticReadFailure();resetWindows();}
        // Overflow means a hole exists in the queue. Discard its bounded backlog
        // without DSP, then restart frame sync. Never stitch across missing audio.
        if(gap){
          for(unsigned n=0;n<32;++n){
            uint32_t len=0;auto drain=adc_continuous_read(_handle,buffer,sizeof(buffer),&len,0);
            if(drain!=ESP_OK||!len)break;
            unsigned count=0;
            for(uint32_t i=0;i+SOC_ADC_DIGI_RESULT_BYTES<=len&&i+SOC_ADC_DIGI_RESULT_BYTES<=sizeof(buffer);i+=SOC_ADC_DIGI_RESULT_BYTES){
              const auto *v=reinterpret_cast<const adc_digi_output_data_t *>(&buffer[i]);if(v->type2.channel==ADC_CHANNEL_3&&v->type2.unit==0)++count;
            }
            captured(count);_discardedSamples+=count;
          }
          resetWindows();
        }
        uint32_t elapsed=micros()-started;_processingUs+=elapsed;if(elapsed>_maxBlockUs)_maxBlockUs=elapsed;
      }else{
        if(_driverRunning){if(err==ESP_ERR_TIMEOUT)++_readTimeouts;else ++_readErrors;}
        _validRun=false;
        if(!paused)diagnosticReadFailure();
        resetWindows(false);
      }
      if(uint32_t(millis()-_healthAt)>=100||err!=ESP_OK||gap){_healthAt=millis();publishHealth();}
      // Even with priority 2 and a continuous backlog, lower-priority loop/IDLE
      // get a real blocked tick after at most four processed buffers.
      if(paused||err!=ESP_OK||++blocks>=4){blocks=0;vTaskDelay(1);}
    }
#endif
  }
};

#pragma once
#include <Arduino.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#if defined(ARDUINO)
#include "esp_timer.h"
#endif

struct DiagnosticTime { uint64_t uptimeMs=0, unixMs=0; };
class DeviceClock {
public:
  static uint64_t uptime() {
#if defined(ARDUINO)
    return uint64_t(esp_timer_get_time())/1000;
#else
    return millis();
#endif
  }
  void sync(uint64_t up, uint64_t epoch) {
    if(epoch<1704067200000ULL) return;
    portENTER_CRITICAL(&_mux);_up=up;_epoch=epoch;++_generation;portEXIT_CRITICAL(&_mux);
  }
  DiagnosticTime at(uint64_t up) {
    portENTER_CRITICAL(&_mux);uint64_t a=_up,e=_epoch;portEXIT_CRITICAL(&_mux);
    return {up,e && up>=a ? e+up-a : 0};
  }
  DiagnosticTime now(){return at(uptime());}
  uint32_t generation(){portENTER_CRITICAL(&_mux);auto g=_generation;portEXIT_CRITICAL(&_mux);return g;}
  uint64_t lastSync(){portENTER_CRITICAL(&_mux);auto u=_up;portEXIT_CRITICAL(&_mux);return u;}
  static void format(const DiagnosticTime &t,char *out,size_t size) {
    if(!t.unixMs){snprintf(out,size,"UNSYNC +%llu ms",(unsigned long long)t.uptimeMs);return;}
    time_t sec=time_t(t.unixMs/1000+9*3600);struct tm tm{};gmtime_r(&sec,&tm);
    char date[24];strftime(date,sizeof(date),"%Y-%m-%d %H:%M:%S",&tm);
    snprintf(out,size,"%s.%03u JST +%llu ms",date,unsigned(t.unixMs%1000),(unsigned long long)t.uptimeMs);
  }
private:
  portMUX_TYPE _mux=portMUX_INITIALIZER_UNLOCKED;
  uint64_t _up=0,_epoch=0;uint32_t _generation=0;
};
inline DeviceClock deviceClock;

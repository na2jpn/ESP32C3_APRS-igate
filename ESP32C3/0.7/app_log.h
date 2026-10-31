#pragma once
#include <Arduino.h>
#include <cstdarg>
#include <cstring>
#include "freertos/FreeRTOS.h"

// Fixed RAM storage, complete lines only; never called from an ISR.
class AppLog {
public:
  static constexpr size_t CAPACITY = 4096;
  void add(const char *message) {
    char row[256];
    int n = snprintf(row, sizeof(row), "[%lu ms] %s", (unsigned long)millis(), message);
    size_t len = n < 0 ? 0 : (size_t)n;
    if (len > sizeof(row)-2) len = sizeof(row)-2;
    while (len && (row[len-1]=='\n' || row[len-1]=='\r')) --len;
    for (size_t i=0;i<len;++i) if (row[i]=='\n' || row[i]=='\r') row[i]=' ';
    row[len++]='\n';
    portENTER_CRITICAL(&_mux);
    while (_used + len > CAPACITY) {
      char c;
      do { c=_data[_head]; _head=(_head+1)%CAPACITY; --_used; ++_dropped; } while (_used && c!='\n');
    }
    for (size_t i=0;i<len;++i) { _data[(_head+_used)%CAPACITY]=row[i]; ++_used; }
    portEXIT_CRITICAL(&_mux);
  }
  void printf(const char *format, ...) {
    // High-volume RF/health streams stay on serial, not in the event log.
    if (!strncmp(format,"[HEALTH]",8) || !strncmp(format,"RF:",3) || !strncmp(format,"IS:",3)) return;
    char text[224]; va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);add(text);
  }
  String snapshot() {
    char copy[CAPACITY+1]; size_t n;
    portENTER_CRITICAL(&_mux);
    n=_used;for(size_t i=0;i<n;++i)copy[i]=_data[(_head+i)%CAPACITY];
    portEXIT_CRITICAL(&_mux);
    copy[n]=0;return String(copy);
  }
  size_t used() {portENTER_CRITICAL(&_mux);size_t n=_used;portEXIT_CRITICAL(&_mux);return n;}
  void clear(){portENTER_CRITICAL(&_mux);_head=0;_used=0;_dropped=0;portEXIT_CRITICAL(&_mux);}
private:
  char _data[CAPACITY]{};
  size_t _head=0,_used=0,_dropped=0;
  portMUX_TYPE _mux=portMUX_INITIALIZER_UNLOCKED;
};
inline AppLog appLog;

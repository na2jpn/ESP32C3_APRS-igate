#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cassert>
#include <algorithm>
#define IRAM_ATTR
using TaskHandle_t=void*;
using BaseType_t=int;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) (++*(p))
#define portEXIT_CRITICAL(p) (--*(p))
#define portENTER_CRITICAL_ISR(p) portENTER_CRITICAL(p)
#define portEXIT_CRITICAL_ISR(p) portEXIT_CRITICAL(p)
static constexpr int pdPASS=1;
inline uint32_t fakeUs=0;
inline uint32_t micros(){return ++fakeUs;}
inline uint32_t millis(){return fakeUs/1000;}
inline int yields=0, taskPriority=0, taskStack=0;
inline bool taskFail=false;
inline void vTaskDelay(int n){assert(n==1);++yields;}
inline uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t){return 3000;}
inline BaseType_t xTaskCreate(void(*)(void*),const char*,int stack,void*,int priority,TaskHandle_t* t){taskPriority=priority;taskStack=stack;*t=taskFail?nullptr:(void*)1;return taskFail?0:pdPASS;}
struct SerialMock{void println(const char*){} template<class...T>void printf(const char*,T...){};};
inline SerialMock Serial;

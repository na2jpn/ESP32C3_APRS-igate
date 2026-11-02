#include <Arduino.h>
#include <cmath>
#include <iostream>
#define private public
#include "audio_adc.h"
#undef private
static void received(const uint8_t*,size_t){}
static void at(uint32_t ms){fakeUs=ms*1000;}
static AudioAdcCapture *active;
static int hookReads=0,drainReads=0;
static int overflowRead(uint8_t*b,uint32_t size,uint32_t*len,int timeout){
 if(timeout==0){++drainReads;}else if(++hookReads>1)throw EndRead{};
 if(timeout!=0)AudioAdcCapture::poolOverflow(nullptr,nullptr,active);
 *len=size;for(unsigned i=0;i<size;i+=4){adc_digi_output_data_t v{};v.type2.channel=3;v.type2.data=2048;__builtin_memcpy(b+i,&v,4);}return ESP_OK;
}
int main(){
 Afsk1200Decoder decoder(received);decoder.begin();AudioAdcCapture adc(decoder);
 at(0);assert(adc.begin());int oldStarts=starts;
 // HTTP-side request queues only: no driver call until owning task checks it.
 assert(adc.requestRecovery());assert(starts==oldStarts);
 adc.checkRecovery(true);assert(starts==oldStarts);assert(adc._requestRecovery);
 adc.checkRecovery(false);assert(starts==oldStarts+1&&stops==1&&flushes==1);
 assert(adc.monitor().recovering&&adc.monitor().recoveries==0);
 at(10);adc.captured(128);adc.publishHealth();assert(adc.monitor().recoveries==1&&!adc.monitor().recovering);
 // Three attempts at least 3 s apart, then FAULT. API success is not data success.
 at(3010);adc.checkRecovery(false);assert(adc._stallAttempts==2);
 at(6010);adc.checkRecovery(false);assert(adc._stallAttempts==3);
 at(9010);adc.checkRecovery(false);assert(adc.monitor().fault&&!adc.monitor().recovering);
 oldStarts=starts;at(12010);adc.checkRecovery(false);assert(starts==oldStarts);
 // Manual recovery replenishes exhausted budget; stable data clears burst count.
 assert(adc.requestRecovery());adc.checkRecovery(false);assert(adc._stallAttempts==1);
 at(12020);adc.captured(128);at(42020);adc.captured(128);assert(adc._stallAttempts==0);
 // Stop error: never flush/start a running handle. No deinit/recreate fallback.
 stopError=2;assert(adc.requestRecovery());oldStarts=starts;int oldFlush=flushes;
 adc.checkRecovery(false);assert(starts==oldStarts&&flushes==oldFlush&&adc._driverRunning);
 stopError=0;flushError=2;assert(adc.requestRecovery());adc.checkRecovery(false);
 assert(!adc._driverRunning&&starts==oldStarts);
 flushError=0;startError=2;assert(adc.requestRecovery());adc.checkRecovery(false);assert(!adc._driverRunning);
 startError=0;assert(adc.requestRecovery());adc.checkRecovery(false);assert(adc._driverRunning);
 // Unsigned elapsed-time logic across millis wrap (within representable fake clock).
 adc._lastDataMs=UINT32_MAX-1000;adc._lastRecoveryMs=UINT32_MAX-1000;adc._stallAttempts=0;
 at(2100);adc.checkRecovery(false);assert(adc._stallAttempts==1);
 // Failed ADC rows carry validity=false, never become a measured signal baseline.
 adc.diagnosticReadFailure();auto &event=adc._diagnostics.events[(adc._diagnostics.total-1)%RxDiagnostics::EVENTS];
 assert(event.count>0&&!event.rows[event.count-1].adcValid);
 // A continuously refilled overflow queue is drained at most 32 blocks, with
 // no DSP across the gap and a mandatory yield before the next read.
 AudioAdcCapture queue(decoder);at(0);assert(queue.begin());active=&queue;
 readHook=overflowRead;yields=0;try{queue.taskLoop();}catch(EndRead&){}
 readHook=nullptr;assert(drainReads==32&&queue._totalSamples==0);
 assert(queue._discardedSamples==33*128&&queue._capturedSamples==33*128);

 std::cout<<"PASS: ADC ownership/maintenance, actual-data confirmation, retry cap, manual recovery, stop/flush/start failures, elapsed wrap, invalid ADC record\n";
}

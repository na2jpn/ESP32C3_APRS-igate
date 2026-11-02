#include <Arduino.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <cmath>
#define private public
#include "audio_adc.h"
#define Afsk1200Decoder Fix3Decoder
#include "reference/FIX3_afsk1200.h"
#undef Afsk1200Decoder
#undef private
int main(){
 RxDiagnostics d;RxDiagnosticRow r;r.rms=.1f;r.mark=.04f;r.space=.04f;r.adcMin=1700;r.adcMax=2700;
 auto feed=[&](){r.time.uptimeMs+=20;r.samples+=528;d.feed(r);};
 for(int i=0;i<100;i++)feed();assert(d.total==0);
 r.rms=.05f;feed();assert(d.total==1&&(d.events[0].reason&RX_RMS));
 assert(d.events[0].triggerIndex==8&&d.events[0].count==9);
 assert(d.events[0].rows[0].time.uptimeMs==1860);
 for(int i=0;i<39;i++)feed();assert(d.events[0].complete&&d.events[0].count==48);
 d.frozen=true;auto total=d.total;for(int i=0;i<200;i++)feed();assert(d.total==total);
 d.clear();assert(!d.frozen&&d.total==0);
 for(int event=0;event<7;event++){d.manual();feed();for(int i=0;i<70;i++)feed();}
 assert(d.total==7&&d.overwritten==3);assert(d.events[6%4].id==7);
 d.clear();feed();r.good++;feed();assert(d.total==1&&(d.events[0].reason&RX_GOOD));
 d.clear();feed();r.crc++;feed();assert(d.total==1&&(d.events[0].reason&RX_CRC));
 d.clear();feed();r.errors++;feed();assert(d.events[0].reason&RX_ADC);
 d.clear();feed();r.overflows++;feed();assert(d.events[0].reason&RX_ADC);
 d.clear();feed();r.readErrors++;feed();assert(d.events[0].reason&RX_ADC);
 d.clear();feed();r.timeouts++;feed();assert(d.events[0].reason&RX_ADC);
 d.clear();r.adcMin=0;feed();assert(d.events[0].reason&RX_CLIP);r.adcMin=1700;
 d.clear();for(int i=0;i<100;i++)feed();r.rms=.15f;feed();assert(d.events[0].reason&RX_RMS);
 d.clear();for(int i=0;i<100;i++)feed();r.mark=.09f;feed();assert(d.events[0].reason&RX_TONES);
 d.clear();r.mark=.04f;r.space=.04f;for(int i=0;i<100;i++)feed();r.mark=.075f;r.space=.005f;feed();assert(d.events[0].reason&RX_BALANCE);
 d.pause();assert(d._active<0&&d._preCount==0);
 // 64-bit monotonic uptime survives millis rollover. Stored pre-sync rows stay unsynced.
 DeviceClock c;char out[120];auto before=c.at(100);assert(!before.unixMs);
 c.sync(0x100000010ULL,1793491200123ULL);auto t=c.at(0x100000024ULL);
 assert(t.unixMs==1793491200143ULL&&t.uptimeMs==0x100000024ULL);
 DeviceClock::format(t,out,sizeof(out));assert(strstr(out,"2026-11-01")&&strstr(out,"JST"));
 DeviceClock::format(before,out,sizeof(out));assert(strstr(out,"UNSYNC"));
 c.sync(0x100000030ULL,1793491190000ULL);assert(c.at(0x100000040ULL).unixMs==1793491190016ULL);assert(t.unixMs==1793491200143ULL);
 // Concrete comparison against the prior gate: weak input accepted only in v0.8.
 Afsk1200Decoder a(nullptr);Fix3Decoder b(nullptr);a.begin();b.begin();
 a._dcInitialized=b._dcInitialized=true;a._dc=b._dc=2048;a._rms=b._rms=9e-6f;a._mi=b._mi=.002f;
 a.processRaw(2048);b.processRaw(2048);assert(a._silenceSamples==0&&b._silenceSamples==1);
 a.resetAll();b.resetAll();a._dcInitialized=b._dcInitialized=true;a._dc=b._dc=2048;a._rms=b._rms=2e-5f;a._mi=b._mi=sqrtf(2.5e-6f);
 a.processRaw(2048);b.processRaw(2048);assert(a._silenceSamples==0&&b._silenceSamples==1);
 AudioAdcCapture audio(a);audio.diagnosticReadFailure();audio._readErrors++;audio.diagnosticReadFailure();
 assert(audio._diagnostics.total==1&&(audio._diagnostics.events[0].reason&RX_ADC));
 audio.controlDiagnostics(0);assert(audio._diagnostics.frozen);audio.controlDiagnostics(1);assert(!audio._diagnostics.frozen);
 audio.controlDiagnostics(2);assert(!audio._diagnostics.total);
 std::cout<<"PASS: RX baseline/up/down/tone/balance/clipping/counters/pre-post/ring/freeze/manual/ADC error; clock unsynced/JST/64-bit/correction; both weak gates vs FIX3. Diagnostic RAM="<<sizeof(RxDiagnostics)<<" bytes\n";
}

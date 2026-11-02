#include <Arduino.h>
#include <cmath>
#include <iostream>
#define private public
#include "audio_adc.h"
#undef private
int main(){
 AudioScope scope;AudioScopeFrame frame;
 scope.beginBlock(100,false);for(int i=0;i<1100;i++)scope.feed(4095,100);
 scope.requestAndCopy(frame,100);assert(!frame.valid);
 scope.beginBlock(100,false);
 for(unsigned i=0;i<AudioScopeFrame::SAMPLES;i++)scope.feed(2700,100);
 scope.requestAndCopy(frame,101);assert(frame.valid&&frame.sequence==1&&frame.rms==0&&frame.mean==2700&&frame.peak==0);
 for(unsigned i=0;i<AudioScopeFrame::BINS;i++)assert(frame.low[i]==2700&&frame.high[i]==2700);
 scope.invalidate();scope.beginBlock(102,false);
 for(unsigned i=0;i<AudioScopeFrame::SAMPLES;i++)scope.feed(i%4==0?1500:i%4==1?2500:2000,102);
 scope.requestAndCopy(frame,103);assert(frame.valid&&frame.mean==2000&&fabs(frame.rms-sqrt(125000))<.01&&frame.peak==500);
 for(unsigned i=0;i<AudioScopeFrame::BINS;i++)assert(frame.low[i]==1500&&frame.high[i]==2500);
 // High DC offset with tiny AC: retain sub-code variance without cancellation.
 scope.invalidate();for(unsigned i=0;i<AudioScopeFrame::SAMPLES;i++)scope.feed(i%2?4000:4001,104);
 scope.requestAndCopy(frame,105);assert(fabs(frame.rms-.5)<.001&&frame.clipped==0);
 scope.invalidate();for(unsigned i=0;i<AudioScopeFrame::SAMPLES;i++)scope.feed(i%2?0:4095,106);
 scope.requestAndCopy(frame,107);assert(frame.clipped==AudioScopeFrame::SAMPLES&&frame.minimum==0&&frame.maximum==4095);
 scope.beginBlock(108,true);scope.requestAndCopy(frame,109);assert(!frame.valid); // OTA
 for(unsigned i=0;i<AudioScopeFrame::SAMPLES;i++)scope.feed(2048,110);
 scope.requestAndCopy(frame,111);assert(!frame.valid);
 scope.beginBlock(3111,false);assert(!scope._active); // Lease expires even on exact boundary.
 scope.requestAndCopy(frame,0xfffffff0u);scope.beginBlock(20,false);assert(scope._active);
 for(int i=0;i<500;i++)scope.feed(2048,20);
 scope.invalidate();assert(scope._count==0); // No frame stitched across loss.
 for(int i=0;i<556;i++)scope.feed(2048,21);scope.requestAndCopy(frame,21);assert(!frame.valid);
 // Actual capture task feeds the viewer without changing decoder sample count.
 Afsk1200Decoder decoder(nullptr);decoder.begin();AudioAdcCapture adc(decoder);fakeUs=0;
 assert(adc.begin());adc.copyScope(frame);reads=0;try{adc.taskLoop();}catch(EndRead&){}
 adc.copyScope(frame);assert(frame.valid&&frame.rms==0&&adc._totalSamples==9*128);
 adc.resetWindows();adc.copyScope(frame);assert(!frame.valid);
 // Pulse entirely between HTTP polls must survive on the device peak.
 AudioScope meter;meter.requestAndCopy(frame,0);meter.beginBlock(0,false);
 auto block=[&](uint32_t time,unsigned amplitude){
   for(unsigned i=0;i<AudioScopeFrame::SAMPLES;i++)meter.feed(i%2?2048-amplitude:2048+amplitude,time);
 };
 for(unsigned n=1;n<=25;++n)block(n*40,n==2?1000:0);
 meter.requestAndCopy(frame,1000);
 assert(frame.rms==0&&frame.meterRms==0&&frame.meterPeak5s==1000&&frame.meterWindows==25);
 // Reading by another viewer must not consume or clear the held peak.
 AudioScopeFrame again;meter.requestAndCopy(again,1000);assert(again.meterPeak5s==1000);
 for(unsigned n=26;n<=127;++n)block(n*40,0);
 meter.requestAndCopy(frame,5080);assert(frame.meterPeak5s==1000); // exact 5-second boundary
 block(5120,0);meter.requestAndCopy(frame,5120);assert(frame.meterPeak5s==0);
 for(unsigned n=129;n<=133;++n)block(n*40,100);
 meter.requestAndCopy(frame,5320);assert(frame.meterRms==100&&frame.meterPeak5s==100);
 meter.invalidate();meter.requestAndCopy(frame,5321);assert(!frame.valid);
 block(5360,0);meter.requestAndCopy(frame,5360);assert(frame.meterRms==0&&frame.meterPeak5s==0&&frame.meterWindows==1);
 // Peak expiry survives millis rollover; baseline quiet data does not extend hold.
 meter.invalidate();block(0xffffff00u,800);block(0x00001289u,0);
 meter.requestAndCopy(frame,0x00001289u);assert(frame.meterPeak5s==0);
 std::cout<<"PASS: audio scope lease/off/OTA/rollover, exact extrema/AC RMS/DC/clipping, no-gap stitching, actual ADC task integration; unseen pulse hold/expiry/rollover/multiple readers/200ms meter. Fixed storage="<<sizeof(AudioScope)<<" bytes\n";
}

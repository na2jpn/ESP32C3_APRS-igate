#include <Arduino.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <iostream>
#define private public
#include "audio_adc.h"
#define Afsk1200Decoder Fix3Decoder
#include "reference/FIX3_afsk1200.h"
#undef Afsk1200Decoder
#undef private
static int fixedFrames=0, baselineFrames=0;
static std::vector<uint8_t> fixedBytes, baselineBytes;
static void received(const uint8_t* p,size_t n){++fixedFrames;fixedBytes.assign(p,p+n);}
static void received3(const uint8_t* p,size_t n){++baselineFrames;baselineBytes.assign(p,p+n);}
static bool near(float a,float b){return fabsf(a-b)<1e-6f;}
static std::vector<uint8_t> stuffed(const std::vector<uint8_t>&bytes){
 std::vector<uint8_t>bits;int ones=0;
 for(auto c:bytes)for(int i=0;i<8;++i){bool b=c&(1<<i);bits.push_back(b);if(b){if(++ones==5){bits.push_back(0);ones=0;}}else ones=0;}
 return bits;
}
int main(){
 Afsk1200Decoder d(received);Fix3Decoder base(received3);d.begin();base.begin();AudioAdcCapture a(d);
 // Real decoder state must match FIX3 sample-for-sample; diagnostic peak must
 // equal the former full-rate algorithm, including expiry at 20 windows.
 float bins[20]{};unsigned pos=0;float oldPeak=0;uint16_t lo=4095,hi=0;
 for(unsigned n=0;n<26400*6;n++){
   const float freq=n<26400?1200.f:2200.f;
   uint16_t raw= n<26400*2?uint16_t(2048+600*sinf(2*3.141592653589793*freq*n/26400)):2048;
   d.processRaw(raw);base.processRaw(raw);a.observe(raw);
   assert(near(d.meanSquare(),base.rms()));assert(near(d._mi,base._mi));assert(d.crcBad()==base.crcBad());
   oldPeak=std::max(oldPeak,sqrtf(base.rms()));lo=std::min(lo,raw);hi=std::max(hi,raw);
   if((n+1)%6600==0){
     bins[pos]=oldPeak;pos=(pos+1)%20;auto m=a.monitor();float p=0;for(float b:bins)p=std::max(p,b);
     assert(near(m.rms,sqrtf(base.rms())));assert(near(m.peak5s,p));assert(m.adcMin==lo&&m.adcMax==hi);assert(m.sampleCount==n+1);
     if(n==6599)assert(m.mark1200>m.space2200);
     if(n==32999)assert(m.space2200>m.mark1200);
     lo=4095;hi=0;oldPeak=0;
   }
 }
 assert(a._monitorMux==0&&a._overflowMux==0);
 for(int stage=1;stage<=4;stage++){failStage=stage;deinits=stops=0;AudioAdcCapture x(d);assert(!x.begin());assert(deinits==(stage>1));}
 failStage=0;taskFail=true;stops=deinits=0;{AudioAdcCapture x(d);assert(!x.begin());assert(stops==1&&deinits==1);}taskFail=false;
 {AudioAdcCapture x(d);assert(x.begin());assert(taskPriority==1&&taskStack==6144);assert(!x.begin());}
 reads=yields=0;try{a.taskLoop();}catch(EndRead&){}assert(yields==2&&a._totalSamples==26400*6+9*128);
 a.setMaintenance(true);assert(a.maintenance());uint32_t before=a._totalSamples;
 reads=yields=0;try{a.taskLoop();}catch(EndRead&){}assert(yields==9&&a._totalSamples==before);
 a.setMaintenance(false);assert(!a.maintenance());
 errors=true;reads=yields=0;try{a.taskLoop();}catch(EndRead&){}assert(yields==9);assert(a.monitor().readErrors==4&&a.monitor().readTimeouts==5);
 AudioAdcCapture::poolOverflow(nullptr,nullptr,&a);assert(a.overflowCount()==1);
 // HDLC destuffing/FCS acceptance and rejection on actual decoder code.
 std::vector<uint8_t> frame;
 auto address=[&](const char*call,int ssid){for(int i=0;i<6;i++)frame.push_back(call[i]<<1);frame.push_back(ssid);};
 address("APRS  ",0x60);address("N0CALL",0x61);
 frame.push_back(0x03);frame.push_back(0xf0);
 const char*info=">FIX5 synthetic host test";
 frame.insert(frame.end(),info,info+strlen(info));
 const auto payload=frame;
 uint16_t crc=Afsk1200Decoder::crc16X25(frame.data(),frame.size());frame.push_back(crc&255);frame.push_back(crc>>8);
 auto bits=stuffed(frame);fixedFrames=baselineFrames=0;
 d.decodeRawFrame(bits.data(),bits.size());base.decodeRawFrame(bits.data(),bits.size());assert(fixedFrames==1&&baselineFrames==1);
 frame[0]^=1;bits=stuffed(frame);auto bad=d.crcBad();d.decodeRawFrame(bits.data(),bits.size());assert(d.crcBad()==bad+1&&fixedFrames==1);
 // Synthetic continuous-phase Bell 202 waveform; flags then stuffed frame.
 frame[0]^=1;bits=stuffed(frame);std::vector<uint8_t>wire;
 for(int n=0;n<40;n++)for(int b=0;b<8;b++)wire.push_back((0x7e>>b)&1);
 wire.insert(wire.end(),bits.begin(),bits.end());for(int n=0;n<8;n++)for(int b=0;b<8;b++)wire.push_back((0x7e>>b)&1);
 d.resetAll();base.resetAll();fixedFrames=baselineFrames=0;bool tone=true;double phase=0;
 for(auto b:wire){if(!b)tone=!tone;for(int n=0;n<22;n++){phase+=2*3.141592653589793*(tone?1200:2200)/26400;auto raw=uint16_t(2048+700*sin(phase));d.processRaw(raw);base.processRaw(raw);}}
 assert(fixedFrames==1&&baselineFrames==1);
 assert(fixedBytes==payload&&baselineBytes==payload);
 std::cout<<"PASS: diagnostics, FIX3 decoder equivalence, tone discrimination, peak hold, ADC failure cleanup, bounded scheduling, timeout/error counters, HDLC/FCS. Synthetic waveform frames FIX3="<<baselineFrames<<" FIX5="<<fixedFrames<<"\n";
}

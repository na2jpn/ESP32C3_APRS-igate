#pragma once
#include <Arduino.h>
#include <math.h>

// ADC task owns accumulator. HTTP only renews a short lease and copies published
// frames under _mux. No allocation, string formatting, or driver calls in feed().
struct AudioScopeFrame {
  static constexpr unsigned BINS=264, STRIDE=4, SAMPLES=BINS*STRIDE;
  uint16_t low[BINS]{},high[BINS]{};
  uint16_t minimum=0,maximum=0;
  uint32_t sequence=0,atMs=0;
  float mean=0,rms=0,peak=0;
  float meterRms=0,meterPeak5s=0; // 200ms RMS and maximum 40ms RMS held on device
  uint32_t meterWindows=0;
  unsigned clipped=0;
  bool valid=false;
};
class AudioScope {
public:
  void requestAndCopy(AudioScopeFrame &out,uint32_t now){
    portENTER_CRITICAL(&_mux);_requested=true;_requestedAt=now;out=_frame;portEXIT_CRITICAL(&_mux);
  }
  void beginBlock(uint32_t now,bool paused){
    portENTER_CRITICAL(&_mux);bool enabled=_requested&&uint32_t(now-_requestedAt)<3000;portEXIT_CRITICAL(&_mux);
    if(paused||!enabled){if(_active)invalidate();_active=false;return;}
    _active=true;
  }
  void invalidate(){
    _count=0;_sum=0;_square=0;_min=4095;_max=0;_clipped=0;
    _historyCount=0;_historyPos=0;_meterWindows=0;
    portENTER_CRITICAL(&_mux);_frame.valid=false;portEXIT_CRITICAL(&_mux);
  }
  void feed(uint16_t raw,uint32_t now){
    if(!_active)return;
    unsigned bin=_count/AudioScopeFrame::STRIDE;
    if(_count%AudioScopeFrame::STRIDE==0)_low[bin]=_high[bin]=raw;
    else{if(raw<_low[bin])_low[bin]=raw;if(raw>_high[bin])_high[bin]=raw;}
    _sum+=raw;_square+=uint32_t(raw)*raw;
    if(raw<_min)_min=raw;
    if(raw>_max)_max=raw;
    if(raw<=16||raw>=4079)++_clipped;
    if(++_count<AudioScopeFrame::SAMPLES)return;
    // AC RMS removes measured DC offset, separately shown as mean raw ADC code.
    double exactMean=double(_sum)/_count;
    float mean=float(exactMean);
    float variance=float(double(_square)/_count-exactMean*exactMean);
    float rms=sqrtf(variance>0?variance:0);
    float peak=fmaxf(mean-_min,_max-mean);
    // Every completed frame contributes, even when HTTP does not poll it.
    // Fixed history; neither copying nor multiple viewers consumes the peak.
    _history[_historyPos]={now,rms*rms};
    _historyPos=(_historyPos+1)%HISTORY;
    if(_historyCount<HISTORY)++_historyCount;
    ++_meterWindows;
    float peakEnergy=0,recentEnergy=0;unsigned recentCount=0;
    for(unsigned n=0;n<_historyCount;++n){
      const auto &entry=_history[(_historyPos+HISTORY-1-n)%HISTORY];
      if(uint32_t(now-entry.at)>5000)continue;
      if(entry.energy>peakEnergy)peakEnergy=entry.energy;
      if(n<5){recentEnergy+=entry.energy;++recentCount;}
    }
    const float meterRms=sqrtf(recentEnergy/recentCount);
    const float meterPeak=sqrtf(peakEnergy);
    portENTER_CRITICAL(&_mux);
    _frame.meterRms=meterRms;_frame.meterPeak5s=meterPeak;_frame.meterWindows=_meterWindows;
    for(unsigned i=0;i<AudioScopeFrame::BINS;++i){_frame.low[i]=_low[i];_frame.high[i]=_high[i];}
    _frame.minimum=_min;_frame.maximum=_max;_frame.mean=mean;_frame.rms=rms;_frame.peak=peak;
    _frame.clipped=_clipped;_frame.atMs=now;++_frame.sequence;_frame.valid=true;
    portEXIT_CRITICAL(&_mux);
    _count=0;_sum=0;_square=0;_min=4095;_max=0;_clipped=0;
  }
private:
  portMUX_TYPE _mux=portMUX_INITIALIZER_UNLOCKED;
  AudioScopeFrame _frame;
  static constexpr unsigned HISTORY=128;
  struct MeterEntry{uint32_t at=0;float energy=0;};
  MeterEntry _history[HISTORY]{};
  unsigned _historyPos=0,_historyCount=0;
  uint32_t _meterWindows=0;
  uint16_t _low[AudioScopeFrame::BINS]{},_high[AudioScopeFrame::BINS]{};
  uint32_t _count=0,_sum=0,_requestedAt=0;
  uint64_t _square=0;
  uint16_t _min=4095,_max=0;
  unsigned _clipped=0;
  bool _requested=false,_active=false;
};
static_assert(sizeof(AudioScope)<3600,"Bounded audio monitor RAM");

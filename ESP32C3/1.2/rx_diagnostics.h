#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "device_clock.h"

// Owned by the ADC task. External access is under AudioAdcCapture::_diagMux.
// No allocation, HTTP, text formatting, Flash writes or NTP queries here.
struct RxDiagnosticRow {
  DiagnosticTime time;
  float rms=0,mark=0,space=0;
  uint32_t samples=0,crc=0,good=0,errors=0,overflows=0,readErrors=0,timeouts=0;
  uint16_t adcMin=0,adcMax=0;
  bool adcValid=true;
};
enum RxReason : uint16_t { RX_RMS=1,RX_TONES=2,RX_BALANCE=4,RX_CLIP=8,RX_CRC=16,RX_GOOD=32,RX_ADC=64,RX_MANUAL=128 };
struct RxDiagnosticEvent {
  uint32_t id=0;uint16_t reason=0,count=0,triggerIndex=0;bool complete=false;
  RxDiagnosticRow rows[48]{};
};
class RxDiagnostics {
public:
  static constexpr unsigned EVENTS=4,PRE=8,ROWS=48;
  RxDiagnosticEvent events[EVENTS]{};
  bool frozen=false;
  uint32_t total=0,overwritten=0;
  void clear(){
    // Reset metadata only: never build a 13KB temporary on the loop task stack.
    for(auto &e:events){e.id=0;e.reason=0;e.count=0;e.triggerIndex=0;e.complete=false;}
    frozen=false;total=0;overwritten=0;_prePos=0;_preCount=0;_warm=0;_active=-1;
    _rms=0;_tones=0;_balance=0;_manual=false;_haveLast=false;_coolUntil=0;
  }
  void manual(){_manual=true;}
  void pause(){if(_active>=0)events[_active].complete=true;_active=-1;_preCount=0;_warm=0;_haveLast=false;}
  void feed(const RxDiagnosticRow &r,uint16_t forcedReason=0){
    if(frozen)return;
    uint16_t reason=forcedReason;
    float tones=r.mark+r.space;
    float balance=tones>.002f?(r.mark-r.space)/tones:0;
    if(_haveLast){
      if(r.crc!=_last.crc)reason|=RX_CRC;
      if(r.good!=_last.good)reason|=RX_GOOD;
      if(r.errors!=_last.errors||r.overflows!=_last.overflows||r.readErrors!=_last.readErrors||r.timeouts!=_last.timeouts)reason|=RX_ADC;
    }
    if(r.adcValid&&(r.adcMin<=16||r.adcMax>=4079))reason|=RX_CLIP;
    if(_manual){reason|=RX_MANUAL;_manual=false;}
    if(r.adcValid&&_warm>=50){
      if(fabsf(r.rms-_rms)>fmaxf(.012f,_rms*.30f))reason|=RX_RMS;
      if(fabsf(tones-_tones)>fmaxf(.006f,_tones*.35f))reason|=RX_TONES;
      if(tones>.008f&&fabsf(balance-_balance)>.45f)reason|=RX_BALANCE;
    }
    if(_active>=0){
      auto &e=events[_active];e.reason|=reason;e.rows[e.count++]=r;
      if(e.count>=ROWS){e.complete=true;_active=-1;_coolUntil=r.time.uptimeMs+500;}
    }else if(reason&&(r.time.uptimeMs>=_coolUntil||(reason&(RX_GOOD|RX_MANUAL)))){
      unsigned slot=total%EVENTS;auto &e=events[slot];
      if(total>=EVENTS)++overwritten;
      e.id=++total;e.reason=reason;e.complete=false;e.count=0;
      for(unsigned i=0;i<_preCount;++i)e.rows[e.count++]=_pre[(_prePos+PRE-_preCount+i)%PRE];
      e.triggerIndex=e.count;e.rows[e.count++]=r;_active=int(slot);
    }
    // Track baseline only when not inside an event; rebase after each event
    // instead of repeatedly triggering on a permanently changed noise floor.
    if(r.adcValid&&!_warm){_rms=r.rms;_tones=tones;_balance=balance;}
    else if(r.adcValid&&_active<0){_rms+=.05f*(r.rms-_rms);_tones+=.05f*(tones-_tones);_balance+=.05f*(balance-_balance);}
    if(r.adcValid&&_warm<50)++_warm;
    _pre[_prePos]=r;_prePos=(_prePos+1)%PRE;if(_preCount<PRE)++_preCount;
    _last=r;_haveLast=true;
  }
private:
  RxDiagnosticRow _pre[PRE]{},_last{};
  unsigned _prePos=0,_preCount=0,_warm=0;int _active=-1;
  float _rms=0,_tones=0,_balance=0;bool _manual=false,_haveLast=false;
  uint64_t _coolUntil=0;
};
static_assert(sizeof(RxDiagnostics)<=16384,"RX diagnostics must fit 16 KiB");

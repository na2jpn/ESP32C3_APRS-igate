#pragma once
#include "ui_page.h"
#include <memory>
#include <new>
#include "audio_scope_page.h"

static void setupAudioScopeRoutes(WebServer &s,AppConfig &cfg,AudioAdcCapture &audio){
  s.on("/audio",HTTP_GET,[&s,&cfg](){
    if(!logAuth(s,cfg))return;
    s.sendHeader("Cache-Control","no-store");sendUiPage(s,AUDIO_SCOPE_PAGE);
  });
  s.on("/audio/data",HTTP_GET,[&s,&cfg,&audio](){
    if(!logAuth(s,cfg))return;
    s.sendHeader("Cache-Control","no-store");
    if(otaBusy||audio.maintenance()){s.send(409,"text/plain","OTA in progress; audio monitor paused");return;}
    std::unique_ptr<AudioScopeFrame> frame(new(std::nothrow) AudioScopeFrame);
    if(!frame){s.send(503,"text/plain","Not enough free heap");return;}
    audio.copyScope(*frame);
    auto m=audio.monitor();uint32_t age=uint32_t(millis()-frame->atMs);
    const char *state=m.fault?"FAULT":m.recovering?"RECOVERING":!m.driverRunning?"ADC NOT RUNNING":!m.dataSeen||uint32_t(millis()-m.lastDataAtMs)>1500?"STALE":"OK";
    bool valid=frame->valid&&age<=1500&&state[0]=='O';
    String j;j.reserve(valid?3500:180);
    j="{\"firmware\":\"";j+=FW_VERSION;j+="\",\"state\":\"";j+=state;
    j+="\",\"valid\":";j+=valid?"true":"false";
    j+=",\"age_ms\":";j+=String(age);
    if(valid){
      j+=",\"sequence\":";j+=String(frame->sequence);j+=",\"at_ms\":";j+=String(frame->atMs);
      j+=",\"sample_rate\":";j+=String(AFSK_SAMPLE_RATE);
      j+=",\"samples\":";j+=String(AudioScopeFrame::SAMPLES);
      j+=",\"stride\":";j+=String(AudioScopeFrame::STRIDE);
      j+=",\"duration_ms\":";j+=String(1000.0f*AudioScopeFrame::SAMPLES/AFSK_SAMPLE_RATE,2);
      j+=",\"mean\":";j+=String(frame->mean,3);j+=",\"rms\":";j+=String(frame->rms,3);
      j+=",\"meter_rms\":";j+=String(frame->meterRms,3);
      j+=",\"meter_peak5s\":";j+=String(frame->meterPeak5s,3);
      j+=",\"meter_windows\":";j+=String(frame->meterWindows);
      j+=",\"peak\":";j+=String(frame->peak,3);j+=",\"minimum\":";j+=String(frame->minimum);
      j+=",\"maximum\":";j+=String(frame->maximum);j+=",\"clipped\":";j+=String(frame->clipped);
      j+=",\"low\":[";
      for(unsigned i=0;i<AudioScopeFrame::BINS;++i){if(i)j+=',';j+=String(frame->low[i]);}
      j+="],\"high\":[";
      for(unsigned i=0;i<AudioScopeFrame::BINS;++i){if(i)j+=',';j+=String(frame->high[i]);}
      j+=']';
    }
    j+='}';s.send(200,"application/json",j);
  });
}

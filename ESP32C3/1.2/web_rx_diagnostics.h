#pragma once
#include "ui_page.h"
#include <memory>
#include <new>

static void setupRxDiagnosticRoutes(WebServer &s,AppConfig &cfg,AudioAdcCapture &audio){
  s.on("/rxdiag",HTTP_GET,[&s,&cfg](){
    if(!logAuth(s,cfg))return;
    String page=R"HTML(<!doctype html><html lang='ja'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>{{rx.title}}</title><style>body{font-family:system-ui;margin:20px}pre{white-space:pre;overflow:auto;background:#f2f5f7;padding:10px}button{margin:5px;padding:8px}</style><h1>{{rx.title}} v1.2</h1><p>{{rx.hint}}</p><p>{{rx.window}}</p><p>{{rx.holdhint}}</p><button onclick="action('freeze')">{{rx.freeze}}</button><button onclick="action('resume')">{{rx.resume}}</button><button onclick="action('manual')">{{rx.manual}}</button><button onclick="action('clear')">{{rx.clear}}</button><button onclick='load()'>{{rx.refresh}}</button><a href='/rxdiag.csv?download=1'>{{rx.csv}}</a> / <a href='/logs'>{{rx.log}}</a> / <a href='/'>{{rx.back}}</a><p id='result'></p><p>{{rx.reason}}</p><pre id='data'>{{rx.loading}}</pre><script>async function load(){try{const r=await fetch('/rxdiag.csv',{cache:'no-store'});if(!r.ok)throw Error(r.status);document.getElementById('data').textContent=await r.text();}catch(e){document.getElementById('data').textContent='{{rx.fetchfail}}'+e.message;}}async function action(a){if(a==='clear'&&!confirm('{{rx.confirm}}'))return;try{const r=await fetch('/rxdiag/control?token=)HTML";
    page+=otaToken;page+=R"HTML(&lang={{lang}}&action='+a,{method:'POST'});if(!r.ok)throw Error(r.status);document.getElementById('result').textContent=await r.text();load();}catch(e){document.getElementById('result').textContent='{{rx.opfail}}'+e.message;}}load();</script></html>)HTML";
    s.sendHeader("Cache-Control","no-store");sendUiPage(s,page);
  });
  s.on("/rxdiag/control",HTTP_POST,[&s,&cfg,&audio](){
    if(!logAuth(s,cfg))return;
    if(s.arg("token")!=otaToken){s.send(403,"text/plain","Invalid token");return;}
    if(otaBusy){s.send(409,"text/plain","OTA in progress");return;}
    String a=s.arg("action");unsigned n;
    if(a=="freeze")n=0;else if(a=="resume")n=1;else if(a=="clear")n=2;else if(a=="manual")n=3;
    else{s.send(400,"text/plain","Unknown action");return;}
    audio.controlDiagnostics(n);appLog.printf("[RXDIAG] action=%s",a.c_str());
    s.send(200,"text/plain; charset=utf-8",uiText(n==0?"rx.frozen":"rx.accepted",uiEnglish(s)));
  });
  s.on("/rxdiag.csv",HTTP_GET,[&s,&cfg,&audio](){
    if(!logAuth(s,cfg))return;
    if(otaBusy){s.send(409,"text/plain","OTA in progress; retry after update");return;}
    std::unique_ptr<RxDiagnostics> copy(new(std::nothrow) RxDiagnostics);
    if(!copy){s.send(503,"text/plain","Not enough free heap for diagnostic snapshot");return;}
    audio.copyDiagnostics(*copy);
    s.sendHeader("Cache-Control","no-store");
    if(s.arg("download")=="1")s.sendHeader("Content-Disposition","attachment; filename=igate-rx-v1_2.csv");
    s.setContentLength(CONTENT_LENGTH_UNKNOWN);s.send(200,"text/plain; charset=utf-8","");
    char line[512],date[90];
    snprintf(line,sizeof(line),"# v1.2 frozen=%u total=%lu overwritten=%lu storage=%u bytes; rows are ~20ms, not raw audio\n",copy->frozen,(unsigned long)copy->total,(unsigned long)copy->overwritten,unsigned(sizeof(RxDiagnostics)));
    s.sendContent(line);
    s.sendContent("event,reason,complete,row,trigger,time,uptime_ms,unix_ms,rms,mark1200,space2200,adc_min,adc_max,samples,crc_bad,good,frame_errors,overflows,read_errors,timeouts,delta_crc,delta_good,delta_frame_errors,delta_overflow,delta_read_errors,delta_timeouts,adc_valid\n");
    unsigned first=copy->total>RxDiagnostics::EVENTS?copy->total-RxDiagnostics::EVENTS:0;
    for(unsigned k=first;k<copy->total;++k){
      const auto &e=copy->events[k%RxDiagnostics::EVENTS];
      for(unsigned i=0;i<e.count;++i){
        const auto &r=e.rows[i];const auto &prev=e.rows[i?i-1:0];DeviceClock::format(r.time,date,sizeof(date));
        snprintf(line,sizeof(line),"%lu,%u,%u,%u,%u,%s,%llu,%llu,%.6f,%.6f,%.6f,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u\n",
          (unsigned long)e.id,e.reason,e.complete,i,i==e.triggerIndex,date,(unsigned long long)r.time.uptimeMs,(unsigned long long)r.time.unixMs,
          r.rms,r.mark,r.space,r.adcMin,r.adcMax,(unsigned long)r.samples,(unsigned long)r.crc,(unsigned long)r.good,(unsigned long)r.errors,(unsigned long)r.overflows,(unsigned long)r.readErrors,(unsigned long)r.timeouts,
          (unsigned long)(r.crc-prev.crc),(unsigned long)(r.good-prev.good),(unsigned long)(r.errors-prev.errors),(unsigned long)(r.overflows-prev.overflows),(unsigned long)(r.readErrors-prev.readErrors),(unsigned long)(r.timeouts-prev.timeouts),r.adcValid?1:0);
        s.sendContent(line);
      }
      delay(1);
    }
    s.sendContent("");
  });
}

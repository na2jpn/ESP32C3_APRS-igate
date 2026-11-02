#pragma once
#include "ui_page.h"
static bool logAuth(WebServer &s,const AppConfig &cfg){
  if(cfg.otaPassword.length()>=8&&!s.authenticate("admin",cfg.otaPassword.c_str())){s.requestAuthentication();return false;}return true;
}
static void setupLogRoutes(WebServer &s,AppConfig &cfg){
  s.on("/logs",HTTP_GET,[&s,&cfg](){if(!logAuth(s,cfg))return;
    String page=R"HTML(<!doctype html><html lang='ja'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>{{logs.title}}</title><style>body{font-family:system-ui;max-width:1000px;margin:24px auto;padding:0 14px}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#f2f5f7;padding:12px}button{padding:8px;margin:5px}</style><h1>{{logs.title}}</h1><p>{{logs.hint}}</p><button onclick='load()'>{{logs.refresh}}</button><a href='/logs.txt?download=1'>{{logs.download}}</a><button onclick='clearLog()'>{{logs.clear}}</button><a href='/'>{{logs.back}}</a><pre id='log'>{{logs.loading}}</pre><script>async function load(){try{const r=await fetch('/logs.txt',{cache:'no-store'});if(!r.ok)throw Error(r.status);document.getElementById('log').textContent=await r.text();}catch(e){document.getElementById('log').textContent='{{logs.failure}}'+e.message;}}async function clearLog(){if(!confirm('{{logs.confirm}}'))return;const r=await fetch('/logs/clear?token=)HTML";
    page+=otaToken;page+=R"HTML(',{method:'POST'});if(!r.ok){alert('{{logs.clearfail}}'+r.status);return;}load();}load();</script></html>)HTML";
    s.sendHeader("Cache-Control","no-store");sendUiPage(s,page);
  });
  s.on("/logs.txt",HTTP_GET,[&s,&cfg](){if(!logAuth(s,cfg))return;s.sendHeader("Cache-Control","no-store");if(s.arg("download")=="1")s.sendHeader("Content-Disposition","attachment; filename=igate-log.txt");s.send(200,"text/plain; charset=utf-8",appLog.snapshot());});
  s.on("/logs/clear",HTTP_POST,[&s,&cfg](){if(!logAuth(s,cfg))return;if(s.arg("token")!=otaToken){s.send(403,"text/plain","Invalid token");return;}appLog.clear();appLog.printf("[LOG] cleared by administrator");s.send(200,"text/plain","OK");});
}

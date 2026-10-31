#pragma once
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <cstring>

static bool otaBusy=false,otaSuccess=false;
static size_t otaExpected=0,otaReceived=0;
static uint32_t otaActivity=0,otaRebootAt=0,otaSession=0,otaLogAt=0;
static String otaError,otaToken;
static void (*otaMaintenance)(bool)=nullptr;
// RTC retention is best effort across software/watchdog reset, not power loss.
struct OtaRecord { uint32_t magic,expected,received,state,checksum; };
RTC_NOINIT_ATTR static OtaRecord otaRecord;
static uint32_t otaChecksum(const OtaRecord &r){return r.magic^r.expected^r.received^r.state^0x618c42adu;}
static void rememberOta(uint32_t state){otaRecord={0x49473037u,(uint32_t)otaExpected,(uint32_t)otaReceived,state,0};otaRecord.checksum=otaChecksum(otaRecord);}
static void logOtaBoot(){
  appLog.printf("[BOOT] firmware=%s reset_reason=%d",FW_VERSION,(int)esp_reset_reason());
  if(otaRecord.magic==0x49473037u && otaRecord.checksum==otaChecksum(otaRecord))
    appLog.printf("[PREVIOUS OTA] state=%s received=%lu/%lu (RTC; power loss may erase)",otaRecord.state==1?"interrupted":(otaRecord.state==2?"verified before reboot":"failed"),(unsigned long)otaRecord.received,(unsigned long)otaRecord.expected);
}
static bool parseOtaNumber(const String &s,uint32_t &out){
  if(!s.length() || s.length()>10)return false; uint64_t n=0;
  for(size_t i=0;i<s.length();++i){if(s[i]<'0'||s[i]>'9')return false;n=n*10+(s[i]-'0');if(n>0xffffffffu)return false;}
  out=(uint32_t)n;return true;
}
static void abortOta(const char *reason){
  if(otaSuccess)return;
  if(otaBusy)Update.abort();
  String detail(reason);otaBusy=false;otaError=detail;rememberOta(3);
  if(otaMaintenance)otaMaintenance(false);
  appLog.printf("[OTA] failed received=%u/%u: %s",(unsigned)otaReceived,(unsigned)otaExpected,otaError.c_str());
}
static void serviceOtaTimeout(){
  uint32_t now=millis();
  if(otaSuccess && otaRebootAt && (int32_t)(now-otaRebootAt)>=0){ESP.restart();return;}
  if(otaBusy && !otaSuccess && uint32_t(now-otaActivity)>120000UL)abortOta("No acknowledged chunk for 120 s; upload aborted");
}
static bool otaAuth(WebServer &s,const AppConfig &cfg,bool token){
  if(cfg.otaPassword.length()<8){s.send(403,"text/plain","Set OTA password first.");return false;}
  if(!s.authenticate("admin",cfg.otaPassword.c_str())){s.requestAuthentication();return false;}
  if(token && s.arg("token")!=otaToken){s.send(403,"text/plain","Invalid token; reopen update page.");return false;}
  return true;
}
static String otaStateJson(){
  String s="{\"busy\":";s+=otaBusy?"true":"false";
  s+=",\"verified\":";s+=otaSuccess?"true":"false";
  s+=",\"received\":";s+=(unsigned long)otaReceived;
  s+=",\"expected\":";s+=(unsigned long)otaExpected;
  s+=",\"firmware\":\"";s+=FW_VERSION;s+="\"}";return s;
}
static int hexNibble(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static void setupOtaRoutes(WebServer &s,AppConfig &cfg,void (*maintenance)(bool)){
  otaMaintenance=maintenance;
  char token[33];snprintf(token,sizeof(token),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());otaToken=token;
  s.on("/update/status",HTTP_GET,[&s,&cfg](){if(!otaAuth(s,cfg,false))return;s.sendHeader("Cache-Control","no-store");s.send(200,"application/json",otaStateJson());});
  s.on("/update",HTTP_GET,[&s,&cfg](){
    if(!otaAuth(s,cfg,false))return;
    const esp_partition_t *part=esp_ota_get_next_update_partition(nullptr);
    if(!part){s.send(409,"text/plain","No OTA slot. Install by USB with Default 4MB OTA partitions.");return;}
    String page=R"HTML(<!doctype html><html lang='ja'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>APRS iGate OTA</title><style>body{font-family:system-ui;max-width:760px;margin:24px auto;padding:0 16px}button,input{margin:8px 0;padding:8px}progress{width:100%}pre{white-space:pre-wrap}</style><h1>ファームウェア更新</h1><p>本機用のアプリ単体 .bin を選択してください。更新中は受信・転送を一時休止します。電源は切らないでください。</p><input id='f' type='file' accept='.bin'><button id='go' onclick='go()'>更新開始</button><button id='cancel' onclick='cancel()' disabled>中止</button><progress id='p' max='100' value='0'></progress><pre id='s'>待機中</pre><p><a href='/logs'>本体ログ</a> / <a href='/'>管理画面へ戻る</a></p><script>
let token=')HTML";
    page+=otaToken;
    page+=R"HTML(',stop=false,session='',oldVersion='';
const msg=t=>document.getElementById('s').textContent=t;
async function req(path,body=''){const ctrl=new AbortController();const timer=setTimeout(()=>ctrl.abort(),30000);try{const r=await fetch(path,{method:'POST',body,headers:{'Content-Type':'text/plain'},signal:ctrl.signal,cache:'no-store'});const t=await r.text();if(!r.ok)throw Error('HTTP '+r.status+': '+t);return JSON.parse(t);}finally{clearTimeout(timer);}}
function cancel(){stop=true;msg('中止要求中。現在のブロックの応答を待っています。');}
async function check(){try{const r=await fetch('/update/status',{cache:'no-store'});if(!r.ok)throw Error();const d=await r.json();msg('再接続しました。現在のファーム: '+d.firmware+' / 更新前: '+oldVersion+'。バージョンと本体ログを確認してください。');}catch(e){msg('まだ再接続できません。しばらく待って管理画面を開いてください。IP変更時は新しいアドレスで開きます。');}}
async function go(){const f=document.getElementById('f').files[0];if(!f||!f.name.endsWith('.bin')){msg('.bin を選択してください。');return;}
stop=false;document.getElementById('go').disabled=true;document.getElementById('cancel').disabled=false;
try{const b=await req('/update/begin?token='+token+'&size='+f.size);session='&id='+b.id;oldVersion=b.firmware;
for(let off=0;off<f.size;off+=2048){if(stop){await req('/update/abort?token='+token+session);msg('更新を中止しました。受信処理は再開されます。');return;}
const bytes=new Uint8Array(await f.slice(off,off+2048).arrayBuffer());let hex='';for(const x of bytes)hex+=x.toString(16).padStart(2,'0');
const a=await req('/update/chunk?token='+token+session+'&offset='+off,hex);
document.getElementById('p').value=a.received/f.size*100;msg('本体受信確認: '+a.received+' / '+f.size+' バイト ('+Math.floor(a.received/f.size*100)+'%)');await new Promise(r=>setTimeout(r,20));}
await req('/update/finish?token='+token+session);msg('全データの受信・検証が完了しました。再起動後、約30秒で再接続を確認します。');setTimeout(check,30000);
}catch(e){msg('更新の完了を確認できません: '+e.message+'\n通信断だけでは成功とは判断できません。30秒後に再接続を確認します。本体ログも確認してください。未完了のセッションは120秒で解除されます。');setTimeout(check,30000);
}finally{document.getElementById('go').disabled=false;document.getElementById('cancel').disabled=true;}}
</script></html>)HTML";
    s.sendHeader("Cache-Control","no-store");s.send(200,"text/html; charset=utf-8",page);
  });
  s.on("/update/begin",HTTP_POST,[&s,&cfg](){
    if(!otaAuth(s,cfg,true))return;
    if(otaBusy || otaSuccess){s.send(409,"text/plain","Session active. Check status/logs; cancel or wait for timeout.");return;}
    uint32_t size;const esp_partition_t *part=esp_ota_get_next_update_partition(nullptr);
    if(!parseOtaNumber(s.arg("size"),size)||!size||!part||size>part->size){s.send(400,"text/plain","Invalid image size or missing OTA slot.");return;}
    otaExpected=size;otaReceived=0;otaError="";otaSuccess=false;otaRebootAt=0;otaSession=esp_random();otaLogAt=0;
    if(!otaSession)otaSession=1;
    if(otaMaintenance)otaMaintenance(true);
    if(!Update.begin(size,U_FLASH)){otaError=Update.errorString();abortOta(otaError.c_str());s.send(500,"text/plain",otaError);return;}
    otaBusy=true;otaActivity=millis();rememberOta(1);
    appLog.printf("[OTA] begin size=%u slot=%s heap=%u; RF decode paused",size,part->label,ESP.getFreeHeap());
    String r="{\"id\":";r+=otaSession;r+=",\"firmware\":\"";r+=FW_VERSION;r+="\"}";s.send(200,"application/json",r);
  });
  auto sessionOK=[&s,&cfg](){
    if(!otaAuth(s,cfg,true))return false;uint32_t id;
    if(!otaBusy||otaSuccess||!parseOtaNumber(s.arg("id"),id)||id!=otaSession){s.send(409,"text/plain","No matching active session.");return false;}return true;
  };
  s.on("/update/chunk",HTTP_POST,[&s,sessionOK](){
    if(!sessionOK())return;
    uint32_t offset;String hex=s.arg("plain");
    if(!parseOtaNumber(s.arg("offset"),offset)||offset!=otaReceived){s.send(409,"text/plain","Unexpected offset; abort and start again.");return;}
    if(hex.length()==0||hex.length()>4096||(hex.length()&1)||hex.length()/2>otaExpected-otaReceived){abortOta("Invalid chunk size");s.send(400,"text/plain",otaError);return;}
    uint8_t bytes[2048];size_t n=hex.length()/2;
    for(size_t i=0;i<n;++i){int a=hexNibble(hex[2*i]),b=hexNibble(hex[2*i+1]);if(a<0||b<0){abortOta("Invalid hex data");s.send(400,"text/plain",otaError);return;}bytes[i]=(uint8_t)((a<<4)|b);}
    if(Update.write(bytes,n)!=n){String e=Update.errorString();abortOta(e.c_str());s.send(500,"text/plain",otaError);return;}
    otaReceived+=n;otaActivity=millis();rememberOta(1);
    if(otaReceived-otaLogAt>=65536||otaReceived==otaExpected){otaLogAt=otaReceived;appLog.printf("[OTA] received=%u/%u",(unsigned)otaReceived,(unsigned)otaExpected);}
    s.send(200,"application/json",otaStateJson());
  });
  s.on("/update/finish",HTTP_POST,[&s,sessionOK](){
    if(!sessionOK())return;
    if(otaReceived!=otaExpected){abortOta("Incomplete image");s.send(400,"text/plain",otaError);return;}
    if(!Update.end(false)){String e=Update.errorString();abortOta(e.c_str());s.send(400,"text/plain",otaError);return;}
    otaSuccess=true;rememberOta(2);otaRebootAt=millis()+2000UL;
    appLog.printf("[OTA] image verified; reboot scheduled in 2 s");
    s.send(200,"application/json",otaStateJson());
  });
  s.on("/update/abort",HTTP_POST,[&s,sessionOK](){if(!sessionOK())return;abortOta("Cancelled by administrator");s.send(200,"application/json",otaStateJson());});
  s.on("/update",HTTP_POST,[&s,&cfg](){if(!otaAuth(s,cfg,false))return;s.send(405,"text/plain","Reopen /update and use the v0.7 chunk uploader.");});
}

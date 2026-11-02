#pragma once
#include "ui_page.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <cstring>
#include <mbedtls/sha256.h>
#include "ota_page.h"

static bool otaBusy=false,otaSuccess=false;
static size_t otaExpected=0,otaReceived=0;
static uint32_t otaActivity=0,otaRebootAt=0,otaSession=0,otaLogAt=0;
static String otaError,otaToken,otaClient,otaSha;
static constexpr uint32_t OTA_IDLE_MS=10UL*60UL*1000UL;
static mbedtls_sha256_context otaHash;
static bool otaHashLive=false;
static size_t otaLastOffset=0,otaLastSize=0;
static unsigned char otaLastHash[32]{};
static bool validOtaHex(const String &s,size_t n){
  if(s.length()!=n)return false;
  for(size_t i=0;i<n;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
  return true;
}
static void digestHex(const unsigned char *bytes,char *out){for(unsigned i=0;i<32;++i)snprintf(out+2*i,3,"%02x",bytes[i]);}
static bool digestBytes(const uint8_t *bytes,size_t n,unsigned char *out){
  mbedtls_sha256_context h;mbedtls_sha256_init(&h);
  bool ok=mbedtls_sha256_starts(&h,0)==0&&mbedtls_sha256_update(&h,bytes,n)==0&&mbedtls_sha256_finish(&h,out)==0;
  mbedtls_sha256_free(&h);return ok;
}
static void freeOtaHash(){if(otaHashLive){mbedtls_sha256_free(&otaHash);otaHashLive=false;}}

static void (*otaMaintenance)(bool)=nullptr;
// RTC retention is best effort across software/watchdog reset, not power loss.
struct OtaRecord { uint32_t magic,expected,received,state,checksum;char sha[65],client[33]; };
RTC_NOINIT_ATTR static OtaRecord otaRecord;
static uint32_t otaChecksum(const OtaRecord &r){
  uint32_t h=r.magic^r.expected^r.received^r.state^0x618c42adu;
  for(unsigned i=0;i<65;++i)h=(h^uint8_t(r.sha[i]))*16777619u;
  for(unsigned i=0;i<33;++i)h=(h^uint8_t(r.client[i]))*16777619u;
  return h;
}
static bool otaRecordValid(){return otaRecord.magic==0x49473039u&&otaRecord.checksum==otaChecksum(otaRecord)&&otaRecord.sha[64]==0&&otaRecord.client[32]==0;}
static void rememberOta(uint32_t state){
  otaRecord={};otaRecord.magic=0x49473039u;otaRecord.expected=otaExpected;otaRecord.received=otaReceived;otaRecord.state=state;
  snprintf(otaRecord.sha,sizeof(otaRecord.sha),"%s",otaSha.c_str());snprintf(otaRecord.client,sizeof(otaRecord.client),"%s",otaClient.c_str());
  otaRecord.checksum=otaChecksum(otaRecord);
}
static void logOtaBoot(){
  appLog.printf("[BOOT] firmware=%s reset_reason=%d",FW_VERSION,(int)esp_reset_reason());
  if(otaRecordValid())
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
  freeOtaHash();
  String detail(reason);otaBusy=false;otaError=detail;rememberOta(3);
  if(otaMaintenance)otaMaintenance(false);
  appLog.printf("[OTA] failed received=%u/%u: %s",(unsigned)otaReceived,(unsigned)otaExpected,otaError.c_str());
}
static void serviceOtaTimeout(){
  uint32_t now=millis();
  if(otaSuccess && (int32_t)(now-otaRebootAt)>=0){ESP.restart();return;}
  if(otaBusy && !otaSuccess && uint32_t(now-otaActivity)>OTA_IDLE_MS)abortOta("No received chunk for 10 min; upload aborted");
}
static bool otaAuth(WebServer &s,const AppConfig &cfg,bool token){
  if(cfg.otaPassword.length()<8){s.send(403,"text/plain",uiText("error.ota.password",uiEnglish(s)));return false;}
  if(!s.authenticate("admin",cfg.otaPassword.c_str())){s.requestAuthentication();return false;}
  if(token && s.arg("token")!=otaToken){s.send(403,"text/plain",uiText("error.ota.token",uiEnglish(s)));return false;}
  return true;
}
static String otaQuoted(const String &input){
  String out="\"";
  for(size_t i=0;i<input.length();++i){char c=input[i];if(c=='"'||c=='\\')out+="\\";if(uint8_t(c)<32){out+=" ";continue;}char one[2]={c,0};out+=one;}
  out+="\"";return out;
}
static String otaStateJson(){
  String s="{\"protocol\":9,\"busy\":";s+=otaBusy?"true":"false";
  s+=",\"verified\":";s+=otaSuccess?"true":"false";
  s+=",\"received\":";s+=(unsigned long)otaReceived;
  s+=",\"expected\":";s+=(unsigned long)otaExpected;
  s+=",\"id\":";s+=otaSession;
  s+=",\"boot\":";s+=otaQuoted(otaToken);
  s+=",\"client\":";s+=otaQuoted(otaClient);
  s+=",\"sha256\":";s+=otaQuoted(otaSha);
  s+=",\"error\":";s+=otaQuoted(otaError);
  s+=",\"idle_limit_ms\":";s+=OTA_IDLE_MS;
  s+=",\"idle_ms\":";s+=uint32_t(millis()-otaActivity);
  bool prior=otaRecordValid()&&otaRecord.state==2&&otaRecord.received==otaRecord.expected;
  s+=",\"previous_verified\":";s+=prior?"true":"false";
  s+=",\"previous_sha256\":";s+=otaQuoted(prior?otaRecord.sha:"");
  s+=",\"previous_client\":";s+=otaQuoted(prior?otaRecord.client:"");
  s+=",\"previous_size\":";s+=prior?otaRecord.expected:0;
  s+=",\"firmware\":";s+=otaQuoted(FW_VERSION);s+="}";return s;
}
static int hexNibble(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static void setupOtaRoutes(WebServer &s,AppConfig &cfg,void (*maintenance)(bool)){
  otaMaintenance=maintenance;
  char token[33];snprintf(token,sizeof(token),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());otaToken=token;
  s.on("/update/status",HTTP_GET,[&s,&cfg](){if(!otaAuth(s,cfg,false))return;s.sendHeader("Cache-Control","no-store");s.send(200,"application/json",otaStateJson());});
  s.on("/update",HTTP_GET,[&s,&cfg](){
    if(!otaAuth(s,cfg,false))return;
    const esp_partition_t *part=esp_ota_get_next_update_partition(nullptr);
    if(!part){s.send(409,"text/plain",uiText("error.ota.slot",uiEnglish(s)));return;}
    String page=otaUpdatePage(otaToken);
    s.sendHeader("Cache-Control","no-store");sendUiPage(s,page);
  });
  s.on("/update/begin",HTTP_POST,[&s,&cfg](){
    if(!otaAuth(s,cfg,true))return;
    uint32_t size;const esp_partition_t *part=esp_ota_get_next_update_partition(nullptr);
    String client=s.arg("client"),sha=s.arg("sha256");
    if(!validOtaHex(client,32)||!validOtaHex(sha,64)||!parseOtaNumber(s.arg("size"),size)||!size||!part||size>part->size){s.send(400,"text/plain","Invalid size, SHA256, client key or missing OTA slot.");return;}
    if(otaBusy||otaSuccess){
      if(client==otaClient&&sha==otaSha&&size==otaExpected){s.send(200,"application/json",otaStateJson());return;}
      s.send(409,"text/plain","Another session is active. Resume with original browser and exact same file, or wait for timeout.");return;
    }
    otaExpected=size;otaReceived=0;otaError="";otaSuccess=false;otaRebootAt=0;otaSession=esp_random();otaLogAt=0;
    otaClient=client;otaSha=sha;otaLastSize=0;otaLastOffset=0;
    if(!otaSession)otaSession=1;
    if(otaMaintenance)otaMaintenance(true);
    if(!Update.begin(size,U_FLASH)){otaError=Update.errorString();abortOta(otaError.c_str());s.send(500,"text/plain",otaError);return;}
    otaBusy=true;
    mbedtls_sha256_init(&otaHash);otaHashLive=true;
    if(mbedtls_sha256_starts(&otaHash,0)!=0){abortOta("SHA256 initialization failed");s.send(500,"text/plain",otaError);return;}
    otaBusy=true;otaActivity=millis();rememberOta(1);
    appLog.printf("[OTA] begin size=%u slot=%s heap=%u; RF paused; inactivity timeout=10 min",size,part->label,ESP.getFreeHeap());
    s.send(200,"application/json",otaStateJson());
  });
  auto sessionOK=[&s,&cfg](){
    if(!otaAuth(s,cfg,true))return false;uint32_t id;
    if(!otaSession||!parseOtaNumber(s.arg("id"),id)||id!=otaSession||s.arg("client")!=otaClient){s.send(409,"text/plain","No matching active session.");return false;}return true;
  };
  s.on("/update/chunk",HTTP_POST,[&s,sessionOK](){
    if(!sessionOK())return;
    if(!otaBusy||otaSuccess){s.send(409,"text/plain","No writable session");return;}
    uint32_t offset;String hex=s.arg("plain");
    if(!parseOtaNumber(s.arg("offset"),offset)){s.send(400,"text/plain","Invalid offset");return;}
    if(hex.length()==0||hex.length()>4096||(hex.length()&1)){abortOta("Invalid chunk size");s.send(400,"text/plain",otaError);return;}
    uint8_t bytes[2048];size_t n=hex.length()/2;
    for(size_t i=0;i<n;++i){int a=hexNibble(hex[2*i]),b=hexNibble(hex[2*i+1]);if(a<0||b<0){abortOta("Invalid hex data");s.send(400,"text/plain",otaError);return;}bytes[i]=(uint8_t)((a<<4)|b);}
    unsigned char digest[32];
    if(!digestBytes(bytes,n,digest)){abortOta("Chunk digest failed");s.send(500,"text/plain",otaError);return;}
    if(offset!=otaReceived){
      // A lost ACK may replay only the exact most recent block. Never write it twice.
      if(otaLastSize&&offset==otaLastOffset&&n==otaLastSize&&memcmp(digest,otaLastHash,32)==0){s.send(200,"application/json",otaStateJson());return;}
      s.send(409,"text/plain","Offset or replay mismatch; query status before continuing");return;
    }
    if(n>otaExpected-otaReceived){abortOta("Chunk exceeds declared size");s.send(400,"text/plain",otaError);return;}
    if(Update.write(bytes,n)!=n){String e=Update.errorString();abortOta(e.c_str());s.send(500,"text/plain",otaError);return;}
    if(mbedtls_sha256_update(&otaHash,bytes,n)!=0){abortOta("Image digest update failed");s.send(500,"text/plain",otaError);return;}
    otaLastOffset=offset;otaLastSize=n;memcpy(otaLastHash,digest,32);
    otaReceived+=n;otaActivity=millis();rememberOta(1);
    if(otaReceived-otaLogAt>=65536||otaReceived==otaExpected){otaLogAt=otaReceived;appLog.printf("[OTA] received=%u/%u",(unsigned)otaReceived,(unsigned)otaExpected);}
    s.send(200,"application/json",otaStateJson());
  });
  s.on("/update/finish",HTTP_POST,[&s,sessionOK](){
    if(!sessionOK())return;
    if(otaSuccess){s.send(200,"application/json",otaStateJson());return;}
    if(!otaBusy){s.send(409,"text/plain","No active session");return;}
    if(otaReceived!=otaExpected){abortOta("Incomplete image");s.send(400,"text/plain",otaError);return;}
    unsigned char digest[32];char hex[65];
    if(!otaHashLive||mbedtls_sha256_finish(&otaHash,digest)!=0){abortOta("Image digest finish failed");s.send(500,"text/plain",otaError);return;}
    freeOtaHash();digestHex(digest,hex);
    if(otaSha!=hex){abortOta("Whole image SHA256 mismatch");s.send(400,"text/plain",otaError);return;}
    if(!Update.end(false)){String e=Update.errorString();abortOta(e.c_str());s.send(400,"text/plain",otaError);return;}
    otaSuccess=true;rememberOta(2);otaRebootAt=millis()+2000UL;
    appLog.printf("[OTA] image verified; reboot scheduled in 2 s");
    s.send(200,"application/json",otaStateJson());
  });
  s.on("/update/abort",HTTP_POST,[&s,sessionOK](){if(!sessionOK())return;if(otaSuccess){s.send(409,"text/plain","Already verified; reboot pending");return;}if(otaBusy)abortOta("Cancelled by administrator");s.send(200,"application/json",otaStateJson());});
  s.on("/update",HTTP_POST,[&s,&cfg](){if(!otaAuth(s,cfg,false))return;s.send(405,"text/plain","Reopen /update and use the v0.9 retry-capable uploader.");});
}

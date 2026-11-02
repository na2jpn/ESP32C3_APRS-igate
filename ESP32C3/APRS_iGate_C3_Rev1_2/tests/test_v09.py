from pathlib import Path
import subprocess,tempfile,hashlib,json
root=Path(__file__).resolve().parent.parent
source=(root/'tests/reference/v07_host_fixture.txt').read_text()
# Reuse mocks and position regressions; no old OTA test is executed.
exec(source[:source.index("test=r'''" )])
stub=stub.replace('#define FW_VERSION "0.7"','#define FW_VERSION "0.9"')
shaShim='''#pragma once
#include <openssl/evp.h>
inline bool failHashStart=false,failHashUpdate=false,failHashFinish=false;
struct mbedtls_sha256_context{EVP_MD_CTX *ctx;};
inline void mbedtls_sha256_init(mbedtls_sha256_context*h){h->ctx=EVP_MD_CTX_new();}
inline void mbedtls_sha256_free(mbedtls_sha256_context*h){EVP_MD_CTX_free(h->ctx);h->ctx=nullptr;}
inline int mbedtls_sha256_starts(mbedtls_sha256_context*h,int){return failHashStart?-1:(EVP_DigestInit_ex(h->ctx,EVP_sha256(),nullptr)==1?0:-1);}
inline int mbedtls_sha256_update(mbedtls_sha256_context*h,const unsigned char*p,size_t n){return failHashUpdate?-1:(EVP_DigestUpdate(h->ctx,p,n)==1?0:-1);}
inline int mbedtls_sha256_finish(mbedtls_sha256_context*h,unsigned char*p){unsigned n;return failHashFinish?-1:(EVP_DigestFinal_ex(h->ctx,p,&n)==1?0:-1);}
'''
# Whole-image identity for 4 bytes and binary multi-block data.
sha4=hashlib.sha256(bytes(range(4))).hexdigest()
data=bytes(range(256))*16+b'\0';shaLarge=hashlib.sha256(data).hexdigest()
test=r'''
#include "app_log.h"
#include "position_beacon.h"
#include "web_ota.h"
#include "web_log.h"
bool paused=false;void maintenance(bool b){paused=b;}
int main(){
'''+previous+r'''
 appLog.clear();for(int i=0;i<200;i++)appLog.printf("event-%03d abcdefghijklmnopqrstuvwxyz",i);
 assert(appLog.used()<=4096&&appLog.snapshot().find("event-199")!=String::npos&&appLog.snapshot().find("event-000")==String::npos);
 auto used=appLog.used();appLog.printf("[HEALTH] omitted");assert(appLog.used()==used);appLog.add("test\nforged");assert(appLog.snapshot().find("test forged")!=String::npos);
 WebServer w;setupOtaRoutes(w,c,maintenance);setupLogRoutes(w,c);
 w.gets["/update"]();assert(w.code==403);c.otaPassword="password123";w.auth=false;w.gets["/update"]();assert(w.code==401);w.gets["/logs"]();assert(w.code==401);
 w.auth=true;w.gets["/update"]();assert(w.code==200);std::ofstream("/tmp/igate-ota09.html")<<w.body;
 w.args["lang"]="en";w.gets["/update"]();assert(w.code==200);std::ofstream("/tmp/igate-ota12-en.html")<<w.body;w.args.erase("lang");
 w.gets["/logs"]();std::ofstream("/tmp/igate-logs12-ja.html")<<w.body;w.args["lang"]="en";w.gets["/logs"]();std::ofstream("/tmp/igate-logs12-en.html")<<w.body;w.args.erase("lang");
 havePartition=false;w.gets["/update"]();assert(w.code==409);havePartition=true;
 w.args["client"]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";w.args["sha256"]="SHA4";w.args["size"]="4";
 w.args["token"]="wrong";w.posts["/update/begin"]();assert(w.code==403&&!otaBusy);
 w.args["token"]=otaToken;w.args["sha256"]="bad";w.posts["/update/begin"]();assert(w.code==400&&!otaBusy);w.args["sha256"]="SHA4";
 uint32_t n;assert(!parseOtaNumber("4294967296",n)&&!parseOtaNumber("-1",n)&&!parseOtaNumber("4x",n));
 auto begin=[&](int size=4,const char *hash="SHA4"){
   w.args["size"]=std::to_string(size);w.args["sha256"]=hash;w.posts["/update/begin"]();assert(w.code==200&&otaBusy&&paused);w.args["id"]=std::to_string(otaSession);
 };
 auto chunk=[&](const char *off,const String &hex){w.args["offset"]=off;w.args["plain"]=hex;w.posts["/update/chunk"]();};
 auto cancel=[&](){w.posts["/update/abort"]();assert(w.code==200&&!otaBusy&&!paused&&!otaHashLive);};
 begin();auto id=otaSession;auto beganAt=otaActivity;int begins=Update.begins;nowMs+=100;w.posts["/update/begin"]();assert(w.code==200&&otaSession==id&&Update.begins==begins&&otaActivity==beganAt);
 w.args["client"]="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";w.posts["/update/begin"]();assert(w.code==409&&otaSession==id);w.args["client"]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
 w.args["id"]="0";chunk("0","00010203");assert(w.code==409&&otaReceived==0);w.args["id"]=std::to_string(id);
 chunk("1","00010203");assert(w.code==409&&!otaReceived);chunk("0","0001");assert(w.code==200&&otaReceived==2);auto activity=otaActivity;
 nowMs+=100;chunk("0","0001");assert(w.code==200&&otaReceived==2&&Update.data.size()==2&&otaActivity==activity);
 chunk("0","0002");assert(w.code==409&&Update.data.size()==2);cancel();w.posts["/update/abort"]();assert(w.code==200); // abort ACK loss
 begin();chunk("0","zz");assert(w.code==400&&!otaBusy);
 begin();chunk("0","0");assert(w.code==400&&!otaBusy);
 begin();chunk("0","0001020304");assert(w.code==400&&!otaBusy);
 begin();Update.writeOK=false;chunk("0","00010203");assert(w.code==500&&!otaBusy&&!paused);Update.writeOK=true;
 begin();w.posts["/update/finish"]();assert(w.code==400&&!otaBusy);
 begin();nowMs+=120001;serviceOtaTimeout();assert(otaBusy);nowMs+=480000;serviceOtaTimeout();assert(!otaBusy&&!paused&&!otaHashLive);
 nowMs=0xffff0000;begin();nowMs+=600001;serviceOtaTimeout();assert(!otaBusy); // wrap
 begin();chunk("0","00010204");w.posts["/update/finish"]();assert(w.code==400&&!otaBusy&&otaError=="Whole image SHA256 mismatch");
 begin();chunk("0","00010203");Update.endOK=false;w.posts["/update/finish"]();assert(w.code==400&&!otaBusy);Update.endOK=true;
 Update.beginOK=false;w.posts["/update/begin"]();assert(w.code==500&&!otaBusy&&!paused);Update.beginOK=true;
 begin();chunk("0","00010203");failHashFinish=true;w.posts["/update/finish"]();assert(w.code==500&&!otaBusy&&!otaHashLive);failHashFinish=false;
 failHashStart=true;w.posts["/update/begin"]();assert(w.code==500&&!otaBusy&&!otaHashLive&&!paused);failHashStart=false;
 begin();failHashUpdate=true;chunk("0","00010203");assert(w.code==500&&!otaBusy);failHashUpdate=false;
 partition.size=10000;begin(4097,"SHALARGE");String hex;for(int i=0;i<2048;i++){char v[3];snprintf(v,3,"%02x",i&255);hex+=v;}
 chunk("0",hex);assert(w.code==200);chunk("0",hex);assert(w.code==200&&otaReceived==2048&&Update.data.size()==2048);
 chunk("2048",hex);chunk("4096","00");assert(w.code==200&&otaReceived==4097&&Update.data.size()==4097);
 nowMs=0xfffff830u;w.posts["/update/finish"]();assert(w.code==200&&otaSuccess&&!otaHashLive&&otaRebootAt==0); // scheduled exactly at rollover
 w.posts["/update/finish"]();assert(w.code==200&&otaSuccess);w.posts["/update/abort"]();assert(w.code==409&&otaSuccess);
 nowMs+=1999;serviceOtaTimeout();assert(ESP.restarts==0);++nowMs;serviceOtaTimeout();assert(ESP.restarts==1);
 assert(otaRecordValid()&&otaRecord.state==2);otaToken="newboot";otaBusy=false;otaSuccess=false;
 w.gets["/update/status"]();assert(w.code==200);std::ofstream("/tmp/igate-state09.json")<<w.body;
 appLog.clear();logOtaBoot();assert(appLog.snapshot().find("verified before reboot")!=String::npos);
 otaRecord.sha[0]^=1;assert(!otaRecordValid());w.gets["/update/status"]();assert(w.body.find("\"previous_verified\":false")!=String::npos);
 otaError="quote\"\\\n";std::ofstream("/tmp/igate-error09.json")<<otaStateJson();
 w.args["token"]="bad";w.posts["/logs/clear"]();assert(w.code==403);w.args["token"]=otaToken;w.posts["/logs/clear"]();assert(w.code==200);
 puts("PASS: real OTA SHA256/session/idempotent begin/replayed chunks/offsets/auth/cancel/10min timeout/wrap/verification/RTC/JSON; existing log and position regressions.");
}
'''
test=test.replace('SHA4',sha4).replace('SHALARGE',shaLarge)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'shim.h').write_text(stub);(p/'freertos').mkdir();(p/'mbedtls').mkdir();(p/'mbedtls/sha256.h').write_text(shaShim)
 for name in ['Arduino.h','Update.h','esp_ota_ops.h','esp_system.h','esp_attr.h','freertos/FreeRTOS.h']:(p/name).write_text('#include "shim.h"\n')
 (p/'test.cpp').write_text('#include "shim.h"\n'+write_test+test)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+d,'-I'+str(root),str(p/'test.cpp'),'-lcrypto','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
state=json.loads(Path('/tmp/igate-state09.json').read_text());assert state['previous_verified'] and state['previous_sha256']==shaLarge and state['previous_size']==4097
assert json.loads(Path('/tmp/igate-error09.json').read_text())['error']=='quote"\\ '

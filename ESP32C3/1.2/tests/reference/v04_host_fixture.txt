#!/usr/bin/env python3
"""Host tests of real position/OTA headers; flash/network APIs are mocks."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parent.parent
stub=r'''
#pragma once
#include <string>
#include <map>
#include <functional>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstdlib>
#include <cstdio>
struct String:std::string {
 using std::string::string; using std::string::operator=;
 String(const std::string&s):std::string(s){}
 size_t length()const{return size();}
 long toInt()const{return strtol(c_str(),nullptr,10);}
 bool endsWith(const char*s)const{std::string v=s;return size()>=v.size()&&substr(size()-v.size())==v;}
 void replace(const char*a,const char*b){size_t p=0;while((p=find(a,p))!=npos){std::string::replace(p,std::string(a).size(),b);p+=std::string(b).size();}}
};
uint32_t nowMs=0; uint32_t millis(){return nowMs;}
void delay(int){}
#define DBG_PRINTF(...) ((void)0)
#define DBG_PRINTLN(...) ((void)0)
struct AppConfig{bool beaconEnabled=false;String latitude,longitude,comment="test",callsign="JH1HST-10",otaPassword;};
struct AprsIsClient{bool verifiedValue=true,sendOK=true;int sends=0;String last;bool verified(){return verifiedValue;}bool sendRfPacket(const String&s){++sends;last=s;return sendOK;}};
struct Esp{int restarts=0;void restart(){++restarts;}} ESP;
constexpr int HTTP_GET=0,HTTP_POST=1;
constexpr int UPLOAD_FILE_START=0,UPLOAD_FILE_WRITE=1,UPLOAD_FILE_END=2,UPLOAD_FILE_ABORTED=3;
struct HTTPUpload{int status=0;String name="firmware",filename="firmware.bin";uint8_t buf[16]{};size_t currentSize=0,totalSize=0;};
struct WebServer{std::map<std::string,std::function<void()>> gets,posts,uploads;std::map<std::string,String> args;HTTPUpload u;bool auth=true;int code=0;String body;
 void on(const char*p,int method,std::function<void()>f){(method==HTTP_GET?gets:posts)[p]=f;}
 void on(const char*p,int method,std::function<void()>f,std::function<void()>up){on(p,method,f);uploads[p]=up;}
 bool authenticate(const char*,const char*){return auth;}
 void requestAuthentication(){code=401;}
 void send(int c,const char*,const String&s){code=c;body=s;}
 void sendHeader(const char*,const char*){}
 String arg(const char*k){return args[k];} HTTPUpload&upload(){return u;}
};
constexpr int U_FLASH=0;
struct Updater{bool running=false,beginOK=true,writeOK=true,endOK=true;int begins=0,aborts=0;size_t received=0,expected=0;
 bool begin(size_t n,int){++begins;received=0;expected=n;running=beginOK;return beginOK;}
 void abort(){++aborts;running=false;}
 size_t write(uint8_t*,size_t n){if(!writeOK)return 0;received+=n;return n;}
 bool end(bool evenIfRemaining){assert(!evenIfRemaining);running=false;return endOK&&received==expected;}
 const char*errorString(){return "mock update error";}
} Update;
struct esp_partition_t{size_t size=1024;const char*label="ota_1";} partition;
bool havePartition=true;
const esp_partition_t*esp_ota_get_next_update_partition(void*){return havePartition?&partition:nullptr;}
uint32_t esp_random(){return 1234;}
'''
aprs=(root/'aprsis.h').read_text()
a=aprs.index('  bool sendRfPacket(');b=aprs.index('\n  bool connected()',a)
write_test='''
struct WriteTest {
 struct Tcp {int bytes=0, calls=0;bool connectedValue=true; bool connected(){return connectedValue;}size_t print(const String&){++calls;return bytes;}} _client;
 bool _verified=true; uint32_t _lastGood=0,_sent=0;
 void disconnect(){_client.connectedValue=false;_verified=false;}
'''+aprs[a:b]+'};\n'
test=r'''
#include "position_beacon.h"
#include "web_ota.h"
int main(){
 WriteTest wt;wt._client.bytes=5;assert(wt.sendRfPacket("abc"));assert(wt._sent==1);
 wt._client.bytes=2;assert(!wt.sendRfPacket("abc"));assert(!wt._client.connectedValue&&wt._sent==1);
 int calls=wt._client.calls;assert(!wt.sendRfPacket("abc"));assert(wt._client.calls==calls);
 AppConfig c; double a,b;
 assert(parsePosition("35.1234","139.1234",a,b));
 for(auto s:{"", "NaN", "inf", "1e2", "12x", "--2", "1.2.3", " 2", "91"})assert(!parsePosition(s,"0",a,b));
 assert(!parsePosition("0","181",a,b));
 c.latitude="35.1234";c.longitude="139.1234";
 assert(positionPacket(c)=="JH1HST-10>APRS,TCPIP*:!3507.40NI13907.40E&test");
 c.latitude="-12.9999999";c.longitude="-179.9999999";
 assert(positionPacket(c)=="JH1HST-10>APRS,TCPIP*:!1300.00SI18000.00W&test");
 c.latitude="90";c.longitude="180";assert(positionPacket(c).find("9000.00NI18000.00E&")!=String::npos);
 c.comment="a\r\nb";assert(positionPacket(c).find('\n')==String::npos);
 AprsIsClient cl;servicePositionBeacon(cl,c);assert(cl.sends==0);
 c.beaconEnabled=true;cl.verifiedValue=false;servicePositionBeacon(cl,c);assert(cl.sends==0);
 cl.verifiedValue=true;cl.sendOK=false;servicePositionBeacon(cl,c);assert(cl.sends==1&&beaconSentCount==0);
 nowMs=59999;servicePositionBeacon(cl,c);assert(cl.sends==1);
 nowMs=60000;cl.sendOK=true;servicePositionBeacon(cl,c);assert(cl.sends==2&&beaconSentCount==1);
 nowMs+=1799999;servicePositionBeacon(cl,c);assert(cl.sends==2);
 ++nowMs;servicePositionBeacon(cl,c);assert(cl.sends==3);
 beaconLastSent=0xffff0000u;beaconLastAttempt=beaconLastSent;nowMs=uint32_t(beaconLastSent+1800000u);servicePositionBeacon(cl,c);assert(cl.sends==4);
 WebServer w;setupOtaRoutes(w,c);w.gets["/update"]();assert(w.code==403);
 c.otaPassword="password123";w.auth=false;w.gets["/update"]();assert(w.code==401);
 w.auth=true;havePartition=false;w.gets["/update"]();assert(w.code==409);havePartition=true;
 w.args["token"]="wrong";w.args["size"]="4";w.u.status=UPLOAD_FILE_START;w.uploads["/update"]();assert(!otaBusy&&Update.begins==0);
 w.args["token"]=otaToken;w.args["size"]="2048";w.uploads["/update"]();assert(!otaBusy&&Update.begins==0);
 auto start=[&](){w.args["size"]="4";w.u.status=UPLOAD_FILE_START;w.uploads["/update"]();assert(otaBusy);};
 auto write=[&](int n){w.u.status=UPLOAD_FILE_WRITE;w.u.currentSize=n;w.uploads["/update"]();};
 auto end=[&](int n){w.u.status=UPLOAD_FILE_END;w.u.totalSize=n;w.uploads["/update"]();};
 start();write(2);end(2);assert(!otaSuccess&&!otaBusy);w.posts["/update"]();assert(w.code==400&&ESP.restarts==0);
 start();write(5);assert(!otaBusy&&!otaSuccess);
 start();w.u.status=UPLOAD_FILE_ABORTED;w.uploads["/update"]();assert(!otaBusy&&!otaSuccess);
 start();nowMs+=30001;serviceOtaTimeout();assert(!otaBusy&&!otaSuccess);
 start();Update.writeOK=false;write(4);assert(!otaBusy);Update.writeOK=true;
 start();write(4);Update.endOK=false;end(4);assert(!otaSuccess);Update.endOK=true;
 start();write(4);end(4);assert(otaSuccess&&!otaBusy);w.posts["/update"]();assert(w.code==200&&ESP.restarts==1);
 puts("PASS: position formatting/ranges/rollover/retry/rate, OTA auth/token/slot/size/truncation/abort/timeout/write/verification/success.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'shim.h').write_text(stub)
 for name in ['Update.h','esp_ota_ops.h','esp_system.h']:(p/name).write_text('#include "shim.h"\n')
 (p/'test.cpp').write_text('#include "shim.h"\n'+write_test+test)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+d,'-I'+str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

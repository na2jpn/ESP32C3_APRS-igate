#!/usr/bin/env python3
"""Real C++ logger, position, TCP send and chunk OTA handlers with API mocks."""
from pathlib import Path
import ast, subprocess,tempfile
root=Path(__file__).resolve().parent.parent
old=(root/'tests/reference/v04_host_fixture.txt').read_text();tree=ast.parse(old)
stub=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stub' for t in n.targets))
stub=stub.replace('#include <string>','#include <string>\n#include <type_traits>\n#include <vector>')
stub=stub.replace(' size_t length()const', ''' template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0> String(T n):std::string(std::to_string(n)){}
 template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0> String&operator+=(T n){std::string::append(std::to_string(n));return *this;}
 using std::string::operator+=;
 size_t length()const''')
stub=stub.replace('void restart(){++restarts;}','void restart(){++restarts;}unsigned getFreeHeap(){return 150000;}')
stub=stub.replace('size_t received=0,expected=0;','size_t received=0,expected=0;std::vector<uint8_t> data;')
stub=stub.replace('++begins;received=0;','++begins;data.clear();received=0;')
stub=stub.replace('size_t write(uint8_t*,size_t n){if(!writeOK)return 0;received+=n;return n;}','size_t write(uint8_t*p,size_t n){if(!writeOK)return 0;data.insert(data.end(),p,p+n);received+=n;return n;}')
stub=stub.replace('uint32_t esp_random(){return 1234;}','uint32_t esp_random(){static uint32_t n=1234;return ++n;}')
stub+='#include <fstream>\n'
stub+='''
#define F(x) x
#define RTC_NOINIT_ATTR
#define FW_VERSION "0.7"
int esp_reset_reason(){return 3;}
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
'''
aprs=(root/'aprsis.h').read_text();a=aprs.index('  bool sendRfPacket(');b=aprs.index('\n  bool connected()',a)
write_test='''struct WriteTest {struct Tcp{int bytes=0,calls=0;bool connectedValue=true;bool connected(){return connectedValue;}size_t print(const String&){++calls;return bytes;}}_client;bool _verified=true;uint32_t _lastGood=0,_sent=0;void disconnect(){_client.connectedValue=false;_verified=false;}'''+aprs[a:b]+'};\n'
# Preserve all coordinate/rate/partial-write regressions from the old test.
previous=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='test' for t in n.targets))
previous=previous[previous.index(' WriteTest wt;'):previous.index(' WebServer w;')]
test=r'''
#include "app_log.h"
#include "position_beacon.h"
#include "web_ota.h"
#include "web_log.h"
bool paused=false;void maintenance(bool b){paused=b;}
int main(){
'''+previous+r'''
 appLog.clear();for(int i=0;i<200;++i)appLog.printf("event-%03d abcdefghijklmnopqrstuvwxyz",i);
 assert(appLog.used()<=4096);String lines=appLog.snapshot();assert(lines.back()=='\n');assert(lines.find("event-199")!=String::npos);assert(lines.find("event-000")==String::npos);assert(lines[0]=='[');
 size_t len=appLog.used();appLog.printf("[HEALTH] omitted");appLog.printf("RF: omitted");assert(appLog.used()==len);
 appLog.clear();assert(appLog.snapshot().empty());appLog.add("test\nforged");assert(appLog.snapshot().find("test forged")!=String::npos);
 WebServer w;setupOtaRoutes(w,c,maintenance);setupLogRoutes(w,c);
 w.gets["/logs"]();assert(w.code==200);w.gets["/update"]();assert(w.code==403);
 c.otaPassword="password123";w.auth=false;w.gets["/logs"]();assert(w.code==401);w.gets["/update"]();assert(w.code==401);
 w.auth=true;w.gets["/update"]();assert(w.code==200);std::ofstream("/tmp/igate-ota07.html")<<w.body;havePartition=false;w.gets["/update"]();assert(w.code==409);havePartition=true;
 w.args["token"]="wrong";w.args["size"]="4";w.posts["/update/begin"]();assert(w.code==403&&!otaBusy);
 w.args["token"]=otaToken;w.args["size"]="2048";w.posts["/update/begin"]();assert(w.code==400&&!otaBusy);
 uint32_t number;assert(!parseOtaNumber("-1",number));assert(!parseOtaNumber("4294967296",number));assert(!parseOtaNumber("4x",number));
 auto begin=[&](int size=4){w.args["size"]=std::to_string(size);w.posts["/update/begin"]();assert(w.code==200&&otaBusy&&paused);w.args["id"]=std::to_string(otaSession);};
 auto chunk=[&](const char*off,const String&hex){w.args["offset"]=off;w.args["plain"]=hex;w.posts["/update/chunk"]();};
 auto cancel=[&](){w.posts["/update/abort"]();assert(w.code==200&&!otaBusy&&!paused);};
 begin();auto goodId=w.args["id"];w.args["id"]="1";chunk("0","00010203");assert(w.code==409&&otaReceived==0);w.args["id"]=goodId;
 chunk("1","00010203");assert(w.code==409&&otaReceived==0);chunk("0","0001");assert(w.code==200&&otaReceived==2);
 w.posts["/update/finish"]();assert(w.code==400&&!otaBusy&&!paused&&!otaSuccess);
 begin();chunk("0","zz");assert(w.code==400&&!otaBusy&&!paused);
 begin();chunk("0","0001020304");assert(w.code==400&&!otaBusy);
 begin();chunk("0","0");assert(w.code==400&&!otaBusy);
 begin();Update.writeOK=false;chunk("0","00010203");assert(w.code==500&&!otaBusy&&!paused);Update.writeOK=true;
 begin();cancel();assert(otaRecord.state==3&&otaRecord.checksum==otaChecksum(otaRecord));
 begin();nowMs+=120001;serviceOtaTimeout();assert(!otaBusy&&!paused);
 nowMs=0xffff0000u;begin();nowMs+=120001;serviceOtaTimeout();assert(!otaBusy);
 begin();chunk("0","00010203");Update.endOK=false;w.posts["/update/finish"]();assert(w.code==400&&!otaBusy&&!paused);Update.endOK=true;
 // A whole image in multiple blocks preserves every byte, including zero.
 partition.size=10000;begin(4097);
 String hex;std::vector<uint8_t> expected;for(int i=0;i<2048;++i){char b[3];snprintf(b,sizeof(b),"%02x",i&255);hex+=b;expected.push_back(i&255);}
 chunk("0",hex);assert(w.code==200&&otaReceived==2048);
 chunk("0",hex);assert(w.code==409&&otaReceived==2048); // Lost ACK: never write duplicate data.
 chunk("2048",hex);assert(w.code==200&&otaReceived==4096);expected.insert(expected.end(),expected.begin(),expected.end());
 chunk("4096","00");expected.push_back(0);assert(Update.data==expected);
 w.posts["/update/finish"]();assert(w.code==200&&otaSuccess&&otaBusy&&ESP.restarts==0&&paused);
 nowMs+=1999;serviceOtaTimeout();assert(ESP.restarts==0);++nowMs;serviceOtaTimeout();assert(ESP.restarts==1);
 appLog.clear();logOtaBoot();assert(appLog.snapshot().find("verified before reboot")!=String::npos);
 w.gets["/logs.txt"]();assert(w.code==200&&w.body.find("[PREVIOUS OTA]")!=String::npos);
 w.args["token"]="wrong";w.posts["/logs/clear"]();assert(w.code==403);
 w.args["token"]=otaToken;w.posts["/logs/clear"]();assert(w.code==200&&appLog.snapshot().find("cleared by administrator")!=String::npos);
 puts("PASS: 4KB log eviction/escape/auth/clear; OTA token/session/size/order/duplicates/hex/binary integrity/cancel/timeout/rollover/write/verify/deferred reboot/RTC; position/TCP regressions.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'shim.h').write_text(stub);(p/'freertos').mkdir()
 for name in ['Arduino.h','Update.h','esp_ota_ops.h','esp_system.h','esp_attr.h','freertos/FreeRTOS.h']:(p/name).write_text('#include "shim.h"\n')
 (p/'test.cpp').write_text('#include "shim.h"\n'+write_test+test)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+d,'-I'+str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

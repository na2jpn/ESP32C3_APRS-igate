"""Execute production settings, RF select, DRA UART and save route with host APIs."""
from pathlib import Path
import subprocess, tempfile

root=Path(__file__).resolve().parent.parent
source=(root/'APRS_iGate_C3_Rev1_2.ino').read_text()
start=source.index('  web.on("/save", HTTP_POST')
end=source.index('\n\n  web.on("/reboot"', start)
save_route=source[start:end]
start=source.index('  if (!draReady && millis() - lastDraAttempt > 30000UL)')
end=source.index('\n\n  serviceLeds();',start)
retry=source[start:end]

shim=r'''
#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <algorithm>
#include <type_traits>
#include <cctype>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <cstdlib>
struct String:std::string {
 using std::string::string;using std::string::operator=;using std::string::operator+=;
 String(const std::string&s):std::string(s){}
 template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>String(T n):std::string(std::to_string(n)){}
 size_t length()const{return size();}
 int indexOf(char c)const{auto p=find(c);return p==npos?-1:int(p);}
 int indexOf(const char*s)const{auto p=find(s);return p==npos?-1:int(p);}
 void replace(const char*a,const char*b){size_t p=0;while((p=find(a,p))!=npos){std::string::replace(p,std::string(a).size(),b);p+=std::string(b).size();}}
 String substring(size_t first,size_t end=npos)const{return substr(first,end==npos?npos:end-first);}
 void trim(){auto a=find_first_not_of(" \r\n\t");if(a==npos){clear();return;}auto b=find_last_not_of(" \r\n\t");*this=substr(a,b-a+1);}
 void toUpperCase(){for(auto &c:*this)c=std::toupper(static_cast<unsigned char>(c));}
 long toInt()const{return std::strtol(c_str(),nullptr,10);}
};
inline bool isAlphaNumeric(char c){return std::isalnum(static_cast<unsigned char>(c));}
#define F(x) x
inline uint32_t nowMs=0;
inline uint32_t millis(){return nowMs;}
inline void delay(unsigned n){nowMs+=n;}
inline bool failRead=false,failWrite=false;
inline int writes=0;
inline std::map<std::string,String> nv;
struct Preferences {
 bool begin(const char*,bool read){return read?!failRead:!failWrite;}
 String getString(const char*k,const char*d){return nv.count(k)?nv[k]:String(d);}
 uint16_t getUShort(const char*k,uint16_t d){return nv.count(k)?uint16_t(nv[k].toInt()):d;}
 bool getBool(const char*k,bool d){return nv.count(k)?nv[k]=="1":d;}
 void putString(const char*k,const String&v){nv[k]=v;++writes;}
 void putUShort(const char*k,uint16_t v){putString(k,String(v));}
 void putBool(const char*k,bool v){putString(k,v?"1":"0");}
 void clear(){nv.clear();}void end(){}
};
constexpr int SERIAL_8N1=0,HTTP_POST=1;
struct HardwareSerial {
 unsigned baud=0;int rx=0,tx=0;String pending,reply;std::vector<String> commands;
 void begin(unsigned b,int,int r,int t){baud=b;rx=r;tx=t;}
 size_t print(const String&s){
  pending+=s;
  if(pending.ends_with("\r\n")){
   pending.resize(pending.size()-2);commands.push_back(pending);
   if(pending=="AT+DMOCONNECT")reply="+DMOCONNECT:0";
   else if(pending.find("AT+DMOSETGROUP=")==0)reply="+DMOSETGROUP:0";
   else reply="OK";
   pending.clear();
  }return s.size();
 }
 int available(){return reply.size();}
 int read(){char c=reply.front();reply.erase(0,1);return c;}
};
struct WebServer {
 std::map<std::string,String> args;std::function<void()> handler;int code=0;String body;bool auth=true;
 void on(const char*,int,std::function<void()>f){handler=f;}
 bool hasArg(const char*k){return args.count(k);}
 String arg(const char*k){auto i=args.find(k);return i==args.end()?String():i->second;}
 bool authenticate(const char*,const char*){return auth;}
 void requestAuthentication(){code=401;}
 void send(int c,const char*,const String&s){code=c;body=s;}
};
'''
code=r'''
#include "settings.h"
#include "web_rf_settings.h"
#include "ui_page.h"
#include "dra818v.h"
SettingsStore settingsStore;AppConfig config;WebServer web;bool otaBusy=false,reboot=false;
String rebootResponse(const char*s){return s;}
void scheduleReboot(){reboot=true;}
bool parsePosition(const String&,const String&,double&,double&){return true;}
HardwareSerial port;DRA818VController dra(port);bool draReady=false;uint32_t lastDraAttempt=0;
void routes(){
'''+save_route+r'''
}
void retryDra(){
'''+retry+r'''
}
int main(){
 AppConfig empty;assert(empty.rxFrequency=="144.6600");
 assert(settingsStore.load(empty)&&empty.rxFrequency=="144.6600");
 // Old NVS has no rx_freq key; preserve unrelated credentials and default to 660.
 nv["ssid"]="old-wifi";nv["wpass"]="old-pass";nv["call"]="JH1HST-10";nv["otapass"]="password123";
 AppConfig old;settingsStore.load(old);assert(old.rxFrequency=="144.6600"&&old.wifiPass=="old-pass"&&old.otaPassword=="password123");
 for(const char*freq:{"144.6400","144.6600","144.3900","144.8000","145.1750","144.5750","144.9300","145.5700"}){
  old.rxFrequency=freq;assert(settingsStore.save(old));AppConfig loaded;assert(settingsStore.load(loaded));
  assert(loaded.rxFrequency==freq&&loaded.wifiPass=="old-pass"&&loaded.otaPassword=="password123");
  const String label=SettingsStore::rxFrequencyLabel(loaded);assert(label==String(freq).substring(0,7));
  String html=uiLocalize(makeRfFrequencySelect(loaded),false);
  String selected=String("value='")+freq+"' selected";
  assert(html.find(selected)!=String::npos);
  auto at=html.find(" selected");assert(at!=String::npos&&html.find(" selected",at+1)==String::npos);
  assert(html.find("9600bps")!=String::npos&&html.find("保存・再起動")!=String::npos);
  port.commands.clear();assert(dra.begin(loaded.rxFrequency.c_str()));
  assert(port.baud==9600&&port.rx==PIN_DRA_RX&&port.tx==PIN_DRA_TX);
  assert(port.commands[1]==String("AT+DMOSETGROUP=0,")+freq+","+freq+",0000,0,0000");
  config=loaded;draReady=false;nowMs=100000;lastDraAttempt=0;port.commands.clear();retryDra();
  assert(draReady&&port.commands[1]==String("AT+DMOSETGROUP=0,")+freq+","+freq+",0000,0,0000");
 }
 for(const char*bad:{"","144.6500","garbage","144.6600,0000","144.640"}){
  nv["rx_freq"]=bad;AppConfig c;settingsStore.load(c);assert(c.rxFrequency=="144.6600"&&c.wifiSsid=="old-wifi");
 }
 nv["rx_freq"]=" 144.6400 ";AppConfig trimmed;settingsStore.load(trimmed);assert(trimmed.rxFrequency=="144.6400");
 config=old;config.rxFrequency="144.6600";routes();
 web.args={{"ssid","old-wifi"},{"call","JH1HST-10"},{"port","14580"},{"rxfreq","144.6400"}};
 web.auth=false;int count=writes;web.handler();assert(web.code==401&&writes==count&&!reboot);
 web.auth=true;otaBusy=true;web.handler();assert(web.code==409&&writes==count&&!reboot);otaBusy=false;
 for(const char*bad:{"","144.6500","144.640","9600","144.6600\r\nAT+DMOCONNECT"}){
  web.args["rxfreq"]=bad;web.handler();assert(web.code==400&&writes==count&&!reboot&&config.rxFrequency=="144.6600");
 }
 web.args["rxfreq"]="144.6400";web.handler();assert(web.code==200&&reboot&&nv["rx_freq"]=="144.6400");
 assert(config.rxFrequency=="144.6600"); // Changes apply on reboot, not mid-reception.
 AppConfig next;settingsStore.load(next);assert(next.rxFrequency=="144.6400"&&next.wifiPass=="old-pass"&&next.otaPassword=="password123");
 config=next;web.args.erase("rxfreq");reboot=false;web.handler();assert(web.code==200&&reboot&&nv["rx_freq"]=="144.6400");
 failWrite=true;web.args["rxfreq"]="144.6600";reboot=false;web.handler();assert(web.code==500&&!reboot&&nv["rx_freq"]=="144.6400");failWrite=false;
 settingsStore.clear();AppConfig reset;settingsStore.load(reset);assert(reset.rxFrequency=="144.6600");
 failRead=true;AppConfig failed;assert(!settingsStore.load(failed)&&failed.rxFrequency=="144.6600");
 puts("PASS: production RF NVS migration/default/save/reload/clear/invalid fallback; selected HTML; actual UART setup and retry for all 8 frequencies; save route auth/OTA/invalid/preserve/error/reboot semantics.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'Arduino.h').write_text(shim);(p/'Preferences.h').write_text('#include "Arduino.h"\n');(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I',d,'-I',str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

# Whole-sketch wiring uses the saved value at boot and retry, and one label source.
assert source.count('dra.begin(config.rxFrequency.c_str())')==2
assert 'page += makeRfFrequencySelect(config);' in source
assert 'lcd.printLine(0, SettingsStore::rxFrequencyLabel(config) + " RX ONLY")' in source
assert 'page += SettingsStore::rxFrequencyLabel(config);' in source

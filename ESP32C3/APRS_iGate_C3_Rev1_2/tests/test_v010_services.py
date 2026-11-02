from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parent.parent/'APRS_iGate_C3_Rev1_2.ino').read_text()
w=s[s.index('static bool wifiOutageActive='):s.index('\n// -----------------------------------------------------------------------------',s.index('static bool wifiOutageActive='))]
# Test actual production function, not a mirrored implementation.
a=s.index('static const char *adcState(');b=s.index('static void serviceAdcHealth()',a)
state=s[a:b]
a=s.index('  web.on("/adc/recover"');b=s.index('\n\n  web.on("/save"',a)
route=s[a:b]
code=r'''
#include <string>
#include <cassert>
#include <cstdint>
#include <functional>
using String=std::string;
uint32_t nowMs=0,lastWifiRetry=0,lastInternetGood=0,lastWifiStackReset=0,wifiAttemptStart=0;
uint32_t millis(){return nowMs;}void delay(int){}
using wl_status_t=int;constexpr int WL_CONNECTED=3,WL_NO_SHIELD=255,WIFI_OFF=0,HTTP_POST=1;
constexpr uint32_t WIFI_RETRY_MS=15000,WIFI_CONNECT_TIMEOUT_MS=60000,WIFI_STACK_RESET_MS=3600000;
bool setupApActive=false,adcReady=true,otaBusy=false;String otaToken="secret";
struct Config{String wifiSsid="configured",wifiPass="",otaPassword="password";}config;
struct IP{String toString(){return "IP";}};
struct Wifi{int state=3;int status(){return state;}IP localIP(){return{};}void disconnect(bool,bool){}void begin(const char*,const char*){}void mode(int){}}WiFi;
struct Aprs{void configure(Config&){}void connectNow(){}}aprs;
struct Log{template<class...T>void printf(const char*,T...){}}appLog;
#define DBG_PRINTF(...) ((void)0)
void updateMdns(){}void startSetupAp(){setupApActive=true;}void beginStationWifi(){}
struct Audio{bool paused=false,ready=true;int requested=0;bool maintenance(){return paused;}bool requestRecovery(){++requested;return ready;}}audio;
struct AfMonitorSnapshot{bool fault=false,recovering=false,driverRunning=true,dataSeen=true;uint32_t lastDataAtMs=0;};
struct Web{bool auth=true;String token="secret";int status=0;std::function<void()> handler;
bool authenticate(const char*,const char*){return auth;}void requestAuthentication(){status=401;}
String arg(const char*){return token;}void send(int n,const char*,const char*){status=n;}
void on(const char*,int,std::function<void()> f){handler=f;}}web;
'''+w+state+'\nconst char*uiText(const char*k,bool){return k;}bool uiEnglish(Web&){return false;}\nvoid routes(){\n'+route+'\n}\n'+r'''
int main(){
 nowMs=100000;serviceWifi();WiFi.state=0;nowMs=101000;serviceWifi();assert(!setupApActive);
 nowMs=146000;serviceWifi();assert(!setupApActive);nowMs=160999;serviceWifi();assert(!setupApActive);
 nowMs=161000;serviceWifi();assert(setupApActive);
 setupApActive=false;WiFi.state=3;nowMs=200000;serviceWifi();WiFi.state=0;nowMs=300000;serviceWifi();assert(!setupApActive);
 nowMs=359999;serviceWifi();assert(!setupApActive);nowMs=360000;serviceWifi();assert(setupApActive);
 AfMonitorSnapshot m;m.lastDataAtMs=nowMs;assert(String(adcState(m))=="OK");nowMs+=1501;assert(String(adcState(m))=="STALE");
 m.recovering=true;assert(String(adcState(m))=="RECOVERING");m.fault=true;assert(String(adcState(m))=="FAULT");
 audio.paused=true;assert(String(adcState(m))=="PAUSED");adcReady=false;assert(String(adcState(m))=="INIT FAILED");
 routes();web.auth=false;web.handler();assert(web.status==401&&!audio.requested);
 web.auth=true;web.token="bad";web.handler();assert(web.status==403&&!audio.requested);
 web.token="secret";otaBusy=true;web.handler();assert(web.status==409&&!audio.requested);
 otaBusy=false;audio.ready=false;web.handler();assert(web.status==503&&audio.requested==1);
 audio.ready=true;web.handler();assert(web.status==200&&audio.requested==2);
 config.otaPassword="";web.auth=false;web.handler();assert(web.status==200);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production Wi-Fi continuous-outage timer, ADC freshness states, recovery HTTP auth/token/OTA/queue failures/unconfigured password')

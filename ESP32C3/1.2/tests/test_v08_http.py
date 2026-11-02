from pathlib import Path
import runpy,subprocess,tempfile,re
root=Path(__file__).resolve().parent.parent
# Reuse the existing HTTP API mocks and execute inherited OTA/position regressions.
base=runpy.run_path(str(root/'tests/test_v09.py'))
stub=base['stub']
stub=stub.replace(' void sendHeader(', ' void setContentLength(int){} void sendContent(const char*s){body+=s;}\n void sendHeader(')
stub+='''
constexpr int CONTENT_LENGTH_UNKNOWN=-1;
#include <sys/time.h>
int ntpStarts=0;void(*syncCallback)(timeval*)=nullptr;
void sntp_set_time_sync_notification_cb(void(*f)(timeval*)){syncCallback=f;}
void configTime(int,int,const char*a,const char*b,const char*c){assert(a&&b&&c);++ntpStarts;}
'''
test=r'''
#include "app_log.h"
#include "rx_diagnostics.h"
#include "ntp_clock.h"
#include "web_ota.h"
#include "web_log.h"
struct AudioAdcCapture {
 RxDiagnostics data;
 void copyDiagnostics(RxDiagnostics &out){out=data;}
 void controlDiagnostics(unsigned n){if(n==0)data.frozen=true;if(n==1)data.frozen=false;if(n==2)data.clear();if(n==3)data.manual();}
};
#include "web_rx_diagnostics.h"
int main(){
 logOtaBoot();serviceOtaTimeout();
 serviceClock(false);assert(ntpStarts==0);serviceClock(true);serviceClock(true);assert(ntpStarts==1&&syncCallback);
 assert(clockStatus().find("UNSYNC")!=String::npos);
 timeval tv{};tv.tv_sec=1793491200;tv.tv_usec=456000;syncCallback(&tv);serviceClock(true);
 assert(clockStatus().find("JST")!=String::npos);assert(appLog.snapshot().find("NTP synchronized")!=String::npos);
 WebServer w;AppConfig cfg;AudioAdcCapture audio;setupOtaRoutes(w,cfg,nullptr);setupLogRoutes(w,cfg);setupRxDiagnosticRoutes(w,cfg,audio);
 cfg.otaPassword="password123";w.auth=false;w.gets["/rxdiag"]();assert(w.code==401);w.gets["/rxdiag.csv"]();assert(w.code==401);
 w.auth=true;w.gets["/rxdiag"]();assert(w.code==200);std::ofstream("/tmp/igate-rx08.html")<<w.body;
 w.args["lang"]="en";w.gets["/rxdiag"]();assert(w.code==200);std::ofstream("/tmp/igate-rx12-en.html")<<w.body;w.args.erase("lang");
 w.posts["/rxdiag/control"]();assert(w.code==403);w.args["token"]=otaToken;w.args["action"]="freeze";w.posts["/rxdiag/control"]();assert(w.code==200&&audio.data.frozen);
 w.args["action"]="resume";w.posts["/rxdiag/control"]();assert(!audio.data.frozen);
 w.args["action"]="manual";w.posts["/rxdiag/control"]();assert(w.code==200);
 RxDiagnosticRow row;row.time=deviceClock.now();row.adcMin=1700;row.adcMax=2700;audio.data.feed(row);row.time.uptimeMs+=20;row.crc=2;audio.data.feed(row);
 w.gets["/rxdiag.csv"]();assert(w.code==200);assert(w.body.find("delta_crc")!=String::npos);assert(w.body.find("JST")!=String::npos);std::ofstream("/tmp/igate-rx08.csv")<<w.body;
 w.args["action"]="invalid";w.posts["/rxdiag/control"]();assert(w.code==400);
 otaBusy=true;w.gets["/rxdiag.csv"]();assert(w.code==409);w.args["action"]="clear";w.posts["/rxdiag/control"]();assert(w.code==409&&audio.data.total);
 otaBusy=false;w.posts["/rxdiag/control"]();assert(w.code==200&&!audio.data.total);
 cfg.otaPassword="";w.auth=false;w.gets["/rxdiag"]();assert(w.code==200);
 puts("PASS: actual RX HTTP auth/token/control/OTA exclusion/CSV; actual NTP service starts once, sync callback/JST/logging.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'shim.h').write_text(stub);(p/'freertos').mkdir();(p/'mbedtls').mkdir();(p/'mbedtls/sha256.h').write_text(base['shaShim'])
 for name in ['Arduino.h','Update.h','esp_ota_ops.h','esp_system.h','esp_attr.h','esp_sntp.h','freertos/FreeRTOS.h']:(p/name).write_text('#include "shim.h"\n')
 (p/'test.cpp').write_text('#include "shim.h"\n'+test)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+d,'-I'+str(root),str(p/'test.cpp'),'-lcrypto','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
import csv
rows=list(csv.reader(x for x in Path('/tmp/igate-rx08.csv').read_text().splitlines() if not x.startswith('#')))
assert len(rows)>2 and all(len(row)==27 for row in rows)
assert rows[-1][20]=='2'
page=Path('/tmp/igate-rx08.html').read_text();Path('/tmp/igate-rx08.js').write_text(re.search('<script>(.*?)</script>',page,re.S).group(1))
subprocess.run(['node','--check','/tmp/igate-rx08.js'],check=True)

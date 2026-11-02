from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parent.parent
source=(root/'APRS_iGate_C3_Rev1_2.ino').read_text()
a=source.index('static void serviceOtaWifi()');b=source.index('\nstatic void updateMdns()',a);fn=source[a:b]
code=r'''
#include <cstdint>
#include <cassert>
struct Config{struct S{int length(){return 4;}}wifiSsid;}config;
struct Wifi{int state=3,retries=0;int status(){return state;}void reconnect(){++retries;}}WiFi;
struct Log{template<class...T>void printf(const char*,T...){}}appLog;
uint32_t nowMs=0,lastWifiRetry=0;uint32_t millis(){return nowMs;}
constexpr int WL_CONNECTED=3;constexpr uint32_t WIFI_RETRY_MS=15000;
'''+fn+r'''
int main(){serviceOtaWifi();assert(!WiFi.retries);WiFi.state=0;nowMs=15000;serviceOtaWifi();assert(WiFi.retries==1);nowMs=29000;serviceOtaWifi();assert(WiFi.retries==1);nowMs=30000;serviceOtaWifi();assert(WiFi.retries==2);WiFi.state=3;nowMs=60000;serviceOtaWifi();assert(WiFi.retries==2);lastWifiRetry=0xfffffff0u;nowMs=15000;WiFi.state=0;serviceOtaWifi();assert(WiFi.retries==3);}
'''
assert 'if (otaBusy) { serviceOtaWifi();' in source
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
print('PASS: actual OTA Wi-Fi reconnect service cadence/connected state/millis wrap; no mode reset or APRS connect in maintenance path.')

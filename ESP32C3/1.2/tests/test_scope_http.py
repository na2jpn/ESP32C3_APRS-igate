from pathlib import Path
import runpy,subprocess,tempfile,json
root=Path(__file__).resolve().parent.parent
base=runpy.run_path(str(root/'tests/test_v09.py'));stub='#define PROGMEM\n'+base['stub'].replace('#define FW_VERSION "0.9"','#define FW_VERSION "1.2"')
stub=stub.replace(' size_t length()const',' String(float n,int decimals){char out[64];snprintf(out,sizeof(out),"%.*f",decimals,double(n));assign(out);} size_t length()const')
test=r'''
#include "app_log.h"
#include "web_ota.h"
#include "web_log.h"
#include "audio_scope.h"
constexpr int AFSK_SAMPLE_RATE=26400;
struct AfMonitorSnapshot{bool fault=false,recovering=false,driverRunning=true,dataSeen=true;uint32_t lastDataAtMs=0;};
struct AudioAdcCapture{
 AudioScopeFrame frame;AfMonitorSnapshot health;bool paused=false;int copied=0;
 bool maintenance(){return paused;}void copyScope(AudioScopeFrame &out){++copied;out=frame;}
 AfMonitorSnapshot monitor(){return health;}
};
#include "web_audio_scope.h"
int main(){
 WebServer w;AppConfig cfg;AudioAdcCapture audio;setupAudioScopeRoutes(w,cfg,audio);
 cfg.otaPassword="password123";w.auth=false;w.gets["/audio"]();assert(w.code==401);
 w.gets["/audio/data"]();assert(w.code==401&&!audio.copied);
 w.auth=true;w.gets["/audio"]();assert(w.code==200&&w.body.find("audio/data")!=String::npos);
 std::ofstream("/tmp/igate-audio011.html")<<w.body;
 w.args["lang"]="en";w.gets["/audio"]();assert(w.code==200);std::ofstream("/tmp/igate-audio12-en.html")<<w.body;w.args.erase("lang");
 otaBusy=true;w.gets["/audio/data"]();assert(w.code==409&&!audio.copied);otaBusy=false;
 audio.paused=true;w.gets["/audio/data"]();assert(w.code==409&&!audio.copied);audio.paused=false;
 nowMs=100;audio.health.lastDataAtMs=100;
 w.gets["/audio/data"]();assert(w.code==200&&w.body.find("\"valid\":false")!=String::npos);
 audio.frame.valid=true;audio.frame.atMs=100;audio.frame.sequence=7;audio.frame.mean=2200;audio.frame.rms=100;audio.frame.meterRms=150;audio.frame.meterPeak5s=500;audio.frame.meterWindows=25;
 audio.frame.minimum=2000;audio.frame.maximum=2400;audio.frame.peak=200;
 for(unsigned i=0;i<AudioScopeFrame::BINS;++i){audio.frame.low[i]=2000+i;audio.frame.high[i]=2400+i;}
 w.gets["/audio/data"]();assert(w.code==200&&w.body.find("\"valid\":true")!=String::npos);
 std::ofstream("/tmp/igate-audio011.json")<<w.body;
 nowMs=1701;audio.health.lastDataAtMs=1701;w.gets["/audio/data"]();assert(w.body.find("\"valid\":false")!=String::npos); // old frame
 audio.frame.atMs=nowMs;audio.health.lastDataAtMs=0;w.gets["/audio/data"]();assert(w.body.find("STALE")!=String::npos&&w.body.find("\"valid\":false")!=String::npos);
 audio.health.fault=true;w.gets["/audio/data"]();assert(w.body.find("FAULT")!=String::npos);
 audio.health.fault=false;audio.health.recovering=true;w.gets["/audio/data"]();assert(w.body.find("RECOVERING")!=String::npos);
 cfg.otaPassword="";w.auth=false;w.gets["/audio"]();assert(w.code==200);
 puts("PASS: audio monitor production HTTP auth/unconfigured/OTA/maintenance/invalid/fresh/stale/fault/recovering and bounded JSON frame");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'shim.h').write_text(stub);(p/'freertos').mkdir();(p/'mbedtls').mkdir();(p/'mbedtls/sha256.h').write_text(base['shaShim'])
 for name in ['Arduino.h','Update.h','esp_ota_ops.h','esp_system.h','esp_attr.h','freertos/FreeRTOS.h']:(p/name).write_text('#include "shim.h"\n')
 (p/'test.cpp').write_text('#include "shim.h"\n'+test)
 subprocess.run(['g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-I',str(p),'-I',str(root),str(p/'test.cpp'),'-lcrypto','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
data=json.loads(Path('/tmp/igate-audio011.json').read_text());assert len(data['low'])==len(data['high'])==264;assert data['duration_ms']==40;assert data['rms']==100;assert data['firmware']=='1.2'
page=Path('/tmp/igate-audio011.html').read_text();script=(root/'audio_scope_ui.js').read_text()
for key,value in json.loads((root/'ui_texts.json').read_text()).items():script=script.replace('{{'+key+'}}',value['ja'])
assert script in page

assert data["meter_rms"]==150 and data["meter_peak5s"]==500 and data["meter_windows"]==25

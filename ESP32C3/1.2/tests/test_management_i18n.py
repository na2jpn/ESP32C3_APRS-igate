from pathlib import Path
import ast,json,re,subprocess,tempfile
from bs4 import BeautifulSoup
r=Path(__file__).resolve().parent.parent
source=(r/'APRS_iGate_C3_Rev1_2.ino').read_text()
tree=ast.parse((r/'tests/test_rf_settings.py').read_text())
shim=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='shim' for t in n.targets))
shim=shim.replace(' size_t length()const', ' String(float n,int decimals){char b[64];snprintf(b,sizeof(b),"%.*f",decimals,double(n));assign(b);}\n template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>String&operator+=(T n){append(std::to_string(n));return *this;}\n size_t length()const')
def function(start,end):
 a=source.index(start);return source[a:source.index(end,a)]
helpers=function('static String htmlEscape(', 'static uint32_t fnv1a(')+function('static String statusBadge(', 'static const char *adcState(')+function('static String rebootResponse(', 'static void scheduleReboot(')
page=function('static String makePage()', 'static void setupWebRoutes()')
code=r'''
#include <fstream>
#include "settings.h"
#include "ui_page.h"
#include "web_rf_settings.h"
AppConfig config;WebServer web;bool draReady=true;
constexpr int WL_CONNECTED=3;
struct Ip{String toString(){return "192.0.2.1";}};
struct Wifi{int status(){return WL_CONNECTED;}Ip localIP(){return {};}}WiFi;
struct Aprs{bool verified(){return true;}bool connected(){return true;}}aprs;
struct Monitor{uint32_t crcBad=0;float rms=.1;};
struct Audio{Monitor monitor(){return {};}}audio;
const char*adcState(const Monitor&){return "OK";}
String clockStatus(){return "UNSYNC";}
uint32_t rxPacketCount=3,gatedPacketCount=3,duplicateCount=0,blockedCount=0,beaconSentCount=1;
String lastRxLine="TEST>APRS:>literal {{button.save}}",otaToken="0123456789abcdef";
'''+helpers+page+r'''
int main(){
 config.wifiSsid="literal {{button.save}} & <input>";config.callsign="JH1HST-10";
 config.aprsPasscode="";config.grid="TEST";config.comment="User text {{rf.default}}";
 assert(!uiEnglish(web));web.args["lang"]="EN";assert(!uiEnglish(web));web.args["lang"]="en";assert(uiEnglish(web));web.args.clear();
 for(bool en:{false,true}){
  const char*lang=en?"en":"ja";String body=uiRenderPage(makePage(),en);
  assert(body.find("{{")==String::npos);
  assert(body.find("&#123;&#123;button.save&#125;&#125;")!=String::npos);
  assert(body.find(String("action='/save?lang=")+lang+"'")!=String::npos);
  assert(body.find(String("href='/audio?lang=")+lang+"'")!=String::npos);
  assert(body.find(en?"Receive frequency":"受信周波数")!=String::npos);
  std::ofstream(std::string("/tmp/igate-main12-")+lang+".html")<<body;
  String response=uiRenderPage(rebootResponse(uiText("saved",en)),en);
  assert(response.find(String("60;url=/?lang=")+lang)!=String::npos);
  assert(response.find(en?"Settings saved. Rebooting":"設定を保存しました")!=String::npos);
  std::ofstream(std::string("/tmp/igate-reboot12-")+lang+".html")<<response;
 }
 puts("PASS: actual main/reboot HTML in Japanese and English; default Japanese, navigation/form/redirect language, literal user data preserved.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'Arduino.h').write_text(shim);(p/'Preferences.h').write_text('#include "Arduino.h"\n');(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-I',d,'-I',str(r),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
texts=json.loads((r/'ui_texts.json').read_text())
for filename in ['audio_scope_template.html','audio_scope_ui.js','ota_uploader.js']:
 template=(r/filename).read_text()
 for key in re.findall(r'\{\{([a-z0-9_.]+)\}\}',template):assert key in texts,key
for name in ['main12-ja','main12-en','reboot12-ja','reboot12-en','ota09','ota12-en','audio011','audio12-en','rx08','rx12-en','logs12-ja','logs12-en']:
 page=Path('/tmp/igate-'+name+'.html').read_text()
 assert not re.search(r'\{\{[a-z0-9_.]+\}\}',page),name
 soup=BeautifulSoup(page,'html.parser')
 if name.startswith('main'):
  select=soup.select_one('select[name=rxfreq]');assert len(select.select('option'))==8
  assert [o['value'] for o in select.select('option[selected]')]==['144.6600']
  assert soup.select_one('input[name=ssid]')['value']=='literal {{button.save}} & <input>'
 for i,script in enumerate(soup.find_all('script')):
  q=Path('/tmp/igate-i18n-'+name+'-'+str(i)+'.js');q.write_text(script.string or script.get_text())
  subprocess.run(['node','--check',str(q)],check=True,capture_output=True)
 if name.endswith('-en'):
  visible=soup.get_text();visible=visible.replace('日本語','')
  assert not re.search('[\u3040-\u30ff\u4e00-\u9fff]',visible),(name,visible)
  assert soup.html.get('lang')=='en',name
print('PASS: 12 localized production pages; all preset options, user values, rendered JS syntax, English labels and resolved translation keys.')

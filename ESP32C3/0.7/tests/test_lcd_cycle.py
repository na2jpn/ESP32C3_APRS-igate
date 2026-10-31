#!/usr/bin/env python3
"""Compile actual LCD service with mocked display/status; verify timing."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parent.parent
source=(root/'APRS_iGate_C3_Rev0_7.ino').read_text()
a=source.index('static void serviceLcd()');b=source.index('// -----------------------------------------------------------------------------',a)
config=(root/'config.h').read_text()
constants='\n'.join(re.findall(r'^static constexpr uint(?:8|32)_t LCD_\w+\s*=.*?;',config,re.M))
code=r'''
#include <string>
#include <cstdint>
#include <cstdio>
#include <cassert>
struct String:std::string{using std::string::string;String(const std::string&s):std::string(s){}String(float,int):std::string("0.1234"){}String(uint32_t):std::string("0"){}size_t length()const{return size();}};
#define DBG_PRINTLN(...) ((void)0)
uint32_t nowMs=0;uint32_t millis(){return nowMs;}
uint32_t lcdOnUntil=0,lastLcdAutoWake=0,lastLcdUpdate=0,lcdPageSince=0,rxPacketCount=0,gatedPacketCount=0;
uint8_t lcdPage=0;bool lcdPresent=true,setupApActive=false;
String lastRxCall="TEST";
struct Cfg{String callsign="JH1HST-10";}config;
constexpr int WL_CONNECTED=3;
struct Wifi{int status(){return WL_CONNECTED;}}WiFi;
struct Aprs{bool verified(){return true;}}aprs;
struct Monitor{float rms=0;uint32_t crcBad=0;};struct Audio{Monitor monitor(){return {};}}audio;
struct Lcd{bool lit=false;int writes=0;bool backlight(){return lit;}void setBacklight(bool v){lit=v;}void printLine(int,const String&){++writes;}}lcd;
'''+constants+'\n'+source[a:b]+r'''
void checkAt(uint32_t t,bool on,int page=-1){nowMs=t;serviceLcd();assert(lcd.lit==on);if(page>=0)assert(lcdPage==page);}
int main(){
 checkAt(899999,false);
 checkAt(900000,true,0);assert(lcd.writes==2);
 checkAt(903500,true,0);checkAt(904000,true,1);
 checkAt(908000,true,2);checkAt(912000,true,0);checkAt(916000,true,1);checkAt(920000,true,2);checkAt(923500,true,2);checkAt(924000,false,2);
 checkAt(1799999,false);checkAt(1800000,true,0);
 // An active 60-second manual interval must not be shortened or reset.
 lcdOnUntil=2750000;lcdPage=1;lcdPageSince=2700000;lastLcdUpdate=2699500;
 checkAt(2700000,true,1);assert(lcdOnUntil==2750000);
 checkAt(2749999,true);checkAt(2750000,false);
 // Setup AP keeps backlight on; periodic timer does not replace its deadline.
 setupApActive=true;lcdOnUntil=0;checkAt(3600000,true);assert(lcdOnUntil==0);
 setupApActive=false;checkAt(3600001,false);
 // Elapsed and deadline arithmetic across millis wrap, including 2 full cycles.
 uint32_t base=0xffff0000u;lastLcdAutoWake=base;lcdOnUntil=base;lastLcdUpdate=base;
 uint32_t start=uint32_t(base+900000u);
 checkAt(start-1,false);checkAt(start,true,0);checkAt(start+4000,true,1);checkAt(start+8000,true,2);checkAt(start+12000,true,0);checkAt(start+16000,true,1);checkAt(start+20000,true,2);checkAt(start+24000,false);
 // No display: no I2C calls or periodic state changes.
 lcdPresent=false;int writes=lcd.writes;auto last=lastLcdAutoWake;
 checkAt(start+900000,false);assert(lcd.writes==writes&&lastLcdAutoWake==last);
 puts("PASS: actual LCD service, 15-minute cadence, 2 cycles of 3x4-second pages, manual/AP priority, millis rollover, absent display.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

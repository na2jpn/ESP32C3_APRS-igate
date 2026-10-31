#!/usr/bin/env python3
"""Compile actual LED function/constants with host-only GPIO and status stubs."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parent.parent
source=(root/'APRS_iGate_C3_Rev0_3.ino').read_text()
config=(root/'config.h').read_text()
constants='\n'.join(re.findall(r'^static constexpr uint(?:8|32)_t (?:PIN_LED_\w+|LED_\w+)\s*=.*?;',config,re.M))
a=source.index('static void serviceLeds()'); b=source.index('\nstatic String fit16',a)
code='''#include <cstdint>
#include <cassert>
#include <cstdio>
constexpr int HIGH=1,LOW=0,WL_CONNECTED=3;
uint32_t clockMs=0,aprsLedPulseUntil=0;
int pins[22]{};
uint32_t millis(){return clockMs;}
void digitalWrite(int p,int v){pins[p]=v;}
struct Wifi{int st=3;int status(){return st;}} WiFi;
struct Aprs{bool valid=true;bool verified(){return valid;}} aprs;
'''+constants+'\n'+source[a:b]+'''
int main(){
 for(uint32_t t=0;t<120000;++t){
  clockMs=t;serviceLeds();auto phase=t%30000;
  assert(pins[4]==(phase<10000));
  assert(pins[5]==(phase>=10000&&phase<20000));
 }
 // Each disconnected indicator remains independent of the other state.
 for(uint32_t t=0;t<30000;t+=100){
  clockMs=t;WiFi.st=0;aprs.valid=true;serviceLeds();
  assert(pins[4]==int((t/400)&1));
  assert(pins[5]==(t>=10000&&t<20000));
  WiFi.st=3;aprs.valid=false;serviceLeds();
  assert(pins[4]==(t<10000));assert(pins[5]==int((t/700)&1));
 }
 WiFi.st=3;aprs.valid=true;clockMs=15000;aprsLedPulseUntil=15100;
 serviceLeds();assert(pins[5]==LOW);
 clockMs=15100;serviceLeds();assert(pins[5]==HIGH);
 puts("PASS: actual LED function/config; 4 complete cycles, all boundaries, disconnected/unverified indicators, RX blanking.");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'led.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'led.cpp'),'-o',str(p/'led')],check=True)
 subprocess.run([str(p/'led')],check=True)

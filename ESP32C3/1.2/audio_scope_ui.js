'use strict';
const el=id=>document.getElementById(id);
let frozen=false,hidden=false,timer=null,controller=null,generation=0,failures=0,last=null;
const db=value=>value>0?20*Math.log10(value/2048):-Infinity;
const dbText=value=>Number.isFinite(value)?value.toFixed(1)+' dBFS':'−∞ dBFS';
const position=value=>Math.max(0,Math.min(100,(value+60)/60*100));
function draw(data){
 const c=el('wave'),ctx=c.getContext('2d'),w=c.width,h=c.height;
 const choice=el('gain').value;
 let gain=choice==='auto'?Math.min(32,Math.max(1,Math.pow(2,Math.floor(Math.log2(1700/Math.max(1,data.peak)))))):Number(choice);
 const center=choice==='auto'?data.mean:2048;
 ctx.fillStyle='#101d29';ctx.fillRect(0,0,w,h);
 const y=value=>h/2-(value-center)*gain/4096*(h-36);
 ctx.strokeStyle='#345064';ctx.lineWidth=1;
 for(let i=0;i<=4;i++){const yy=18+i*(h-36)/4;ctx.beginPath();ctx.moveTo(0,yy);ctx.lineTo(w,yy);ctx.stroke();ctx.fillStyle='#afc2d1';ctx.fillText(String(Math.round(center+(0.5-i/4)*4096/gain)),5,yy-3);}
 ctx.strokeStyle=data.clipped?'#ff7777':'#71e0ba';
 const n=data.low.length;
 for(let i=0;i<n;i++){const x=50+i*(w-60)/(n-1);ctx.beginPath();ctx.moveTo(x,y(data.high[i]));ctx.lineTo(x,Math.max(y(data.high[i])+1,y(data.low[i])));ctx.stroke();}
 ctx.fillStyle='#afc2d1';ctx.fillText('0 ms',50,h-3);ctx.fillText(data.duration_ms.toFixed(1)+' ms',w-65,h-3);
 el('scale').textContent='{{scopejs.0}}'+gain+(choice==='auto'?'{{scopejs.1}}':'{{scopejs.2}}')+'{{scopejs.3}}'+center.toFixed(1)+'{{scopejs.4}}'+data.duration_ms.toFixed(1)+' ms';
}
function render(data,now){
 const current=db(data.meter_rms),peak=db(data.meter_peak5s);
 el('level').style.width=position(current)+'%';el('peak').style.left=position(peak)+'%';
 el('meter').setAttribute('aria-valuenow',String(Math.max(-60,Number.isFinite(current)?current:-60)));
 el('numbers').textContent='{{scopejs.5}}'+dbText(current)+' ('+(data.meter_rms/2048*100).toFixed(2)+'{{scopejs.6}}'+dbText(peak)+'{{scopejs.7}}'+dbText(db(data.rms))+'{{scopejs.8}}'+dbText(db(data.peak))+'{{scopejs.9}}'+data.mean.toFixed(1)+'{{scopejs.10}}'+data.minimum+'/'+data.maximum+'{{scopejs.11}}'+data.clipped+' / '+data.samples+' samples';
 draw(data);
}
function blank(message){
 last=null;el('status').textContent=message;
 el('level').style.width='0%';el('peak').style.left='0%';el('meter').setAttribute('aria-valuenow','-60');
 el('numbers').textContent='{{scopejs.12}}';
 const c=el('wave'),ctx=c.getContext('2d');ctx.fillStyle='#101d29';ctx.fillRect(0,0,c.width,c.height);
}
function schedule(ms){clearTimeout(timer);if(!frozen&&!hidden)timer=setTimeout(poll,ms);}
async function poll(){
 if(frozen||hidden||controller)return;
 const gen=generation,started=Date.now();const request=new AbortController();controller=request;
 const timeout=setTimeout(()=>request.abort(),8000);
 try{
  const response=await fetch('/audio/data',{cache:'no-store',signal:request.signal});
  if(!response.ok)throw Error(response.status===409?'{{scopejs.13}}':'HTTP '+response.status);
  const data=await response.json();if(gen!==generation||frozen||hidden)return;
  failures=0;
  if(!data.valid){blank('{{scopejs.14}}'+data.state+'{{scopejs.15}}');}
  else{last=data;render(data,Date.now());el('status').textContent='{{scopejs.16}}'+data.state+'{{scopejs.17}}'+data.age_ms+'{{scopejs.18}}'+(Date.now()-started)+'{{scopejs.19}}'+data.sequence+(data.clipped?'{{scopejs.20}}':'');}
 }catch(e){if(gen===generation&&!frozen&&!hidden){++failures;blank('{{scopejs.21}}'+e.message+'{{scopejs.22}}');}}
 finally{clearTimeout(timeout);if(controller===request)controller=null;schedule(failures?Math.min(15000,1000*Math.pow(2,failures-1)):1000);}
}
function stopRequests(){++generation;clearTimeout(timer);if(controller)controller.abort();}
el('freeze').addEventListener('click',()=>{
 frozen=!frozen;stopRequests();el('freeze').textContent=frozen?'{{scopejs.23}}':'{{scopejs.24}}';
 if(frozen)el('status').textContent='{{scopejs.25}}';
 else{if(last)render(last,Date.now());schedule(0);}
});
el('gain').addEventListener('change',()=>{if(last)draw(last);});
document.addEventListener('visibilitychange',()=>{hidden=document.hidden;stopRequests();if(!hidden&&!frozen){blank('{{scopejs.26}}');schedule(0);}});
window.addEventListener('pagehide',()=>{hidden=true;stopRequests();});
hidden=document.hidden;blank('{{scopejs.27}}');schedule(0);

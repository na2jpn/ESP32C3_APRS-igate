const vm=require('vm'),fs=require('fs'),assert=require('assert');
const lang=process.env.UI_TEST_LANG||'ja';
const tr=(ja,en)=>lang==='en'?en:ja;
const code=require('./localize_ui.js')(fs.readFileSync(__dirname+'/../audio_scope_ui.js','utf8'),lang);
const source=fs.readFileSync(lang==='en'?'/tmp/igate-audio12-en.html':'/tmp/igate-audio011.html','utf8');assert(source.includes(code));
let count=0,now=0,next=0,timers=new Map(),docHandlers={},windowHandlers={},requests=[];
const context={fillRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){},fillText(){}};
const elements={};for(const id of ['wave','gain','status','level','peak','meter','numbers','scale','freeze'])elements[id]={textContent:'',style:{},value:'1',width:800,height:240,handlers:{},getContext(){return context;},setAttribute(k,v){this[k]=v;},addEventListener(k,v){this.handlers[k]=v;}};
const document={hidden:false,getElementById:id=>elements[id],addEventListener:(n,f)=>docHandlers[n]=f};
const sandbox={document,window:{addEventListener:(n,f)=>windowHandlers[n]=f},Date:{now:()=>now},Math,Number,String,Error,AbortController,
 setTimeout:(fn,ms)=>{timers.set(++next,{fn,ms});return next;},clearTimeout:id=>timers.delete(id),fetch:(url,opts)=>{count++;return new Promise((resolve,reject)=>requests.push({resolve,reject,opts,url}));}};
vm.createContext(sandbox);vm.runInContext(code,sandbox);
const flush=async()=>{for(let i=0;i<8;i++)await Promise.resolve();};
function fire(ms){const pair=[...timers].find(([,v])=>v.ms===ms);assert(pair,'Missing timer '+ms);timers.delete(pair[0]);pair[1].fn();}
const frame={valid:true,state:'OK',sequence:1,at_ms:100,age_ms:0,duration_ms:40,samples:1056,mean:2200,rms:100,meter_rms:100,meter_peak5s:1000,peak:300,minimum:1900,maximum:2500,clipped:0,low:Array(264).fill(1900),high:Array(264).fill(2500)};
async function respond(body,status=200){const r=requests.shift();assert(r);r.resolve({ok:status===200,status,json:async()=>body});await flush();return r;}
(async()=>{
 fire(0);assert.equal(count,1);vm.runInContext('poll()',sandbox);assert.equal(count,1); // no overlap
 await respond(frame);assert(elements.numbers.textContent.includes('4.88%'));assert(elements.numbers.textContent.includes('−')||elements.numbers.textContent.includes('-26.2'));
 assert.equal(elements.level.style.width,((20*Math.log10(100/2048)+60)/60*100)+'%');
 assert.equal(elements.peak.style.left,((20*Math.log10(1000/2048)+60)/60*100)+'%'); // server peak includes an unseen burst
 const held=elements.numbers.textContent;elements.freeze.handlers.click();assert.equal(elements.numbers.textContent,held);assert(elements.status.textContent.includes(tr('保持中','Paused and held')));
 assert(![...timers.values()].some(t=>t.ms===1000));elements.gain.value='auto';elements.gain.handlers.change();assert(elements.scale.textContent.includes(tr('自動・DC除去','auto, DC removed')));
 elements.freeze.handlers.click();fire(0);await respond({...frame,sequence:2,rms:200,meter_rms:200,meter_peak5s:1000,peak:600});assert(elements.numbers.textContent.includes('9.77%'));
 fire(1000);requests.shift().reject(Error('lost'));await flush();assert(elements.status.textContent.includes(tr('再試行','retrying')));assert.equal(elements.level.style.width,'0%');
 fire(1000);requests.shift().reject(Error('lost'));await flush();assert([...timers.values()].some(t=>t.ms===2000));
 fire(2000);await respond({},409);assert(elements.status.textContent.includes(tr('OTA中','OTA active')));assert([...timers.values()].some(t=>t.ms===4000));
 fire(4000);await respond({...frame,valid:false,state:'STALE'});assert(elements.status.textContent.includes('STALE'));assert(elements.numbers.textContent.includes(tr('音量ゼロを意味しません','does not mean zero audio')));
 fire(1000);elements.freeze.handlers.click();const staleRequest=requests[0];assert(staleRequest.opts.signal.aborted);await respond(frame);assert(elements.status.textContent.includes(tr('保持中','Paused and held'))); // stale response ignored
 elements.freeze.handlers.click();fire(0);await respond({...frame,clipped:5});assert(elements.status.textContent.includes(tr('ADC端付近','ADC near edge')));
 document.hidden=true;docHandlers.visibilitychange();assert(![...timers.values()].some(t=>t.ms===1000));
 document.hidden=false;docHandlers.visibilitychange();fire(0);await respond({...frame,sequence:5});assert(elements.status.textContent.includes(tr('取得中','Capturing')));
 windowHandlers.pagehide();assert(![...timers.values()].some(t=>t.ms===1000));
 console.log('PASS: exact audio UI meter/dBFS/scale, pause/freeze race, resume, no-overlap, retry/backoff, OTA, stale-data clearing, hidden/pagehide suspension');
})().catch(e=>{console.error(e);process.exit(1);});

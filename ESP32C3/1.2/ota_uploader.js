// Browser-side OTA v0.9. No third-party resources; SHA256 works on plain LAN HTTP.
let stop=false,running=false,session=0,oldVersion='',fileSha='',fileSize=0,clientKey='',retries=0,lastProgress=0;
const storageKey='igate-ota09:'+location.origin;
const msg=t=>document.getElementById('s').textContent=t;
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const RETRY_BUDGET=8*60*1000,CHUNK=1024;
class Fatal extends Error{} class Cancelled extends Error{}
function sha256(bytes){
 const K=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
 const H=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
 const data=new Uint8Array(Math.ceil((bytes.length+9)/64)*64);data.set(bytes);data[bytes.length]=128;
 const view=new DataView(data.buffer),bits=bytes.length*8;view.setUint32(data.length-8,Math.floor(bits/0x100000000));view.setUint32(data.length-4,bits>>>0);
 const R=(x,n)=>(x>>>n)|(x<<(32-n)),w=new Uint32Array(64);
 for(let off=0;off<data.length;off+=64){
  for(let i=0;i<16;i++)w[i]=view.getUint32(off+4*i);
  for(let i=16;i<64;i++){const a=w[i-15],b=w[i-2];w[i]=(w[i-16]+(R(a,7)^R(a,18)^(a>>>3))+w[i-7]+(R(b,17)^R(b,19)^(b>>>10)))>>>0;}
  let [a,b,c,d,e,f,g,h]=H;
  for(let i=0;i<64;i++){const t1=(h+(R(e,6)^R(e,11)^R(e,25))+((e&f)^(~e&g))+K[i]+w[i])>>>0,t2=((R(a,2)^R(a,13)^R(a,22))+((a&b)^(a&c)^(b&c)))>>>0;h=g;g=f;f=e;e=(d+t1)>>>0;d=c;c=b;b=a;a=(t1+t2)>>>0;}
  [a,b,c,d,e,f,g,h].forEach((v,i)=>H[i]=(H[i]+v)>>>0);
 }
 return H.map(x=>x.toString(16).padStart(8,'0')).join('');
}
function saved(){try{return JSON.parse(localStorage.getItem(storageKey)||'null');}catch(e){return null;}}
function persist(){try{localStorage.setItem(storageKey,JSON.stringify({boot:token,client:clientKey,sha:fileSha,size:fileSize}));}catch(e){/* Same-page recovery still works without localStorage. */}}
function forget(){try{localStorage.removeItem(storageKey);}catch(e){}}
async function request(path,body=null){
 const ctrl=new AbortController(),timer=setTimeout(()=>ctrl.abort(),30000);
 try{
  const r=await fetch(path,{method:body===null?'GET':'POST',body,headers:body===null?{}:{'Content-Type':'text/plain'},signal:ctrl.signal,cache:'no-store',credentials:'same-origin'});
  const text=await r.text();
  if(!r.ok){const e=new Error('HTTP '+r.status+': '+text);if(r.status===401||r.status===403||r.status===400||r.status===404||r.status===405)throw new Fatal(e.message);throw e;}
  return JSON.parse(text);
 }finally{clearTimeout(timer);}
}
function validate(d,allowNewBoot=false){
 if(d.protocol!==9)throw new Fatal('{{otajs.0}}');
 if(d.boot!==token){
  if(allowNewBoot&&d.previous_verified&&d.previous_sha256===fileSha&&d.previous_client===clientKey&&d.previous_size===fileSize)return 'rebooted';
  throw new Fatal('{{otajs.1}}');
 }
 if(d.client!==clientKey||d.sha256!==fileSha||d.expected!==fileSize||!Number.isInteger(d.id)||d.id<=0)throw new Fatal('{{otajs.2}}');
 if(session&&session!==d.id)throw new Fatal('{{otajs.3}}');
 if(!Number.isInteger(d.received)||d.received<0||d.received>fileSize)throw new Fatal('{{otajs.4}}');
 session=d.id;return 'same';
}
function progress(d){lastProgress=d.received;document.getElementById('p').value=d.received/fileSize*100;msg('{{otajs.5}}'+d.received+' / '+fileSize+'{{otajs.6}}'+Math.floor(d.received/fileSize*100)+'{{otajs.7}}'+retries+'{{otajs.8}}');}
async function backoff(start,attempt,label,allowCancel=true){
 if(allowCancel&&stop)throw new Cancelled();
 if(performance.now()-start>=RETRY_BUDGET)throw new Fatal('{{otajs.9}}');
 const sec=Math.min(15,2**Math.min(attempt,4));++retries;
 msg(label+'{{otajs.10}}'+sec+'{{otajs.11}}'+retries+'{{otajs.12}}'+lastProgress+' / '+fileSize+'{{otajs.13}}');
 await wait(sec*1000);
 if(allowCancel&&stop)throw new Cancelled();
}
const query=()=>request('/update/status');
const suffix=()=>'?token='+token+'&id='+session+'&client='+clientKey;
function cancel(){if(!running)return;stop=true;msg('{{otajs.14}}');}
async function begin(){
 const start=performance.now();let attempt=0,afterError=false;
 while(true){
  if(stop)throw new Cancelled();
  try{
   if(afterError){
    const d=await query();
    if(d.protocol!==9||d.boot!==token)throw new Fatal('{{otajs.15}}');
    if(d.busy||d.verified){validate(d);return d;}
    if(d.client===clientKey&&d.error)throw new Fatal('{{otajs.16}}'+d.error);
   }
   const d=await request('/update/begin?token='+token+'&size='+fileSize+'&client='+clientKey+'&sha256='+fileSha,'');validate(d);return d;
  }catch(e){if(e instanceof Fatal)throw e;afterError=true;await backoff(start,attempt++,'{{otajs.17}}');}
 }
}
async function sendChunk(off,bytes){
 let hex='';for(const x of bytes)hex+=x.toString(16).padStart(2,'0');
 const end=off+bytes.length,start=performance.now();let attempt=0,reconcile=false;
 while(true){
  if(stop)throw new Cancelled();
  try{
   if(reconcile){const d=await query();validate(d);if(!d.busy||d.verified)throw new Fatal('{{otajs.18}}'+(d.error||'{{otajs.19}}'));if(d.received===end)return d;if(d.received!==off)throw new Fatal('{{otajs.20}}');}
   const d=await request('/update/chunk'+suffix()+'&offset='+off,hex);validate(d);if(d.received!==end)throw new Fatal('{{otajs.21}}');return d;
  }catch(e){if(e instanceof Fatal)throw e;reconcile=true;await backoff(start,attempt++,'{{otajs.22}}');}
 }
}
async function finish(){
 const start=performance.now();let attempt=0,reconcile=false;
 while(true){
  try{
   if(reconcile){const d=await query();const state=validate(d,true);if(state==='rebooted')return d;if(d.verified)return d;if(!d.busy)throw new Fatal('{{otajs.23}}'+(d.error||'{{otajs.24}}'));if(d.received!==fileSize)throw new Fatal('{{otajs.25}}');}
   const d=await request('/update/finish'+suffix(),'');validate(d);if(!d.verified)throw new Fatal('{{otajs.26}}');return d;
  }catch(e){if(e instanceof Fatal)throw e;reconcile=true;await backoff(start,attempt++,'{{otajs.27}}',false);}
 }
}
async function abort(){
 const start=performance.now();let attempt=0;
 while(true){
  try{
   const d=await query();
   if(d.protocol===9&&d.boot===token&&!session&&!d.busy&&!d.verified&&d.client!==clientKey){forget();msg('{{otajs.28}}');return;}
   validate(d,true);
   if(d.verified||d.previous_verified&&d.previous_client===clientKey&&d.previous_sha256===fileSha)throw new Fatal('{{otajs.29}}');
   if(!d.busy){forget();msg('{{otajs.30}}');return;}
   const a=await request('/update/abort'+suffix(),'');validate(a);if(a.busy||a.verified)throw new Fatal('{{otajs.31}}');forget();msg('{{otajs.32}}');return;
  }catch(e){if(e instanceof Fatal)throw e;await backoff(start,attempt++,'{{otajs.33}}',false);}
 }
}
async function check(){
 const start=performance.now();let attempt=0;
 while(true){try{const d=await query();msg('{{otajs.34}}'+d.firmware+'{{otajs.35}}'+oldVersion+'{{otajs.36}}');return;}catch(e){if(e instanceof Fatal){msg('{{otajs.37}}'+e.message);return;}try{await backoff(start,attempt++,'{{otajs.38}}',false);}catch(end){msg('{{otajs.39}}');return;}}}
}
async function go(){
 if(running)return;const f=document.getElementById('f').files[0];if(!f||!f.name.toLowerCase().endsWith('.bin')){msg('{{otajs.40}}');return;}
 globalThis.igateOtaRunning=true;running=true;stop=false;session=0;retries=0;lastProgress=0;fileSize=f.size;
 document.getElementById('go').disabled=true;document.getElementById('f').disabled=true;document.getElementById('cancel').disabled=false;
 
 try{
  msg('{{otajs.41}}');fileSha=sha256(new Uint8Array(await f.arrayBuffer()));
  const prior=saved();clientKey=prior&&prior.boot===token&&prior.sha===fileSha&&prior.size===fileSize&&/^[a-f0-9]{32}$/.test(prior.client)?prior.client:Array.from(crypto.getRandomValues(new Uint8Array(16)),x=>x.toString(16).padStart(2,'0')).join('');persist();
  const b=await begin();oldVersion=b.firmware;
  if(b.verified){await finish();}else{
   progress(b);
   for(let off=b.received;off<fileSize;){if(stop)throw new Cancelled();const bytes=new Uint8Array(await f.slice(off,off+CHUNK).arrayBuffer());const d=await sendChunk(off,bytes);off=d.received;progress(d);await wait(20);}
   if(stop)throw new Cancelled();await finish();
  }
  forget();msg('{{otajs.42}}');setTimeout(check,30000);
 }catch(e){
  if(e instanceof Cancelled){
   // A begin ACK can be lost: query by saved client/file identity before aborting.
   if(clientKey){try{await abort();}catch(a){msg('{{otajs.43}}'+a.message+'{{otajs.44}}');}}
   else msg('{{otajs.28}}');
  }else msg('{{otajs.45}}'+e.message+'{{otajs.46}}');
 }finally{globalThis.igateOtaRunning=false;running=false;document.getElementById('go').disabled=false;document.getElementById('f').disabled=false;document.getElementById('cancel').disabled=true;}
}

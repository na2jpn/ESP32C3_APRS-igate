// Executes the exact script extracted from the real server-generated /update page.
const vm=require('vm'),fs=require('fs'),assert=require('assert'),cryptoNode=require('crypto');
const lang=process.env.UI_TEST_LANG||'ja';
const page=fs.readFileSync(lang==='en'?'/tmp/igate-ota12-en.html':'/tmp/igate-ota09.html','utf8');
const code=page.match(/<script>([\s\S]*?)<\/script>/)[1];
const source=require('./localize_ui.js')(fs.readFileSync(__dirname+'/../ota_uploader.js','utf8'),lang);assert(code.includes(source));
fs.writeFileSync('/tmp/igate-ota09.js',code);
async function run(mode){
 const bytes=Buffer.alloc(5001);for(let i=0;i<bytes.length;i++)bytes[i]=i%256;
 const sha=cryptoNode.createHash('sha256').update(bytes).digest('hex');
 const f={size:bytes.length,name:'firmware.bin',arrayBuffer:async()=>Uint8Array.from(bytes).buffer,slice:(a,b)=>({arrayBuffer:async()=>Uint8Array.from(bytes.subarray(a,b)).buffer})};
 const elements={f:{files:[f],disabled:false},s:{textContent:''},go:{disabled:false},cancel:{disabled:true},p:{value:0}};
 let ctx,t=0,key='',fileHash='',received=0,started=false,verified=false,active=false,boot='',failed=false,statusFails=0,networkFailures=0,writes=0,finishes=0,aborts=0,beginCalls=0,postBeginCalls=0;
 const receivedParts=[],paths=[],messages=[],store=new Map();let previous={};
 const state=()=>({protocol:9,boot,id:123,client:key,sha256:fileHash,expected:bytes.length,received,busy:active,verified,firmware:'0.9',...previous});
 const response=(d,status=200)=>({ok:status===200,status,text:async()=>JSON.stringify(d)});
 const sandbox={document:{getElementById:id=>elements[id]},AbortController,Uint8Array,Uint32Array,DataView,Array,Promise,Error,Math,JSON,console,location:{origin:'http://local'},performance:{now:()=>t},crypto:{getRandomValues:a=>cryptoNode.randomFillSync(a)},localStorage:{getItem:k=>store.get(k)||null,setItem:(k,v)=>store.set(k,v),removeItem:k=>store.delete(k)},
 setTimeout:(fn,ms)=>{if(ms!==30000)queueMicrotask(()=>{t+=ms;fn();});return 1;},clearTimeout:()=>{},
 fetch:async(path,opts)=>{
  paths.push(path);const q=new URL('http://local'+path).searchParams;
  if(path.startsWith('/update/status')){
   if(mode==='permanent-drop'&&started)throw Error('offline');
   if(mode==='status-drop'&&failed&&statusFails++<3)throw Error('status offline');
   if(mode==='cancel-retry'&&failed&&statusFails++<2)throw Error('cancel offline');
   let d=state();if(mode==='identity-mismatch'&&failed)d.sha256='b'.repeat(64);
   if(mode==='offset-mismatch'&&failed)d.received=7;
   return response(d);
  }
  if(path.startsWith('/update/begin')){
   ++beginCalls;
   if(mode==='auth-fail')return response('Unauthorized',401);
   if(started){if(q.get('client')!==key||q.get('sha256')!==fileHash)return response('Another session',409);return response(state());}
   started=true;active=true;key=q.get('client');fileHash=q.get('sha256');assert.equal(fileHash,sha);
   if(mode==='start-ack-loss'&&!failed){failed=true;throw Error('lost start ACK');}
   return response(state());
  }
  if(path.startsWith('/update/chunk')){
   ++postBeginCalls;
   assert.equal(q.get('id'),'123');assert.equal(q.get('client'),key);
   const off=Number(q.get('offset')),part=Buffer.from(opts.body,'hex');assert(part.length<=1024);
   if(['before-commit','identity-mismatch','offset-mismatch','status-drop','server-503','permanent-drop','device-reset'].includes(mode)&&!failed){failed=true;if(mode==='device-reset')boot='newboot';if(mode==='server-503')return response('temporary',503);throw Error('connection lost');}
   if(mode==='permanent-drop'){networkFailures++;throw Error('offline');}
   assert.equal(off,received,'No duplicate writes');receivedParts.push(part);received+=part.length;++writes;
   if(mode==='slow-progress')t+=65000;
   if(mode==='cancel'||mode==='cancel-retry'||mode==='cancel-ack-loss'){failed=true;ctx.cancel();}
   if(['after-commit','status-drop'].includes(mode)&&!failed){failed=true;throw Error('lost data ACK');}
   return response(state());
  }
  if(path.startsWith('/update/finish')){
   ++finishes;assert.equal(received,bytes.length);assert(Buffer.concat(receivedParts).equals(bytes));
   if(mode==='verify-fail')return response('bad image',400);
   verified=true;
   if(mode==='finish-ack-loss'&&!failed){failed=true;throw Error('lost verify ACK');}
   if(mode==='finish-reboot-loss'&&!failed){failed=true;previous={previous_verified:true,previous_sha256:fileHash,previous_client:key,previous_size:bytes.length};boot='newboot';verified=false;active=false;throw Error('reboot');}
   return response(state());
  }
  if(path.startsWith('/update/abort')){++aborts;active=false;if(mode==='cancel-ack-loss'&&aborts===1)throw Error('lost abort ACK');return response(state());}
  throw Error('Unknown path');
 }};
 ctx=vm.createContext(sandbox);vm.runInContext(code,ctx);boot=vm.runInContext('token',ctx);
 // SHA-256 padding, full block and large file vectors against Node's implementation.
 for(const len of [0,3,55,56,63,64,65,1000,1203792]){const b=Buffer.alloc(len,0xab);sandbox.testBytes=Uint8Array.from(b);assert.equal(vm.runInContext('sha256(testBytes)',ctx),cryptoNode.createHash('sha256').update(b).digest('hex'));}
 if(mode==='busy-other-file'){started=true;active=true;key='a'.repeat(32);fileHash='b'.repeat(64);}
 if(mode==='reload-resume'){
  started=true;active=true;key='a'.repeat(32);fileHash=sha;received=1024;receivedParts.push(bytes.subarray(0,1024));
  store.set('igate-ota09:http://local',JSON.stringify({boot,sha,size:bytes.length,client:key}));
 }
 await ctx.go();
 assert.equal(elements.go.disabled,false);assert.equal(elements.f.disabled,false);assert.equal(elements.cancel.disabled,true);
 const text=elements.s.textContent;
 const failures=['identity-mismatch','offset-mismatch','permanent-drop','device-reset','verify-fail','auth-fail','busy-other-file'];
 if(failures.includes(mode)){assert(text.includes(lang==='en'?'Update completion not confirmed':'完了を確認できません'),mode+': '+text);assert(!text.includes(lang==='en'?'image verified':'検証が完了'),text);if(mode!=='verify-fail')assert.equal(finishes,0);}
 else if(mode.startsWith('cancel')){assert.equal(aborts,1);assert.equal(finishes,0);assert(lang==='en'?text.includes('cancelled')||text.includes('ended'):text.includes('中止しました')||text.includes('終了を確認しました'),mode+': '+text);}
 else{assert(text.includes(lang==='en'?'image verified':'検証が完了'),mode+': '+text);assert.equal(elements.p.value,100);assert.equal(finishes,1);assert(Buffer.concat(receivedParts).equals(bytes));}
 if(mode==='permanent-drop'){assert(t>=480000&&t<520000);assert(paths.length<100);}
 if(mode==='slow-progress')assert(t>300000);
 if(mode==='after-commit')assert.equal(writes,5);
 if(mode==='start-ack-loss')assert.equal(beginCalls,1);
 if(mode==='reload-resume')assert.equal(writes,4);
}
(async()=>{for(const mode of ['normal','before-commit','after-commit','start-ack-loss','status-drop','server-503','finish-ack-loss','finish-reboot-loss','cancel','cancel-retry','cancel-ack-loss','busy-other-file','reload-resume','identity-mismatch','offset-mismatch','device-reset','verify-fail','auth-fail','permanent-drop','slow-progress'])await run(mode);console.log('PASS: actual OTA UI SHA256 vectors; pre/post commit ACK loss, start/finish/reboot loss, retry/status/503, cancel, reload resume, wrong identity/offset/reset/auth/verify, bounded retry and >5min successful progress.');})().catch(e=>{console.error(e);process.exit(1);});

// Test the exact JavaScript extracted from the generated /update HTML.
const vm=require('vm'),fs=require('fs'),assert=require('assert');
const code=fs.readFileSync(process.argv[2]||'/tmp/igate-ota07.js','utf8');
async function run(mode){
 const bytes=Buffer.alloc(5001);for(let i=0;i<bytes.length;i++)bytes[i]=i%256;
 const f={size:bytes.length,name:'firmware.bin',slice:(a,b)=>({arrayBuffer:async()=>Uint8Array.from(bytes.subarray(a,b)).buffer})};
 const elements={f:{files:[f]},s:{textContent:''},go:{disabled:false},cancel:{disabled:true},p:{value:0}};
 const received=[],paths=[];let context,offset=0;
 const sandbox={document:{getElementById:id=>elements[id]},AbortController,Uint8Array,Promise,Error,Math,JSON,console,
 setTimeout:(fn,ms)=>{if(ms===20)queueMicrotask(fn);return 1;},clearTimeout:()=>{},
 fetch:async(path,opts)=>{
  paths.push(path);if(path.startsWith('/update/status'))return {ok:true,json:async()=>({firmware:'0.7'})};
  let response={};
  if(path.startsWith('/update/begin'))response={id:123,firmware:'0.7'};
  if(path.startsWith('/update/chunk')){
   if(mode==='disconnect')throw Error('connection reset');
   const supplied=Number(new URL('http://local'+path).searchParams.get('offset'));assert.equal(supplied,offset);
   const data=Buffer.from(opts.body,'hex');assert(data.length<=2048);received.push(data);offset+=data.length;response={received:offset};
   if(mode==='cancel')context.cancel();
  }
  if(mode==='server-error'&&path.startsWith('/update/begin'))return {ok:false,status:409,text:async()=>'Session active'};
  return {ok:true,status:200,text:async()=>JSON.stringify(response)};
 }};
 context=vm.createContext(sandbox);vm.runInContext(code,context);await context.go();
 assert.equal(elements.go.disabled,false);assert.equal(elements.cancel.disabled,true);
 if(mode==='normal'){assert(Buffer.concat(received).equals(bytes));assert(paths.some(p=>p.startsWith('/update/finish')));assert(elements.s.textContent.includes('検証が完了'));assert.equal(elements.p.value,100);}
 if(mode==='disconnect'||mode==='server-error'){assert(!paths.some(p=>p.startsWith('/update/finish')));assert(elements.s.textContent.includes('完了を確認できません'));}
 if(mode==='cancel'){assert(paths.some(p=>p.startsWith('/update/abort')));assert(!paths.some(p=>p.startsWith('/update/finish')));}
}
(async()=>{for(const mode of ['normal','disconnect','server-error','cancel'])await run(mode);console.log('PASS: actual OTA page JS, binary chunking/ACK progress/HTTP errors/disconnect/cancel; no false success.');})().catch(e=>{console.error(e);process.exit(1);});

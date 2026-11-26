const {test}=require('node:test');const assert=require('node:assert/strict');
const flows=require('./flows.json'),{decode}=require('./decode');
const source=flows.find(n=>n.id==='decode').func;
function receiver(){const state=new Map();const run=new Function('msg','global','context','node',source);
return (boot,seq,event=false,prepare=()=>{})=>{const b=Buffer.alloc(80);b.write('RC');b[2]=1;b.writeUInt32LE(boot,4);b.writeUInt32LE(seq,8);
prepare(b);return run({payload:b,topic:'inf2004/se08/car01/v1/'+(event?'events':'telemetry')},{get:()=>({decode})},{get:k=>state.get(k),set:(k,v)=>state.set(k,v)},{warn:()=>{}});};}
test('receiver rejects duplicate and old frames; accepts reboot',()=>{const r=receiver();assert.ok(r(1,10)[0]);assert.equal(r(1,10)[0],null);assert.equal(r(1,9)[0],null);assert.ok(r(1,11)[0]);assert.ok(r(2,1)[0]);});
test('receiver supports wrap and excludes events from live view',()=>{const r=receiver();assert.ok(r(1,0xffffffff)[0]);assert.ok(r(1,0)[0]);assert.equal(r(1,100,true)[0],null);assert.ok(r(1,1)[0]);});

test('completed hump can be lower than the record and clears for a new run',()=>{
 const r=receiver();
 function terrain(count){return b=>{b[3]=8;b[55]=45;b[56]=32;b[57]=count;};}
 function completed(count,height){return b=>{
   terrain(count)(b);b[64]=17;b[65]=4;b.writeUInt32LE(count,72);b.writeInt32LE(height,76);
 };}
 r(1,2,true,completed(1,32));
 r(1,5,true,completed(2,18));
 assert.equal(r(1,6,false,terrain(2))[0].payload.latestHumpMm,18);
 r(1,2,true,completed(1,32));
 assert.equal(r(1,7,false,terrain(2))[0].payload.latestHumpMm,18);
 assert.equal(r(1,8,false,terrain(0))[0].payload.latestHumpMm,undefined);
 assert.equal(r(2,1,false,terrain(2))[0].payload.latestHumpMm,undefined);
});

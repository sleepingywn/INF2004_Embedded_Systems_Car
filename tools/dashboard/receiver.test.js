const {test}=require('node:test');const assert=require('node:assert/strict');
const flows=require('./flows.json'),{decode}=require('./decode');
const source=flows.find(n=>n.id==='decode').func;
function receiver(){let state;const run=new Function('msg','global','context','node',source);
return (boot,seq,event=false)=>{const b=Buffer.alloc(80);b.write('RC');b[2]=1;b.writeUInt32LE(boot,4);b.writeUInt32LE(seq,8);
return run({payload:b,topic:'inf2004/se08/car01/v1/'+(event?'events':'telemetry')},{get:()=>({decode})},{get:()=>state,set:(_,v)=>{state=v}},{warn:()=>{}});};}
test('receiver rejects duplicate and old frames; accepts reboot',()=>{const r=receiver();assert.ok(r(1,10)[0]);assert.equal(r(1,10)[0],null);assert.equal(r(1,9)[0],null);assert.ok(r(1,11)[0]);assert.ok(r(2,1)[0]);});
test('receiver supports wrap and excludes events from live view',()=>{const r=receiver();assert.ok(r(1,0xffffffff)[0]);assert.ok(r(1,0)[0]);assert.equal(r(1,100,true)[0],null);assert.ok(r(1,1)[0]);});

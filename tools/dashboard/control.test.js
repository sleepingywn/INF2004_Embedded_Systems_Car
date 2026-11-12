const {test}=require('node:test'),assert=require('node:assert/strict');
const {ControlHub,decodeStatus}=require('./control');
function frame({seq=1,boot=10,session=1,flags=27,ack=0,result=0,owner=0,epoch=3,time=5000}={}){
 const b=Buffer.alloc(36);b.write('RS');b[2]=1;b[3]=flags;
 for(const [o,v]of [[4,boot],[8,session],[12,seq],[16,time],[20,epoch],[24,ack],[32,owner]])b.writeUInt32LE(v,o);
 b[28]=result;return b;
}
const viewer='test-viewer-123456',other='other-viewer-12345';
test('control ABI, explicit Start, own-view keepalive and Stop',()=>{
 let now=1000;const h=new ControlHub(()=>now);h.accept(frame());
 const c=h.issue({action:'start',viewer});assert.equal(c.payload.length,24);assert.equal(c.payload[3],1);assert.equal(c.payload.readUInt32LE(20),3);assert.equal(c.retain,false);
 assert.throws(()=>h.issue({action:'start',viewer:other}));assert.throws(()=>h.issue({action:'keep',viewer:other}));
 now+=400;assert.equal(h.issue({action:'keep',viewer}).payload.readUInt32LE(16),5400);
 h.accept(frame({seq:2,ack:1,result:2,owner:1,flags:29}));
 const stop=h.issue({action:'stop',viewer:other});assert.equal(stop.payload[3],2);assert.throws(()=>h.issue({action:'keep',viewer}));
});
test('old firmware, stale/retained/out-of-order/reboot/reconnect cannot start or revive runs',()=>{
 let now=1000;const h=new ControlHub(()=>now);
 assert.throws(()=>h.issue({action:'start',viewer}));assert.equal(h.accept(frame(),true),null);
 h.accept(frame());assert.equal(h.accept(frame()),null);now+=751;assert.throws(()=>h.issue({action:'start',viewer}));
 h.accept(frame({seq:2}));h.issue({action:'start',viewer});
 h.accept(frame({seq:3,session:2}));assert.throws(()=>h.issue({action:'keep',viewer}));
 h.accept(frame({boot:11}));assert.equal(h.owner,null);
});
test('Start requires armed, fresh idle state; Stop remains allowed without motion snapshot',()=>{
 for(const flags of [0,17,19,25,29]){const h=new ControlHub();h.accept(frame({flags}));assert.throws(()=>h.issue({action:'start',viewer}));}
 const h=new ControlHub();h.accept(frame({flags:17}));assert.equal(h.issue({action:'stop',viewer}).payload[3],2);
 assert.throws(()=>decodeStatus(Buffer.alloc(36)));
 const bad=frame();bad[3]=255;assert.throws(()=>decodeStatus(bad));
});
test('terminal acknowledgement releases ownership and unsigned uptime wraps',()=>{
 let now=1000;const h=new ControlHub(()=>now);h.accept(frame({time:0xfffffff0}));now+=32;
 assert.equal(h.issue({action:'start',viewer}).payload.readUInt32LE(16),16);
 h.accept(frame({seq:2,ack:1,result:3}));assert.equal(h.owner,null);
});

test('dashboard continues heartbeat after 30 seconds while robot status remains fresh',()=>{
 let now=1000;const h=new ControlHub(()=>now);h.accept(frame());h.issue({action:'start',viewer});
 now+=61000;h.accept(frame({seq:2,ack:1,result:2,owner:1,flags:29,time:66000}));
 assert.equal(h.issue({action:'keep',viewer}).payload[3],3);
 now+=751;assert.throws(()=>h.issue({action:'keep',viewer}));
});

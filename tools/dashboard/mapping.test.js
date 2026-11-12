const {test}=require('node:test'),assert=require('node:assert/strict');
const {ControlHub,decodeStatus}=require('./control');
const viewer='settings-viewer-123456',mapping=Array.from({length:26},(_,i)=>i%5);
function status(){const b=Buffer.alloc(68);b.write('RS');b[2]=2;b[3]=27;b.writeUInt32LE(10,4);b.writeUInt32LE(1,8);b.writeUInt32LE(1,12);b.writeUInt32LE(5000,16);b.writeUInt32LE(3,20);b.writeUInt32LE(7,36);Buffer.from(mapping).copy(b,40);b[66]=1;return b;}
test('mapping status and save wire carry all 26 actions and revision',()=>{
 const h=new ControlHub(()=>1000),s=h.accept(status());assert.deepEqual(s.mappings,mapping);assert.equal(s.mappingRevision,7);assert.equal(s.mappingPersisted,true);
 const c=h.issue({action:'mapping',viewer,mappings:mapping,revision:7});assert.equal(c.payload.length,54);assert.equal(c.payload[2],2);assert.equal(c.payload[3],4);assert.equal(c.payload.readUInt32LE(24),7);assert.deepEqual([...c.payload.subarray(28)],mapping);assert.equal(c.retain,false);assert.equal(h.owner,null);
});
test('mapping save rejects running, stale revision, invalid actions, unsupported firmware and stale link',()=>{
 for(const change of [b=>b[3]|=4,b=>b[29]=1,b=>b[30]=1,b=>b[67]=1,b=>b[3]&=~8]){
  const h=new ControlHub(),b=status();change(b);h.accept(b);assert.throws(()=>h.issue({action:'mapping',viewer,mappings:mapping,revision:7}));
 }
 let now=1;const h=new ControlHub(()=>now);h.accept(status());
 for(const m of [[],mapping.slice(1),mapping.map(()=>5),mapping.map(()=>1.5),null])assert.throws(()=>h.issue({action:'mapping',viewer,mappings:m,revision:7}));
 assert.throws(()=>h.issue({action:'mapping',viewer,mappings:mapping,revision:6}));
 now+=751;assert.throws(()=>h.issue({action:'mapping',viewer,mappings:mapping,revision:7}));
 const bad=status();bad[40]=5;assert.throws(()=>decodeStatus(bad));
 const old=status().subarray(0,36);old[2]=1;h.accept(old);assert.throws(()=>h.issue({action:'mapping',viewer,mappings:mapping,revision:7}));
});

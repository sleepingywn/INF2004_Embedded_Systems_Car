const {test}=require('node:test'),assert=require('node:assert/strict');
const {decode}=require('./decode'),{barcodeReading}=require('./barcode'),{PoseTracker}=require('./pose');
function frame(code=65,flags=7){const b=Buffer.alloc(96);b.write('RC');b[2]=2;b[80]=code;b[81]=29;b[82]=flags;b.writeUInt32LE(4000,12);b.writeUInt32LE(3000,88);b.writeUInt32LE(42,84);return b;}
test('v2 barcode distinguishes unsupported, not scanned, failed and decoded',()=>{
 assert.equal(decode(frame()).barcode.code,'A');assert.equal(decode(frame(90)).barcode.code,'Z');
 for(const ch of '0123456789-. '){assert.equal(decode(frame(ch.charCodeAt(0))).barcode.code,ch);}
 assert.equal(barcodeReading({...decode(frame(32)),receivedAt:1000},1100).value,'(space)');
 assert.equal(decode(frame(0)).barcode.code,null);
 const packet={...decode(frame()),receivedAt:1000};assert.deepEqual(barcodeReading(packet,1100),{value:'A',detail:'Read 1.1 s ago · 29 / 29 elements (last capture)'});
 assert.equal(barcodeReading({...decode(frame(0,3)),receivedAt:1000},1100).value,'No scan yet');
 assert.equal(barcodeReading({...decode(frame(0)),receivedAt:1000},1100).value,'Not decoded');
 assert.equal(barcodeReading({version:1},1100).value,'Unavailable');
 assert.equal(barcodeReading({...decode(frame(0,0)),receivedAt:1000},1100).value,'Unavailable');
});
test('reject malformed v2 extensions and mismatched frame lengths',()=>{
 for(const [offset,value] of [[2,1],[80,64],[81,64],[82,8],[82,4],[83,1]]){const b=frame();b[offset]=value;assert.throws(()=>decode(b));}
 assert.throws(()=>decode(frame().subarray(0,80)));const b=frame(0);b[81]=63;assert.equal(decode(b).barcode.elements,63);
});
test('historical scan survives stale link and uptime wrap, reboot clears it',()=>{
 const p={...decode(frame(90)),receivedAt:1000};assert.match(barcodeReading(p,4000).detail,/telemetry stale/);
 p.publishedMs=16;p.barcode.sampledMs=0xfffffff0;assert.match(barcodeReading(p,1000).detail,/0.0 s/);
 const t=new PoseTracker();assert.equal(t.accept({...p,bootId:1,sequence:1},1000),true);
 assert.equal(t.accept({...decode(frame(0,3)),receivedAt:1100,bootId:2,sequence:1},1100),true);
 assert.equal(barcodeReading(t.packet,1100).value,'No scan yet');
});

test('last scan shows captured count against 29, including incomplete and overlength',()=>{
 for(const count of [5,13,28,29,30,63]) {
  const b=frame(0);b[81]=count;
  assert.match(barcodeReading({...decode(b),receivedAt:1000},1100).detail,new RegExp(count+' / 29 elements'));
 }
 assert.match(barcodeReading({...decode(frame(0,3)),receivedAt:1000},1100).detail,/0 \/ 29/);
});

test('last decoded letter follows the confirmed instruction for this boot',()=>{
 const {barcodeInstruction}=require('./barcode');
 const packet=decode(frame(68));
 const status={bootId:packet.bootId,receivedAt:1000,flags:17,mappings:Array(26).fill(0)};
 status.mappings[3]=4;
 assert.equal(barcodeInstruction(packet,status,1000),'U-turn');
 status.mappings[3]=2;
 assert.equal(barcodeInstruction(packet,status,1100),'Turn left');
 status.mappings[3]=0;
 assert.equal(barcodeInstruction(packet,status,1100),'Ignore');
 assert.equal(barcodeInstruction(packet,status,3000),'Mapping unavailable');
 assert.equal(barcodeInstruction(packet,{...status,bootId:42},1100),'Mapping unavailable');
 assert.equal(barcodeInstruction(decode(frame(0)),status,1100),'—');
});

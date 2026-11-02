"use strict";
const assert = require("node:assert/strict");
const {test} = require("node:test");
const {decode} = require("./decode");
function frame() {
 const b = Buffer.alloc(80); b[0]=82; b[1]=67; b[2]=1; return b;
}
test("reject malformed frames and unsupported schemas", () => {
 for (const value of [null, "text", Buffer.alloc(79), Buffer.alloc(81), Buffer.alloc(80)])
   assert.throws(()=>decode(value));
 for (const offset of [2,59,66,67]) { const b=frame(); b[offset]=255; assert.throws(()=>decode(b)); }
 const b=frame();b[3]=64;assert.throws(()=>decode(b));
});
test("unavailable fields stay null rather than false zero", () => {
 const d=decode(frame());
 for(const key of ["leftCount","rightCount","leftSpeed","rightSpeed","lineError","pitch","roll","faults","state"])
   assert.equal(d[key],null,key);
});
test("signed values, counters and explicit little endian layout", () => {
 const b=frame();b[3]=63;b.writeUInt32LE(0xfedcba98,4);b.writeUInt32LE(0xffffffff,28);
 b.writeInt16LE(-2047,44);b.writeInt16LE(2047,46);b.writeInt16LE(-100,48);
 b.writeInt8(-128,50);b.writeInt8(127,51);b[64]=19;b[65]=4;
 b.writeUInt32LE(0xfffffffe,72);b.writeInt32LE(-2147483648,76);
 const d=decode(b);assert.equal(d.bootId,0xfedcba98);assert.equal(d.leftCount,0xffffffff);
 assert.equal(d.leftSpeed,-2047);assert.equal(d.rightSpeed,2047);assert.equal(d.lineError,-100);
 assert.equal(d.pitch,-64);assert.equal(d.roll,63.5);assert.equal(d.eventCount,0xfffffffe);
 assert.equal(d.eventValue,-2147483648);assert.equal(d.ready,true);
});
test("raw encoder counts do not imply calibrated wheel speed", () => {
 const b=frame();b[3]=1;b.writeUInt32LE(42,28);b.writeInt16LE(999,44);
 const d=decode(b);assert.equal(d.leftCount,42);assert.equal(d.leftSpeed,null);
});

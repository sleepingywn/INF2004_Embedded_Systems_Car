const {test} = require('node:test');
const assert = require('node:assert/strict');
const {PoseTracker, CONFIG} = require('./pose');
function sample(n, values = {}) {
    return {version:1,bootId:1,sequence:n,publishedMs:n*250,motionMs:n*250,
        motionSequence:n,motionEpoch:1,leftCount:(-n*100)>>>0,rightCount:n*100,
        pitch:0,roll:0,valid:9,receivedAt:10000+n*250,...values};
}
const add = (t, d) => t.accept(d,d.receivedAt);
test('forward signed counts animate both wheels, without inventing metric speed',()=>{
    const t=new PoseTracker();add(t,sample(1));add(t,sample(2));
    assert.deepEqual(t.speed,{left:400,right:400});assert.equal(t.pose.yaw,0);
    assert.ok(t.pose.left<0);assert.equal(t.pose.left,t.pose.right);
});
test('right and left turns have opposite heading, reverse spins backward',()=>{
    const t=new PoseTracker();add(t,sample(1));
    add(t,sample(2,{leftCount:(-300)>>>0,rightCount:120}));assert.ok(t.pose.yaw<0);
    const l=new PoseTracker();add(l,sample(1));add(l,sample(2,{leftCount:(-120)>>>0,rightCount:300}));assert.ok(l.pose.yaw>0);
    const r=new PoseTracker();add(r,sample(1));add(r,sample(2,{leftCount:0,rightCount:0}));assert.ok(r.pose.left>0);assert.ok(r.pose.right>0);
});
test('counter, source timestamp and sequence rollover remain continuous',()=>{
    const t=new PoseTracker();add(t,sample(1,{sequence:0xffffffff,motionSequence:0xffffffff,motionMs:0xffffff80,publishedMs:0xffffff80,leftCount:10,rightCount:0xfffffff0}));
    add(t,sample(2,{sequence:0,motionSequence:0,motionMs:122,publishedMs:122,leftCount:0xfffffff6,rightCount:4}));
    assert.deepEqual(t.speed,{left:80,right:80});assert.equal(t.pose.yaw,0);
});
test('reboot and motion epoch reset baseline and do not animate a counter jump',()=>{
    for(const changes of [{bootId:2},{motionEpoch:2}]){const t=new PoseTracker();add(t,sample(1));add(t,sample(2));add(t,sample(3,{...changes,leftCount:0,rightCount:0}));assert.equal(t.pose.left,0);assert.equal(t.speed.left,null);}
});
test('duplicates, reordered frames and events do not refresh live telemetry',()=>{
    const t=new PoseTracker();const d=sample(5);add(t,d);
    assert.equal(add(t,sample(5)),false);assert.equal(add(t,sample(4)),false);
    assert.equal(add(t,sample(6,{topic:'car/events'})),false);assert.equal(t.packet.sequence,5);
});
test('invalid readings, long gaps and implausible jumps rebaseline without rotation',()=>{
    for(const change of [{valid:8},{motionMs:3000,publishedMs:3000},{leftCount:2000000000},{publishedMs:1000}]){
        const t=new PoseTracker();add(t,sample(1));add(t,sample(2,change));assert.equal(t.pose.left,0);assert.equal(t.speed.left,null);
    }
    const t=new PoseTracker();add(t,sample(1));add(t,sample(2,{receivedAt:15000}));assert.equal(t.pose.left,0);assert.equal(t.continuous,false);
});
test('stale frames freeze pose, hide speed, and never become fake zero motion',()=>{
    const t=new PoseTracker();add(t,sample(1));add(t,sample(2));const p={...t.pose};
    const s=t.snapshot(t.packet.receivedAt+CONFIG.staleMs+1);
    assert.equal(s.fresh,false);assert.equal(s.speed.left,null);assert.equal(s.motionValid,false);assert.deepEqual(s.pose,p);
    const old=new PoseTracker();old.accept(sample(1),20000);assert.equal(old.attitudeValid,false);assert.equal(old.motionValid,false);
});
test('IMU validity and car-axis signs govern body attitude independently',()=>{
    const t=new PoseTracker();add(t,sample(1,{valid:8,pitch:12,roll:3}));assert.ok(t.pose.pitch>0);assert.ok(t.pose.roll<0);
    const p={...t.pose};add(t,sample(2,{valid:1,pitch:60,roll:60}));assert.equal(t.pose.pitch,p.pitch);assert.equal(t.attitudeValid,false);
});
test('repeated source samples are not integrated twice',()=>{
    const t=new PoseTracker();add(t,sample(1));add(t,sample(2));const p={...t.pose};
    add(t,sample(3,{motionMs:500,publishedMs:550,motionSequence:2}));assert.deepEqual(t.pose,p);assert.equal(t.speed.left,null);
});
test('visual wheel geometry and signs match the firmware profile',()=>{
    const fs=require('node:fs'),path=require('node:path');
    const config=fs.readFileSync(path.join(__dirname,'../../app_program/subsystem2/motion_config.h'),'utf8');
    const define=name=>Number(config.match(new RegExp('#define '+name+'\\s+(-?[0-9]+)'))[1]);
    assert.equal(CONFIG.countsRev,define('MOTION_LEFT_COUNTS_REV'));
    assert.equal(CONFIG.countsRev,define('MOTION_RIGHT_COUNTS_REV'));
    assert.equal(CONFIG.circumferenceMm,define('MOTION_LEFT_CIRCUMFERENCE_UM')/1000);
    assert.equal(CONFIG.circumferenceMm,define('MOTION_RIGHT_CIRCUMFERENCE_UM')/1000);
    assert.equal(CONFIG.trackMm,define('MOTION_TRACK_WIDTH_MM'));
    assert.equal(define('MOTION_LEFT_ENCODER_SIGN'),-1);assert.equal(define('MOTION_RIGHT_ENCODER_SIGN'),1);
});
test('a measured stop shows zero speed and holds the last observed pose',()=>{
    const t=new PoseTracker();add(t,sample(1));add(t,sample(2));const p={...t.pose};
    add(t,sample(3,{leftCount:(-200)>>>0,rightCount:200,state:0}));
    assert.deepEqual(t.speed,{left:0,right:0});assert.deepEqual(t.pose,p);
});

const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const template=fs.readFileSync(path.join(__dirname,'overview-template.vue'),'utf8');
const source=template.match(/<script>([\s\S]*?)<\/script>/)[1].replace('export default','return');
test('Vue reactive payload crosses the frame bridge as cloneable plain data',()=>{
 const component=new Function('window',source)({location:{origin:'http://127.0.0.1:1880'}});
 const payload=new Proxy({version:1,bootId:42,leftCount:0xffffffff,pitch:12,leftSpeed:null},{});
 let sent;
 component.methods.deliver.call({msg:{payload},$refs:{overview:{contentWindow:{postMessage:(message,origin)=>{sent=structuredClone(message);assert.equal(origin,'http://127.0.0.1:1880');}}}}});
 assert.deepEqual(sent,{type:'robocar-telemetry',payload:{version:1,bootId:42,leftCount:0xffffffff,pitch:12,leftSpeed:null}});
});
test('checked-in Node-RED template matches its editable source',()=>{
 assert.equal(require('./flows.json').find(n=>n.id==='view').format,template);
});

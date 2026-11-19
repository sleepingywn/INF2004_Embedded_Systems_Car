"use strict";

const BASE='inf2004/se08/car01/v1/control';

const results=['Ready','Command queued','Running','Start rejected','Stopped','Run ended','Connection lost — stopped','Acknowledgement timed out'];

function decodeStatus(b){

 if(!Buffer.isBuffer(b)||!((b.length===36&&b[2]===1)||(b.length===68&&b[2]===2))||b.toString('ascii',0,2)!=='RS'||(b[3]&~63)||b[28]>7||b[31]>4)throw Error('Invalid control status');

 const u=n=>b.readUInt32LE(n);
 const mappings=b[2]===2?Array.from(b.subarray(40,66)):null;
 if(mappings && (!validMappings(mappings)||b[66]>1||b[67]>1))throw Error('Invalid mapping status');

 return {mappings,mappingRevision:mappings?u(36):null,mappingPersisted:!!b[66],mappingPending:!!b[67],kind:'controlStatus',flags:b[3],bootId:u(4),session:u(8),sequence:u(12),publishedMs:u(16),epoch:u(20),ackId:u(24),result:b[28],state:b[29],faults:b[30],op:b[31],ownerId:u(32),message:results[b[28]]};

}

function validMappings(m){return Array.isArray(m)&&m.length===26&&m.every(x=>Number.isInteger(x)&&x>=0&&x<=4);}
function encodeCommand(op,s,id,now,mappings){

 const b=Buffer.alloc(op===4?54:24);b.write('RC');b[2]=op===4?2:1;b[3]=op;

 for(const [offset,value] of [[4,s.bootId],[8,s.session],[12,id],[16,(s.publishedMs+Math.max(0,now-s.receivedAt))>>>0],[20,s.epoch]])b.writeUInt32LE(value>>>0,offset);

 if(op===4){b.writeUInt32LE(s.mappingRevision>>>0,24);Buffer.from(mappings).copy(b,28);}
 return b;

}

class ControlHub {

 constructor(clock=Date.now){this.clock=clock;this.status=null;this.owner=null;this.nextId=0;}

 accept(payload,retained=false){

  if(retained)return null;

  const s=decodeStatus(payload),old=this.status;

  if(old&&s.bootId===old.bootId&&s.session===old.session){const step=(s.sequence-old.sequence)>>>0;if(!step||step>=0x80000000)return null;}

  else {this.owner=null;this.nextId=0;}

  s.receivedAt=this.clock();this.status=s;

  if(s.ackId>this.nextId)this.nextId=s.ackId;

  if(this.owner && ((!(s.flags&4) && s.ackId>=this.owner.id && s.result>=3) || (s.ownerId && s.ownerId!==this.owner.id)))this.owner=null;

  return s;

 }

 issue(input){

  const now=this.clock(),s=this.status;

  if(!input||!['start','stop','keep','mapping'].includes(input.action)||typeof input.viewer!=='string'||!/^[a-zA-Z0-9-]{16,64}$/.test(input.viewer))throw Error('Invalid control request');

  if(!s || now-s.receivedAt>750 || (s.flags&17)!==17)throw Error('No fresh control connection to the robot');

  if(input.action==='keep'){

   if(!this.owner||this.owner.viewer!==input.viewer)throw Error('No run owned by this dashboard');

   return {payload:encodeCommand(3,s,this.owner.id,now),topic:BASE+'/command',qos:'1',retain:false};

  }

  if(input.action==='start' && ((s.flags&10)!==10 || (s.flags&4) || this.owner || s.state!==0 || s.faults))throw Error('Robot is not armed and idle');

  if(input.action==='mapping' && (!s.mappings || !validMappings(input.mappings) || input.revision!==s.mappingRevision || !(s.flags&8) || s.faults ||
     !((s.flags&32 && [4,5].includes(s.state)) || (!s.mappingPending && !(s.flags&4) && !this.owner && s.state===0))))throw Error('Cannot apply instructions in the current state or revision');

  const id=(this.nextId+1)>>>0;if(!id)throw Error('Command counter exhausted; reconnect the robot');this.nextId=id;

  const op=input.action==='start'?1:input.action==='mapping'?4:2;

  if(op===1)this.owner={viewer:input.viewer,id,created:now};else if(op===2)this.owner=null;

  return {payload:encodeCommand(op,s,id,now,input.mappings),topic:BASE+'/command',qos:'1',retain:false,

   feedback:{kind:'controlFeedback',viewer:input.viewer,action:input.action,id,message:input.action==='mapping'?'Instructions sent — waiting for robot confirmation':input.action==='start'?'Start sent — waiting for robot':'Stop sent — waiting for robot'}};

 }

}

const hub=new ControlHub();module.exports={ControlHub,decodeStatus,encodeCommand,validMappings,hub};


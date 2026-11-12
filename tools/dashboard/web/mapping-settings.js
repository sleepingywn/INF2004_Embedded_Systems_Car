// Drafts never change the car. Only Save sends a command; readback confirms it.
export function installMappingSettings() {
 const el=id=>document.getElementById(id),viewer=crypto.randomUUID();
 const defaults=Array.from({length:26},(_,i)=>i<4?i+1:i===25?2:0);
 const actions=['Ignore','Turn right','Turn left','Straight · 100 mm','U-turn'];
 let status=null,draft=null,baseRevision=null,dirty=false,pending=null,notice='';
 const equal=(a,b)=>a?.length===26&&b?.length===26&&a.every((v,i)=>v===b[i]);
 const selects=defaults.map((_,i)=>{
  const label=document.createElement('label'),select=document.createElement('select');
  label.append(document.createTextNode(String.fromCharCode(65+i)));
  select.setAttribute('aria-label',`Instruction for ${String.fromCharCode(65+i)}`);
  actions.forEach((name,value)=>{const option=document.createElement('option');option.value=value;option.textContent=name;select.append(option);});
  select.addEventListener('change',()=>{draft[i]=Number(select.value);dirty=true;notice='Unsaved changes';draw();});
  label.append(select);el('mapping-grid').append(label);return select;
 });
 function draw(){
  const fresh=status&&Date.now()-status.receivedAt<=1500&&(status.flags&17)===17;
  const available=fresh&&Array.isArray(status.mappings);
  selects.forEach((select,i)=>{select.disabled=!available||!!pending;if(draft)select.value=draft[i];});
  el('mapping-defaults').disabled=!available||!!pending;
  el('mapping-reload').disabled=!available||!!pending;
  el('mapping-save').disabled=!(available&&draft&&!pending&&!status.mappingPending&&
    !(status.flags&4)&&(status.flags&8)&&status.state===0&&!status.faults&&baseRevision===status.mappingRevision&&
    (!equal(draft,status.mappings)||!status.mappingPersisted));
  el('mapping-status').textContent=pending?'Saving — waiting for the robot to restart and confirm its stored instructions…':
    notice || (!available?'Settings unavailable — connect a robot with mapping firmware.':
    dirty?'Unsaved changes':status.mappingPersisted?`Stored on robot · revision ${status.mappingRevision}`:'Factory instructions · not yet saved on robot');
 }
 function reload(){draft=[...status.mappings];baseRevision=status.mappingRevision;dirty=false;notice='';draw();}
 window.addEventListener('message',event=>{
  if(event.origin!==location.origin||event.source!==window.parent)return;
  const p=event.data?.payload;
  if(event.data?.type==='robocar-control'&&p?.kind==='controlStatus'){
   status=p;
   if(pending&&p.mappings){
    if(p.mappingPersisted&&equal(p.mappings,pending.map)&&
       (p.bootId!==pending.boot||p.mappingRevision!==pending.revision)){
     pending=null;reload();notice='Saved on robot — these instructions survive power-off.';
    } else if((pending.id&&p.ackId===pending.id&&p.result===3)||
       (p.bootId!==pending.boot&&!p.mappingPending)){
     pending=null;notice='Save was not confirmed. The robot is using the reported settings; discard edits to reload them.';
    }
   }
   if(p.mappings&&!pending&&!dirty&&(!draft||baseRevision!==p.mappingRevision||!equal(draft,p.mappings)))reload();
   else if(dirty&&!pending&&p.mappings&&baseRevision!==p.mappingRevision)
    notice='Settings changed on the robot. Discard edits to load the latest instructions before saving.';
   draw();
  } else if(event.data?.type==='robocar-control-feedback'&&p?.viewer===viewer&&p.action==='mapping'){
   if(p.error){pending=null;notice=p.message;}
   else if(pending)pending.id=p.id;
   draw();
  }
 });
 el('mapping-defaults').addEventListener('click',()=>{draft=[...defaults];dirty=true;notice='Default instructions selected — press Save on robot to apply.';draw();});
 el('mapping-reload').addEventListener('click',reload);
 el('mapping-save').addEventListener('click',()=>{
  pending={map:[...draft],boot:status.bootId,revision:baseRevision,at:Date.now()};notice='';
  window.parent.postMessage({type:'robocar-command',payload:{action:'mapping',viewer,mappings:pending.map,revision:baseRevision}},location.origin);draw();
 });
 setInterval(()=>{
  if(pending&&Date.now()-pending.at>45000){pending=null;notice='Save not confirmed — reconnect and check the robot’s stored instructions before retrying.';}
  draw();
 },400);
 draw();
}

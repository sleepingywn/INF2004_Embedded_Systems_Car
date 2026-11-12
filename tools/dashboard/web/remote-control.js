// No command is sent on load. Only the initiating view renews a remote run.

export function installRemoteControl({el,text}) {

 const viewer=crypto.randomUUID();let status=null,owner=0,pending=null,notice=false,message='Remote control unavailable — waiting for compatible firmware';

 const send=action=>window.parent.postMessage({type:'robocar-command',payload:{action,viewer}},location.origin);

 function draw(){

  const fresh=status && Date.now()-status.receivedAt<=1500 && (status.flags&17)===17;

  el('remote-start').disabled=!(fresh && (status.flags&10)===10 && !(status.flags&4) && !pending && !owner && status.state===0 && !status.faults);

  el('remote-stop').disabled=!fresh;

  text('control-status',!fresh?'Remote control unavailable — use GP20':(pending||notice)?message:

    (status.flags&4)?'Remote run active · keep this dashboard open':

    status.result===0?((status.flags&2)?'Armed · Start runs line following':'Waiting for the car to arm'):

    status.message+((status.flags&2)?'':' · use GP20 retry when ready, or USB r to rearm'));

 }

 window.addEventListener('message',event=>{

  if(event.origin!==location.origin||event.source!==window.parent)return;

  const p=event.data?.payload;

  if(event.data?.type==='robocar-control' && p?.kind==='controlStatus'){

   if(status && (status.bootId!==p.bootId || status.session!==p.session)){owner=0;pending=null;}

   status=p;

   if(pending && p.ackId===pending.id && p.result!==1){pending=null;notice=false;}

   if(owner && !(p.flags&4) && p.ackId>=owner && p.result>=3)owner=0;

   draw();

  } else if(event.data?.type==='robocar-control-feedback' && p?.viewer===viewer){

   message=p.message;notice=!!p.error;

   if(p.error){pending={error:true,at:Date.now()};if(p.action==='start')owner=0;}

   else {pending={id:p.id,at:Date.now()};if(p.action==='start')owner=p.id;else owner=0;}

   draw();

  }

 });

 el('remote-start').addEventListener('click',()=>{notice=false;pending={at:Date.now()};message='Sending Start…';send('start');draw();});

 el('remote-stop').addEventListener('click',()=>{owner=0;notice=false;pending={at:Date.now()};message='Sending Stop…';send('stop');draw();});

 setInterval(()=>{

  if(owner && status && Date.now()-status.receivedAt<=1500 && !document.hidden)send('keep');

  if(pending && Date.now()-pending.at>2500){owner=0;pending=null;notice=true;message='No acknowledgement — use GP20';}

  draw();

 },400);

 document.addEventListener('visibilitychange',()=>{if(document.hidden && owner){owner=0;send('stop');}});

 window.addEventListener('pagehide',()=>{if(owner)send('stop');owner=0;});

 draw();

}


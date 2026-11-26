"use strict";
// Last observed scan is historical, not a promise that a manoeuvre is armed.
function barcodeReading(packet,now=Date.now()) {
 const b=packet?.barcode;
 if(!b?.supported)return {value:'Unavailable',detail:'Barcode telemetry requires updated route firmware.'};
 if(!b.available)return {value:'Unavailable',detail:'Waiting for a barcode snapshot.'};
 const stale=now-packet.receivedAt>2000;
 if(!b.seen)return {value:'No scan yet',detail:stale?'Telemetry stale. No scan received this boot.':'0 / 29 elements · waiting for a barcode scan.'};
 const age=((packet.publishedMs-b.sampledMs)>>>0)+Math.max(0,now-packet.receivedAt);
 const ago=age<60000?(age/1000).toFixed(1)+' s':Math.floor(age/60000)+' min';
 return {value:b.code===' ' ? '(space)' : b.code || 'Not decoded',detail:
   (b.code?'Read ':'Scan did not decode ')+ago+' ago'+(stale?' · telemetry stale':'')+' · '+b.elements+' / 29 elements (last capture)'};
}
const actionLabels=['Ignore','Turn right','Turn left','Straight · 100 mm','U-turn'];
function barcodeInstruction(packet,status,now=Date.now()){
 const barcode=packet?.barcode;
 if(!barcode?.supported || !barcode.available || !barcode.seen || !barcode.code)return '—';
 if(!/^[A-Z]$/.test(barcode.code))return 'No instruction';
 if(!status || status.bootId!==packet.bootId || !(status.flags&16) ||
    !Number.isFinite(status.receivedAt) || now-status.receivedAt>1500)return 'Mapping unavailable';
 return actionLabels[status.mappings?.[barcode.code.charCodeAt(0)-65]] ?? 'Mapping unavailable';
}
module.exports={barcodeReading,barcodeInstruction,actionLabels};

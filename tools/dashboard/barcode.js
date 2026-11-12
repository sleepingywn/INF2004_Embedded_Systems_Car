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
module.exports={barcodeReading};

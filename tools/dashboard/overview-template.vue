<template>
  <iframe ref="overview" src="/robocar/index.html" title="RoboCar live overview"
    @load="deliver" :style="{width:'100%',height:height+'px',border:0,display:'block'}"></iframe>
</template>
<script>
export default {
  data() { return {height:980, receiveOverview:null, lastTelemetry:null, lastControl:null}; },
  watch: {msg: {deep:true, handler() { this.deliver(); }}},
  methods: {
    deliver() {
      const p=this.msg?.payload;
      if(p?.kind==='controlStatus')this.lastControl=p;
      else if(p && !p.kind)this.lastTelemetry=p;
      const send=(type,payload)=>{if(payload)this.$refs.overview?.contentWindow?.postMessage(
        {type,payload:JSON.parse(JSON.stringify(payload))},window.location.origin);};
      send('robocar-telemetry',this.lastTelemetry);
      send('robocar-control',this.lastControl);
      if(p?.kind==='controlFeedback')send('robocar-control-feedback',p);
    }
  },
  mounted() {
    this.receiveOverview = event => {
      if(event.origin!==window.location.origin || event.source!==this.$refs.overview?.contentWindow) return;
      if(event.data?.type==='robocar-ready') this.deliver();
      if(event.data?.type==='robocar-command') {
        const p=event.data.payload;
        if(p && ['start','stop','keep','mapping'].includes(p.action) && typeof p.viewer==='string')
          this.send({payload:{kind:'controlRequest',action:p.action,viewer:p.viewer,mappings:p.mappings,revision:p.revision}});
      }
      if(event.data?.type==='robocar-resize' && Number.isFinite(event.data.height))
        this.height=Math.max(500,Math.min(4000,event.data.height));
    };
    window.addEventListener('message',this.receiveOverview);
    this.deliver();
  },
  unmounted() { window.removeEventListener('message',this.receiveOverview); }
}
</script>

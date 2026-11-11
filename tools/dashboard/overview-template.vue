<template>
  <iframe ref="overview" src="/robocar/index.html" title="RoboCar live overview"
    @load="deliver" :style="{width:'100%',height:height+'px',border:0,display:'block'}"></iframe>
</template>
<script>
export default {
  data() { return {height:980, receiveOverview:null}; },
  watch: {msg: {deep:true, handler() { this.deliver(); }}},
  methods: {
    deliver() {
      if (this.msg?.payload) this.$refs.overview?.contentWindow?.postMessage(
        {type:'robocar-telemetry',payload:JSON.parse(JSON.stringify(this.msg.payload))},window.location.origin);
    }
  },
  mounted() {
    this.receiveOverview = event => {
      if(event.origin!==window.location.origin || event.source!==this.$refs.overview?.contentWindow) return;
      if(event.data?.type==='robocar-ready') this.deliver();
      if(event.data?.type==='robocar-resize' && Number.isFinite(event.data.height))
        this.height=Math.max(500,Math.min(4000,event.data.height));
    };
    window.addEventListener('message',this.receiveOverview);
    this.deliver();
  },
  unmounted() { window.removeEventListener('message',this.receiveOverview); }
}
</script>

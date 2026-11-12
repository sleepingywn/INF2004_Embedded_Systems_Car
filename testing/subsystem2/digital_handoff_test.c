#define main route_cases_main
#include "line_route_test.c"
#undef main
#include "../../app_program/subsystem2/motion_ir_bridge.c"
#undef memset
#undef memcpy
static motion_ir_bridge_t b;
static uint32_t t, position;
static bool black;
static void tick(void) {
 uint8_t normalized[3]={black?100:0,0,0};
 ++t;clock_ms=t;
 assert(motion_ir_bridge_sample(&b,normalized,true,t,state.epoch)==E_OK);
}
int main(void) {
 reset_owner(1,2000);
 shared.status=state;
 motion_ir_bridge_init(&b);
 tick();tick();
 bc_msg_t pattern=make_code("WNNNNWNNW",false,20,60); /* A */
 black=true;bc_edge_irq((t+1)*1000,position,0,black,false);tick();
 for(unsigned i=0;i<29;++i){
  for(unsigned n=1;n<pattern.widths[i];++n)tick();
  position+=pattern.widths[i]/2;bc_edge_position(position);
  black=!black;bc_edge_irq((t+1)*1000,position,0,black,false);tick();
 }
 tick();tick();assert(!shared.have_barcode); /* wait for quiet travel */
 position+=120;bc_edge_position(position);tick();
 assert(shared.have_barcode && shared.barcode.hdr.count==29 && shared.barcode.distance_valid);
 assert(shared.barcode.distance[0]==10);
 route_on_barcode(&shared.barcode,shared.barcode_ms,false);
 assert(barcode_status.code=='A' && barcode_status.elements==29);
 /* Epoch change discards partial edges and cannot replay the previous capture. */
 shared.have_barcode=false; ++state.epoch;shared.status=state;
 tick();assert(!b.edges.active && !shared.have_barcode);
 puts("PASS digital IRQ-ring -> S3 bridge -> real mailbox -> owner decoder -> telemetry; epoch reset");
 return 0;
}

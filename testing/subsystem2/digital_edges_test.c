#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem3/barcode_edges.h"
#include "../../app_program/subsystem2/motion_route_decode.h"
static uint32_t now, pos;
static bc_edges_t c;
static bc_msg_t out;
static void reset(uint32_t start) {
 now=start; pos=0; bc_edge_position(0); bc_edges_reset(&c);
 assert(!bc_edges_poll(&c,now,false,&out));
}
static void edge(bool black, uint32_t dt, unsigned distance) {
 now+=dt; pos+=distance; bc_edge_position(pos);
 bc_edge_irq(now,pos,0,black,false);
 assert(!bc_edges_poll(&c,now,black,&out));
}
static void symbol(bool reverse, bool moving, bool variable) {
 const char *data=NULL;
 for(unsigned i=0;i<sizeof code39_table/sizeof code39_table[0];++i)
  if(code39_table[i].code=='A')data=code39_table[i].p;
 char widths[29];
 for(unsigned i=0;i<9;++i){widths[i]=CODE39_STAR[i];widths[i+10]=data[i];widths[i+20]=CODE39_STAR[i];}
 widths[9]=widths[19]='N';
 edge(true,10000,0);
 for(unsigned i=0;i<29;++i){
  unsigned k=reverse?28-i:i, scale=widths[k]=='W'?3:1;
  edge((i%2)!=0,scale*(variable?(i<14?10000:30000):20000),moving?scale*10:0);
 }
 assert(!bc_edges_poll(&c,now+BC_GLITCH_US,false,&out)); /* never finish just at 29 */
 if(moving){pos+=120;bc_edge_position(pos);now+=10000;}
 else now+=BC_EDGE_QUIET_US+1;
 assert(bc_edges_poll(&c,now,false,&out));
 assert(out.hdr.count==29 && !out.hdr.overflow);
 assert(out.distance_valid==moving);
 if(moving){for(unsigned i=0;i<29;++i)out.widths[i]=out.distance[i];}
 assert(route_code39_decode(&out)=='A');
 /* Full star validation must reject malformed marker spaces. */
 out.widths[1]=out.widths[3]=out.widths[5]=out.widths[7]=10;
 assert(!route_code39_decode(&out));
}
int main(void){
 reset(0);symbol(false,false,false);
 reset(0);symbol(true,true,false);
 reset(0);pos=0xfffffff0u;bc_edge_position(pos);symbol(false,true,false); /* encoder wrap */
 reset(0xffff0000u);symbol(false,true,true); /* timer wrap, changing speed */
 reset(0);edge(true,10000,0);edge(false,500,0);
 assert(!bc_edges_poll(&c,now+5000,false,&out));assert(!c.active && c.glitches==1);
 reset(0);for(unsigned i=0;i<BC_EDGE_RING+1;++i)bc_edge_irq(now+i,0,0,i%2,false);
 assert(bc_edges_poll(&c,now+1000,false,&out));assert(out.hdr.overflow && out.dropped);
 reset(0);bc_edge_irq(now+100,0,0,true,true);
 assert(bc_edges_poll(&c,now+1000,false,&out));assert(out.hdr.overflow);
 reset(0);edge(true,10000,0);assert(!bc_edges_poll(&c,now+3000,true,&out));
 assert(bc_edges_poll(&c,now+BC_EDGE_STALL_US+1,true,&out));assert(out.hdr.reserved==2 && out.hdr.overflow);
 reset(0);edge(true,10000,0);edge(false,20000,0);
 assert(bc_edges_poll(&c,now+BC_EDGE_QUIET_US+1,false,&out));assert(out.hdr.reserved==3);
 /* A reset/epoch change cannot replay an old capture. */
 reset(0);edge(true,10000,0);bc_edges_reset(&c);
 assert(!bc_edges_poll(&c,now+5000,true,&out));assert(!c.active);
 puts("PASS digital edges: forward/reverse A, hand swipe, actual distance widths, speed changes, timer wrap, chatter, quiet zone, overflow, ambiguous IRQ, stall, partial, epoch reset");
}

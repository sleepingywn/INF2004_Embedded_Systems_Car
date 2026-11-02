#include "transport_mock.h"
#include "../../app_program/subsystem1/telemetry_transport.h"
#include <assert.h>
#include <stdio.h>
static int locked, busy;
int tk_cre_mtx(const T_CMTX *c) {assert(c->mtxatr==TA_INHERIT);return 1;}
int tk_loc_mtx(ID id,int timeout) {assert(id==1 && timeout==TMO_POL);if(busy)return -1;assert(!locked);locked=1;return 0;}
int tk_unl_mtx(ID id) {assert(id==1 && locked);locked=0;return 0;}
int main(void) {
 uint8_t f[S1_FRAME_SIZE]={0},out[S1_FRAME_SIZE];int event;uint32_t ticket,old;
 assert(s1_transport_init()==0);
 assert(!s1_transport_peek(out,&event,&ticket));
 f[0]=1;assert(s1_transport_offer(f,0));assert(s1_transport_peek(out,&event,&old));assert(!event&&out[0]==1);
 f[0]=2;assert(s1_transport_offer(f,0));s1_transport_ack(0,old);
 assert(s1_transport_peek(out,&event,&ticket)&&out[0]==2);
 busy=1;s1_transport_ack(0,ticket);assert(!s1_transport_offer(f,0));busy=0;
 assert(s1_transport_peek(out,&event,&ticket)&&out[0]==2);
 s1_transport_ack(0,ticket);assert(!s1_transport_peek(out,&event,&ticket));
 for(unsigned i=0;i<8;++i){f[0]=(uint8_t)i;assert(s1_transport_offer(f,1));}
 assert(!s1_transport_offer(f,1));assert(s1_transport_dropped()==1);
 for(unsigned i=0;i<8;++i){assert(s1_transport_peek(out,&event,&ticket));assert(event&&out[0]==i);
   old=ticket;s1_transport_ack(1,ticket+1);assert(s1_transport_peek(out,&event,&ticket)&&ticket==old);s1_transport_ack(1,ticket);}
 assert(!s1_transport_peek(out,&event,&ticket));puts("Transport overflow, FIFO, coalescing, contention and acknowledgement checks: PASS");
 return 0;
}

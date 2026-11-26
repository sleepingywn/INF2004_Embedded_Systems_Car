#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "motion_host_shim.h"
#define SUBSYSTEM1_H
#include "../../app_program/subsystem1/telemetry_transport.c"
static bool busy;
ER tk_loc_mtx(ID a,int b){(void)a;(void)b;return busy ? E_TMOUT:E_OK;}
ER tk_unl_mtx(ID a){(void)a;return E_OK;}
ID tk_cre_mtx(const T_CMTX *a){(void)a;return 1;}
int main(void){
 uint8_t b[24]={'R','C',1,CONTROL_START};control_command_t c;
 control_put(b+4,123);control_put(b+8,7);control_put(b+12,1);control_put(b+16,0xfffffff0u);control_put(b+20,9);
 assert(control_decode(b,24,&c));assert(control_fresh(&c,123,7,9,16));
 assert(!control_fresh(&c,124,7,9,16)&&!control_fresh(&c,123,8,9,16)&&!control_fresh(&c,123,7,10,16));
 assert(!control_fresh(&c,123,7,9,1000)&&!control_fresh(&c,123,7,9,0xffffffefu));
 assert(!control_decode(b,23,&c)&&!control_decode(b,25,&c));b[3]=4;assert(!control_decode(b,24,&c));b[3]=1;
 assert(!control_expired(0x10,0xfffffff0u,0xfffffff0u));assert(!control_expired(4999,0,0));assert(control_expired(5000,0,0));
 assert(s1_transport_init()==0);s1_control_boot(123);s1_control_link(7,1);
 assert(s1_control_offer(b,24,16));for(unsigned i=1;i<CONTROL_QUEUE_DEPTH;++i){control_put(b+12,i+1u);assert(s1_control_offer(b,24,16));}
 assert(!s1_control_offer(b,24,16));
 assert(s1_control_take(&c,16,0,9,true) && c.id==1);
 assert(s1_control_take(&c,16,0,9,true) && c.id==2);
 b[3]=CONTROL_STOP;assert(s1_control_offer(b,24,16));assert(s1_control_take(&c,16,0,9,true)&&c.op==CONTROL_STOP);assert(!s1_control_take(&c,16,0,9,true));
 assert(s1_control_offer(b,24,16));s1_control_link(8,1);assert(!s1_control_take(&c,16,0,9,true)&&!s1_control_offer(b,24,16));
 control_put(b+8,8);s1_control_link(8,0);assert(!s1_control_offer(b,24,16));
 s1_control_link(8,1);busy=true;assert(s1_control_offer(b,24,16));busy=false;assert(s1_control_take(&c,16,0,9,true) && c.op==CONTROL_STOP);
 /* A replayed STOP cannot flush queued work in the current epoch. */
 b[3]=CONTROL_START;assert(s1_control_offer(b,24,16));
 b[3]=CONTROL_STOP;control_put(b+20,8);assert(s1_control_offer(b,24,16));
 assert(!s1_control_take(&c,16,0,9,true));
 assert(s1_control_take(&c,16,0,9,true) && c.op==CONTROL_START);
 control_put(b+20,9);
 uint8_t settings[54]={'R','C',2,4};memcpy(settings+4,b+4,20);control_put(settings+24,17);
 for(unsigned i=0;i<26;++i)settings[28+i]=i%5;
 assert(control_decode(settings,54,&c)&&c.revision==17&&c.mapping[24]==4);
 assert(!control_decode(settings,53,&c)&&!control_decode(settings,55,&c));
 settings[53]=5;assert(!control_decode(settings,54,&c));settings[53]=0;
 assert(s1_control_offer(settings,54,16));assert(s1_control_offer(b,24,16));assert(s1_control_take(&c,16,0,9,true)&&c.op==CONTROL_STOP);
 puts("PASS mapping wire: exact v2 framing, all actions/revision, invalid action rejection and STOP override");
 puts("PASS wire/transport: exact framing, boot/session/epoch/expiry checks, uptime wrap, STOP priority, reconnect flush, contention rejection");
}

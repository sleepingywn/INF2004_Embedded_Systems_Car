#define main route_suite_main
#include "line_route_test.c"
#undef main
static int guard_case;
static void guard_tick(void) {
 if(clock_ms==40){
  assert(motion_remote_watch(state.epoch,1,clock_ms,clock_ms)==E_OK);
  if(guard_case!=3)shared.request.source=MOTION_NETWORK;
 }
 if(guard_case==2 && clock_ms%400==0 && owner_flags.active_valid)
  assert(motion_remote_watch(state.epoch,1,clock_ms,40)==E_OK);
}
int main(void) {
 for(guard_case=1;guard_case<=3;++guard_case){
  reset_owner(1,guard_case==2?30600:5200);owner_hook=guard_tick;
  if(!setjmp(done))motion_task(0,NULL);
  if(guard_case!=3){assert(!out_left&&!out_right&&!owner_flags.active_valid);assert(state.result==MOTION_EXPIRED);}
  else assert(owner_flags.active_valid); /* a leftover guard for the SAME id cannot stop a LOCAL GP20 request */
 }
 puts("PASS motor owner: missing heartbeat stops, total 30s bound survives route handover, local run unaffected by leftover remote guard");
}

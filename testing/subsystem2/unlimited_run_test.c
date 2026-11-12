#define main route_suite_main
#include "line_route_test.c"
#undef main
static bool network,renew;
static void unlimited_tick(void) {
 if(clock_ms==40 && network){
  assert(motion_remote_watch(state.epoch,1,clock_ms,clock_ms)==E_OK);
  shared.request.source=MOTION_NETWORK;
 }
 if(network && renew && clock_ms%400==0 && owner_flags.active_valid)
  assert(motion_remote_watch(state.epoch,1,clock_ms,40)==E_OK);
 if(network && !renew && clock_ms%400==0 && clock_ms<60000 && owner_flags.active_valid)
  assert(motion_remote_watch(state.epoch,1,clock_ms,40)==E_OK);
}
int main(void) {
 assert(MOTION_LINE_UNLIMITED);
 network=false;renew=false;reset_owner(5,61000);owner_hook=unlimited_tick;
 if(!setjmp(done))motion_task(0,NULL);
 assert(owner_flags.active_valid&&!state.faults&&!has(ROUTE_EV_TIME_LIMIT));
 network=true;renew=true;reset_owner(5,61000);owner_hook=unlimited_tick;
 if(!setjmp(done))motion_task(0,NULL);
 assert(owner_flags.active_valid&&!state.faults);
 renew=false;reset_owner(5,62000);owner_hook=unlimited_tick;
 if(!setjmp(done))motion_task(0,NULL);
 assert(!owner_flags.active_valid&&!out_left&&!out_right&&state.result==MOTION_EXPIRED);
 puts("PASS no-time-limit: actual local and remote owner run beyond 60s; heartbeat loss still stops after 60s");
 return 0;
}

/* Actual console/GP20 arbiter with fake transport and real motion request APIs. */
#define main route_suite_main
#include "line_route_test.c"
#undef main
#include <stdarg.h>
typedef unsigned char UB;
#define GPIO_MODE_IN 0
#define GPIO(x) (x)
#define GPIO_PUE 1
#define GPIO_IE 2
#define GPIO_PDE 4
static bool pressed;
static void gpio_set_pin(unsigned p,unsigned m){(void)p;(void)m;}
static void set_w(unsigned p,unsigned v){(void)p;(void)v;}
static void clr_w(unsigned p,unsigned v){(void)p;(void)v;}
static unsigned gpio_get_val(unsigned p){(void)p;return !pressed;}
static int tm_printf(UB *fmt,...){(void)fmt;return 0;}
static int tm_usb_enter_bootsel(void){return 0;}
#include "../../app_program/subsystem2/line_console.c"
static control_command_t queued;
static bool available,link=true,healthy=true;
static uint32_t connection=1;
static uint8_t latest_control[CONTROL_STATUS_SIZE];
int s1_control_context(uint32_t *b,uint32_t *s,int *c){*b=123;*s=connection;*c=link;return 1;}
int s1_control_take(control_command_t *c,uint32_t now,uint32_t last_id,uint32_t epoch,bool fresh){(void)now;(void)last_id;(void)epoch;(void)fresh;if(!available)return 0;*c=queued;available=false;return 1;}
int s1_control_status_offer(const uint8_t *b){memcpy(latest_control,b,sizeof latest_control);return 1;}
/* This control fixture does not generate barcode sensor samples. */
unsigned ir_runtime_barcode_levels(uint16_t *out, unsigned max, uint32_t *seq)
{
    (void)out;
    (void)max;
    if (seq) *seq = 0;
    return 0;
}
ER ir_runtime_get_status(ir_runtime_status_t *ir){*ir=(ir_runtime_status_t){0};ir->state=IR_RUNTIME_READY;ir->healthy=healthy;ir->updated_ms=ir->raw.sampled_ms=clock_ms;return E_OK;}
static void test_poll(void){observer_status=shared.status;remote_poll();}
static void setup(void){
 mapping_pending=false;settings_reboots=0;route_mapping_revision=0;
 button_retry_allowed=button_retry_gesture=button_restart_pending=button_retry_waiting=false;
 line_indicator_clear(&indicator);
 reset_owner(1,2000);memset(&remote,0,sizeof remote);button=(line_button_t){.armed=true,.ready=true};
 button_boot_pending=button_reset_pending=boot_pending=false;pressed=false;link=healthy=true;connection=1;
 clock_ms=100;shared.status=state;shared.status.ready=shared.status.raw_valid=true;
 shared.status.state=MOTION_IDLE;shared.status.faults=0;shared.status.updated_ms=clock_ms;
 test_poll();
}
static void command(unsigned op,unsigned id){queued=(control_command_t){.op=op,.boot=123,.session=connection,.id=id,.issued=clock_ms,.epoch=shared.status.epoch};available=true;test_poll();}
int main(void){
 setup();motion_request_t unsafe;
 assert(motion_request_init(&unsafe,MOTION_LINE_TRACE,MOTION_NETWORK,shared.status.epoch,1,30000,0,0,0)==E_OK);
 assert(motion_submit(&unsafe)==E_OBJ); /* network request requires an installed watchdog */
 setup();command(CONTROL_START,1);assert(remote.stage==1&&button.started&&!button.armed&&shared.pending);
 assert(remote.ack==CONTROL_QUEUED && shared.request.source==MOTION_NETWORK);uint32_t request=shared.request.id;
 command(CONTROL_START,1);assert(shared.request.id==request); /* duplicate cannot restart */
 clock_ms+=100;shared.status.updated_ms=clock_ms;shared.status.request_epoch=remote.epoch;
 shared.status.request_id=request;shared.status.state=MOTION_TRACING;shared.status.result=MOTION_ACCEPTED;
 test_poll();assert(remote.ack==CONTROL_RUNNING);
 uint32_t renewed=remote.renewed;command(CONTROL_KEEP,99);assert(remote.renewed==renewed);
 command(CONTROL_KEEP,1);assert(remote.renewed==clock_ms);
 command(CONTROL_STOP,2);assert(remote.stage==3&&!button.started&&!button.armed&&remote.ack==CONTROL_QUEUED);
 ++shared.status.epoch;shared.status.state=MOTION_IDLE;shared.status.updated_ms=++clock_ms;
 test_poll();assert(remote.ack==CONTROL_STOPPED&&remote.stage==0);
 command(CONTROL_START,3);assert(remote.ack==CONTROL_REJECTED); /* no implicit re-arm */
 setup();pressed=true;command(CONTROL_START,1);assert(remote.ack==CONTROL_REJECTED&&!shared.pending);
 setup();healthy=false;command(CONTROL_START,1);assert(remote.ack==CONTROL_REJECTED&&!shared.pending);
 setup();queued=(control_command_t){.op=1,.boot=123,.session=1,.id=1,.issued=clock_ms-1001,.epoch=shared.status.epoch};available=true;test_poll();assert(!shared.pending);
 setup();command(1,1);link=false;test_poll();assert(remote.stage==3&&!button.started);
 setup();command(1,1);connection=2;test_poll();assert(remote.stage==3&&!button.started);
 setup();command(1,1);pressed=true;line_console_key(-1);test_poll();assert(remote.stage==3&&!button.started);
 setup();shared.status.updated_ms=0;clock_ms=1000;command(CONTROL_STOP,1);assert(remote.stage==3); /* STOP even with unavailable motion */
 setup();queued=(control_command_t){.op=4,.boot=123,.session=1,.id=1,.issued=clock_ms,.epoch=shared.status.epoch};
 assert(route_action_set(&queued.mapping,25,4));available=true;test_poll();assert(mapping_pending&&remote.stage==3&&!settings_reboots&&!button.armed);
 test_poll();assert(!settings_reboots); /* no flash until STOP changes epoch */
 ++shared.status.epoch;shared.status.updated_ms=++clock_ms;test_poll();assert(settings_reboots==1&&!mapping_pending);
 setup();shared.status.state=MOTION_TRACING;queued.epoch=shared.status.epoch;queued.issued=clock_ms;available=true;test_poll();assert(mapping_pending && remote.stage==0 && !settings_reboots);
 assert(route_mapping_code('Z')=='D' && route_mapping_revision==1);
 shared.status.state=MOTION_IDLE;test_poll();assert(remote.stage==3&&!settings_reboots);
 ++shared.status.epoch;shared.status.updated_ms=++clock_ms;test_poll();assert(settings_reboots==1);
 setup();queued.revision=1;queued.epoch=shared.status.epoch;queued.issued=clock_ms;available=true;test_poll();assert(remote.ack==CONTROL_REJECTED&&!mapping_pending);
 setup();queued.revision=0;queued.mapping.words[0]=5u << route_action_shift(3);queued.epoch=shared.status.epoch;queued.issued=clock_ms;available=true;test_poll();assert(remote.ack==CONTROL_REJECTED&&!mapping_pending);
 /* Completed local run: rearm gesture only; RESET waits for a fresh click. */
 setup();button=(line_button_t){.started=true};button_retry_allowed=true;
 button_retry_poll();clock_ms+=601;shared.status.updated_ms=clock_ms;observer_status=shared.status;
 button_retry_poll();assert(button.armed&&button_retry_gesture&&!shared.pending);
 clock_ms+=31;shared.status.updated_ms=clock_ms;observer_status=shared.status;line_console_key(-1);
 pressed=true;line_console_key(-1);clock_ms+=31;shared.status.updated_ms=clock_ms;observer_status=shared.status;line_console_key(-1);
 pressed=false;line_console_key(-1);clock_ms+=31;shared.status.updated_ms=clock_ms;observer_status=shared.status;line_console_key(-1);
 assert(button_restart_pending&&button_reset_pending&&shared.request.command==MOTION_RESET);
 assert(!button.started); /* no trace before exact reset acknowledgement */
 shared.pending=false;shared.status.epoch=button_reset_epoch;shared.status.request_id=button_reset_id;
 shared.status.result=MOTION_COMPLETED;shared.status.updated_ms=++clock_ms;observer_status=shared.status;
 button_prepare();assert(!button_restart_pending&&button.started&&shared.request.command==MOTION_LINE_TRACE);
 /* Held stop button requires release THEN another full press/release. */
 setup();button_retry_allowed=true;pressed=true;button_retry_poll();clock_ms+=601;shared.status.updated_ms=clock_ms;observer_status=shared.status;button_retry_poll();
 clock_ms+=100;shared.status.updated_ms=clock_ms;observer_status=shared.status;line_console_key(-1);assert(!shared.pending);
 pressed=false;line_console_key(-1);clock_ms+=31;shared.status.updated_ms=clock_ms;observer_status=shared.status;line_console_key(-1);assert(!shared.pending&&button.ready);
 /* Fatal fault and active configuration/remote transactions block retry. */
 for(unsigned fault=MOTION_FAULT_ENCODER;fault<=MOTION_FAULT_HARDWARE;fault<<=1){
  if(fault==MOTION_FAULT_IR||fault==MOTION_FAULT_TIMEOUT||fault==MOTION_FAULT_RANGE)continue;
  setup();button=(line_button_t){0};button_retry_allowed=true;shared.status.faults=fault;observer_status=shared.status;button_retry_poll();assert(!button_retry_waiting&&!button.armed);
 }
 setup();button=(line_button_t){0};button_retry_allowed=true;mapping_pending=true;button_retry_poll();assert(!button_retry_waiting);
 setup();button=(line_button_t){0};button_retry_allowed=true;healthy=false;button_retry_poll();assert(!button_retry_waiting);
 /* Recoverable line-loss stop offers a fresh gesture, without clearing fault. */
 setup();button=(line_button_t){0};button_retry_allowed=true;shared.status.state=MOTION_ERROR;shared.status.faults=MOTION_FAULT_IR;observer_status=shared.status;
 button_retry_poll();clock_ms+=601;shared.status.updated_ms=clock_ms;observer_status=shared.status;button_retry_poll();
 assert(button.armed&&button_retry_gesture&&shared.status.faults==MOTION_FAULT_IR&&!shared.pending);
 /* STOP cancels the queued restart; a missing RESET ack never starts. */
 setup();button_restart_pending=true;assert(button_queue_reset()==E_OK);shared.pending=false;
 clock_ms+=1001;shared.status.updated_ms=clock_ms;observer_status=shared.status;button_prepare();assert(!button_restart_pending&&!shared.pending);
 setup();button_restart_pending=true;assert(button_queue_reset()==E_OK);line_console_key('x');assert(!button_restart_pending&&!button_reset_pending);
 /* New stall between gesture preparation and RESET cannot be cleared. */
 setup();button_restart_pending=true;shared.status.faults=MOTION_FAULT_STALL;observer_status=shared.status;
 assert(button_queue_reset()<E_OK&&!shared.pending);
 puts("PASS GP20 retry: fresh gesture, exact reset acknowledgement, held-button protection, fault and transaction guards");
 puts("PASS mapping arbiter: live RAM update and deferred save, STOP acknowledgement before reboot, stale revision and invalid action rejection");
 puts("PASS console: queued vs accepted, duplicate/stale/held-button/unhealthy rejection, owner keepalive, STOP confirmation, GP20 override, disconnect/reconnect cancellation");
}

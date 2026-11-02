#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "motion_host_shim.h"
#include "../../app_program/subsystem2/motion.h"
#define IR_RUNTIME_H
#define IR_RUNTIME_READY 2
typedef unsigned char UB;
typedef struct {
    uint32_t updated_ms;
    struct {uint32_t sampled_ms;} raw;
    unsigned state;
    bool healthy,command_pending;
} ir_runtime_status_t;
ER ir_runtime_get_status(ir_runtime_status_t *s);
void tm_printf(UB *format,...);
#if MOTION_LINE_CIRCUIT
#define GPIO_MODE_IN 0
#define GPIO(p) (p)
#define GPIO_PUE 1
#define GPIO_IE 2
#define GPIO_PDE 4
static bool pin_pressed;
static uint32_t test_clock=1000;
static int gpio_set_pin(unsigned pin,unsigned mode){assert(pin==20 && mode==0);return 0;}
static void set_w(unsigned pin,unsigned bits){assert(pin==20);(void)bits;}
static void clr_w(unsigned pin,unsigned bits){assert(pin==20);(void)bits;}
static unsigned gpio_get_val(unsigned pin){assert(pin==20);return !pin_pressed;}
#endif
#include "../../app_program/subsystem2/line_console.c"
static unsigned submitted,stops;
static motion_command_t last_command;
static unsigned fake_state, fake_faults;
static uint32_t fake_epoch=1,fake_id;
static unsigned fake_result;
static ir_runtime_status_t sensor;
uint32_t motion_now_ms(void){
#if MOTION_LINE_CIRCUIT
    return test_clock;
#else
    return 1000;
#endif
}
ER ir_runtime_get_status(ir_runtime_status_t *s){*s=sensor;return E_OK;}
ER motion_get_status(motion_status_t *s){*s=(motion_status_t){.epoch=fake_epoch,.request_id=fake_id,.result=fake_result,.ready=1,.raw_valid=1,.updated_ms=motion_now_ms(),.state=fake_state,.faults=fake_faults};return E_OK;}
ER motion_get_ir_status(motion_status_t *s){return motion_get_status(s);}
ER motion_get_diagnostics(motion_diagnostics_t *d){*d=(motion_diagnostics_t){0};return E_OK;}
ER motion_debug_line(line_msg_t *l,uint32_t *t){*l=(line_msg_t){0};*t=1000;return E_OK;}
ER motion_request_init(motion_request_t *r,motion_command_t c,motion_source_t src,
    uint32_t epoch,uint32_t id,uint32_t ttl,int32_t value,int32_t left,int32_t right)
{
    assert(src==MOTION_LOCAL && epoch==fake_epoch && id==fake_id+1u && !value && !left && !right);
    assert(ttl==(c==MOTION_RESET?1000u:LINE_FOLLOW_MAX_MS));
    *r=(motion_request_t){0};r->command=c;r->id=id;return E_OK;
}
ER motion_submit(const motion_request_t *r){++submitted;last_command=r->command;return E_OK;}
ER motion_stop(void){++stops;return E_OK;}
void tm_printf(UB *format,...){(void)format;}
#if MOTION_LINE_CIRCUIT
static void poll_button(bool pressed) {
    pin_pressed=pressed;test_clock+=100;
    sensor.updated_ms=sensor.raw.sampled_ms=test_clock;
    (void)motion_line_console_key(-1);
}
static void circuit_cases(void) {
    pin_pressed=true;
    poll_button(true);poll_button(false);poll_button(false);
    assert(!submitted); /* boot/unarmed cannot move */
    sensor.state=IR_RUNTIME_READY;sensor.healthy=true;
    pin_pressed=true;
    motion_line_console_key('r');assert(submitted==1);
    motion_line_console_key('g');assert(submitted==1); /* no USB start */
    poll_button(true);poll_button(false);poll_button(false);
    assert(submitted==1); /* button held at arm must first release */
    poll_button(true);poll_button(true);assert(submitted==1);
    poll_button(false);poll_button(false);
    assert(submitted==2 && last_command==MOTION_LINE_TRACE);
    poll_button(false);assert(submitted==2);
    poll_button(true);assert(stops==1); /* first observed press stops */
    poll_button(true);poll_button(false);poll_button(false);
    poll_button(true);poll_button(true);poll_button(false);poll_button(false);
    assert(submitted==2); /* one USB arm permits only one run */
    motion_line_console_key('r');assert(submitted==3);
    motion_line_console_key('x');assert(stops==2);
    poll_button(false);poll_button(true);poll_button(true);poll_button(false);poll_button(false);
    assert(submitted==3);
    motion_line_console_key('r');assert(submitted==4);
    motion_line_console_key('c');
    poll_button(false);poll_button(false);poll_button(true);poll_button(true);poll_button(false);poll_button(false);
    assert(submitted==4); /* calibration cancels permission */
    motion_line_console_key('r');assert(submitted==5);
    poll_button(false);poll_button(false);poll_button(true);poll_button(true);
    fake_faults=MOTION_FAULT_IR;
    poll_button(false);poll_button(false);assert(submitted==5);
    fake_faults=0;
    motion_line_console_key('r');assert(submitted==6);
    poll_button(false);poll_button(false);poll_button(true);poll_button(true);poll_button(false);poll_button(false);
    assert(submitted==7);fake_state=MOTION_TRACING;
    motion_line_console_key('r');assert(stops==3 && submitted==7);
    fake_state=MOTION_IDLE;
    puts("PASS: GP20 one-shot arm/press/release, held-at-arm, USB start disabled, press STOP, disarm and calibration cancellation");
}
#endif
int main(void)
{
    assert(!motion_line_console_key('o')); /* no fault bypass */
#if MOTION_LINE_CIRCUIT
    circuit_cases();return 0;
#endif
    if(!MOTION_LINE_DEMO) {
        assert(!motion_line_console_key('r') && !motion_line_console_key('g'));
        assert(!submitted && !stops);puts("PASS: default diagnostic console has no motion keys");return 0;
    }
    assert(motion_line_console_key('g') && !submitted);
    assert(motion_line_console_key('r') && !submitted); /* unhealthy IR */
    sensor=(ir_runtime_status_t){.state=IR_RUNTIME_READY,.healthy=true,.updated_ms=1000,.raw={1000}};
    assert(motion_line_console_key('r') && submitted==1 && last_command==MOTION_RESET);
    assert(motion_line_console_key('g') && submitted==2 && last_command==MOTION_LINE_TRACE);
    assert(motion_line_console_key('g') && submitted==2); /* held/queued g */
    motion_line_console_key('r');assert(submitted==3);
    motion_line_console_key('x');assert(stops==1);
    motion_line_console_key('g');assert(submitted==3);
    sensor.updated_ms=800;motion_line_console_key('r');assert(submitted==3);
    sensor.updated_ms=1000;sensor.command_pending=true;
    motion_line_console_key('r');assert(submitted==3);
    puts("PASS: health/freshness checks, explicit reset/arm, one run, stop disarm and no IR bypass");
}

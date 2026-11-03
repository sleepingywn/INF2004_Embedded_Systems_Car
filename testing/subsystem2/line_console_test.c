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
int tm_usb_enter_bootsel(void);
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
static unsigned bootsel_calls;
static int fake_left_duty,fake_right_duty;
static uint32_t fake_age;
static ER fake_snapshot_error;
static ir_runtime_status_t sensor;
uint32_t motion_now_ms(void){
#if MOTION_LINE_CIRCUIT
    return test_clock;
#else
    return 1000;
#endif
}
ER ir_runtime_get_status(ir_runtime_status_t *s){*s=sensor;return E_OK;}
ER motion_get_status(motion_status_t *s){
    if(fake_snapshot_error)return fake_snapshot_error;
    *s=(motion_status_t){.epoch=fake_epoch,.request_epoch=fake_epoch,.request_id=fake_id,.result=fake_result,.ready=1,.raw_valid=1,.updated_ms=motion_now_ms()-fake_age,.state=fake_state,.faults=fake_faults,
        .left_duty_bits=(uint16_t)motion_bits_encode(fake_left_duty,11u),.right_duty_bits=(uint16_t)motion_bits_encode(fake_right_duty,11u)};
    return E_OK;
}
ER motion_get_ir_status(motion_status_t *s){return motion_get_status(s);}
ER motion_get_diagnostics(motion_diagnostics_t *d){*d=(motion_diagnostics_t){0};return E_OK;}
static unsigned trace_count,trace_reads;
static ER trace_error;
ER motion_get_line_trace(unsigned index,motion_line_trace_entry_t *out){
    ++trace_reads;
    if(trace_error)return trace_error;
    if(index>=trace_count)return E_NOEXS;
    *out=(motion_line_trace_entry_t){.epoch=fake_epoch,.id=fake_id,.count=(uint16_t)trace_count};
    return E_OK;
}
ER motion_debug_line(line_msg_t *l,uint32_t *t){*l=(line_msg_t){0};*t=1000;return E_OK;}
ER motion_request_init(motion_request_t *r,motion_command_t c,motion_source_t src,
    uint32_t epoch,uint32_t id,uint32_t ttl,int32_t value,int32_t left,int32_t right)
{
    assert(src==MOTION_LOCAL && epoch==fake_epoch && id==fake_id+1u && !value && !left && !right);
    assert(ttl==(c==MOTION_RESET?1000u:LINE_FOLLOW_MAX_MS));
    *r=(motion_request_t){0};r->command=c;r->id=id;r->epoch=epoch;return E_OK;
}
ER motion_submit(const motion_request_t *r){++submitted;last_command=r->command;return E_OK;}
ER motion_stop(void){++stops;return E_OK;}
void tm_printf(UB *format,...){(void)format;}
int tm_usb_enter_bootsel(void){++bootsel_calls;return -1;}
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
static void poll_button(bool pressed) {
    pin_pressed=pressed;test_clock+=100;
    sensor.updated_ms=sensor.raw.sampled_ms=test_clock;
    (void)motion_line_console_key(-1);
}
static void reset_button_fixture(void) {
    line_indicator_clear(&indicator);
    button=(line_button_t){0};button_boot_pending=true;button_reset_pending=false;
    line_armed=false;submitted=stops=0;test_clock=1000;pin_pressed=false;
    fake_epoch=1;fake_id=0;fake_state=MOTION_ERROR;
    fake_faults=MOTION_FAULT_EXTERNAL;fake_result=MOTION_NONE;
    sensor=(ir_runtime_status_t){.state=IR_RUNTIME_READY,.healthy=true};
}
static void button_ack_reset(void) {
    fake_epoch++;fake_id++;fake_state=MOTION_IDLE;
    fake_faults=0;fake_result=MOTION_COMPLETED;
}
static void press_release(void) {
    poll_button(false);poll_button(false);
    poll_button(true);poll_button(true);poll_button(false);poll_button(false);
}
static void circuit_cases(void) {
    reset_button_fixture();sensor.healthy=false;press_release();assert(!submitted);
    sensor.healthy=true;poll_button(true);
    assert(submitted==1 && last_command==MOTION_RESET && !button.armed);
    press_release();assert(submitted==1); /* no start before RESET ack */
    pin_pressed=true;button_ack_reset();poll_button(true);
    assert(button.armed);poll_button(false);poll_button(false);
    assert(submitted==1); /* release of a button held at arming cannot start */
    motion_line_console_key('g');assert(submitted==1);
    press_release();assert(submitted==2 && last_command==MOTION_LINE_TRACE);
    poll_button(false);assert(submitted==2);
    poll_button(true);assert(stops==1);
    press_release();assert(submitted==2); /* no automatic repeat */
    motion_line_console_key('r');assert(submitted==3 && !button.armed);
    button_ack_reset();poll_button(false);press_release();assert(submitted==4);
    fake_state=MOTION_TRACING;
    motion_line_console_key('r');assert(stops==2 && submitted==4);

    reset_button_fixture();motion_line_console_key('x');press_release();assert(!submitted);
    reset_button_fixture();motion_line_console_key('c');press_release();assert(!submitted);
    reset_button_fixture();poll_button(false);motion_line_console_key('f');
    button_ack_reset();press_release();assert(submitted==1 && !button.armed);
    reset_button_fixture();fake_faults=MOTION_FAULT_ENCODER;press_release();assert(!submitted);
    reset_button_fixture();poll_button(false);test_clock+=1000;button_ack_reset();
    press_release();assert(submitted==1 && !button.armed); /* reset timed out */
    reset_button_fixture();poll_button(false);button_ack_reset();fake_epoch++;
    press_release();assert(submitted==1 && !button.armed); /* wrong acknowledgement */
    reset_button_fixture();poll_button(false);button_ack_reset();poll_button(false);
    sensor.healthy=false;press_release();assert(submitted==1 && !button.armed);
    reset_button_fixture();poll_button(false);button_ack_reset();poll_button(false);
    fake_faults=MOTION_FAULT_IR;press_release();assert(submitted==1 && !button.armed);
    puts("PASS: battery GP20 startup waits for RESET ack and deliberate press/release, one run, held input, STOP, manual rearm, timeout/epoch/health/fault guards");
}
#endif
int main(void)
{
    assert(!motion_line_console_key('o')); /* no fault bypass */
#if MOTION_LINE_DEMO
    trace_count=12;trace_reads=0;
    assert(motion_line_console_key('t') && history_cursor==8 && trace_reads==9);
    assert(motion_line_console_key('t') && history_cursor==12 && trace_reads==14);
    assert(motion_line_console_key('t') && history_cursor==8 && trace_reads==23);
    trace_error=E_OBJ;
    assert(motion_line_console_key('t') && history_cursor==0);
    trace_error=0;trace_count=0;
    assert(motion_line_console_key('t') && history_cursor==0 && !submitted && !stops);
    puts("PASS: read-only paged history, wrap, active/unavailable rejection, no motor/reset command");
#endif
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
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
    return 0;
}

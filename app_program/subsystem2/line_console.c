#include "motion.h"
#include "line_follow.h"
#include "line_console.h"
#include "line_button.h"
#include "line_autostart.h"
#include "../subsystem3/ir_runtime.h"

#if MOTION_LINE_DEMO
static bool line_armed;
#if MOTION_LINE_CIRCUIT
#if !MOTION_LINE_AUTOSTART
static line_button_t button;
#else
static line_auto_t autostart;
static uint32_t auto_reset_id,auto_reset_epoch;
#endif
static bool button_initialized;
static bool button_pressed(void)
{
    if(!button_initialized) {
        gpio_set_pin(MOTION_LINE_BUTTON_PIN,GPIO_MODE_IN);
        set_w(GPIO(MOTION_LINE_BUTTON_PIN),GPIO_PUE | GPIO_IE);
        clr_w(GPIO(MOTION_LINE_BUTTON_PIN),GPIO_PDE);
        button_initialized=true;
    }
    return gpio_get_val(MOTION_LINE_BUTTON_PIN)==0u;
}
#endif
static ER local_request(motion_command_t command)
{
    motion_status_t m;
    ir_runtime_status_t ir;
    motion_request_t request;
    ER error=
#if MOTION_LINE_AUTOSTART
        motion_get_ir_status(&m);
#else
        motion_get_status(&m);
#endif
    if(error<E_OK)return error;
    error=ir_runtime_get_status(&ir);
    if(error<E_OK)return error;
    uint32_t now=motion_now_ms();
    if(ir.state!=IR_RUNTIME_READY || !ir.healthy || ir.command_pending ||
       (uint32_t)(now-ir.updated_ms)>LINE_FOLLOW_INPUT_AGE_MS ||
       (uint32_t)(now-ir.raw.sampled_ms)>LINE_FOLLOW_INPUT_AGE_MS)return E_OBJ;
    if(command==MOTION_LINE_TRACE && (m.faults || m.state!=MOTION_IDLE))return E_OBJ;
    uint32_t id=m.request_id+1u;
    if(!id)id=1;
    error=motion_request_init(&request,command,MOTION_LOCAL,m.epoch,id,
                             command==MOTION_RESET ? 1000u : LINE_FOLLOW_MAX_MS,0,0,0);
    return error<E_OK ? error : motion_submit(&request);
}
#if MOTION_LINE_AUTOSTART
static void auto_cancel(void)
{
    autostart.phase=LINE_AUTO_CANCELLED;
    (void)motion_stop();
}
static void auto_poll(void)
{
    if(button_pressed()) {
        if(autostart.phase!=LINE_AUTO_CANCELLED)auto_cancel();
        return; /* GP20 remains optional STOP only, never a start */
    }
    if(autostart.phase==LINE_AUTO_CONSUMED || autostart.phase==LINE_AUTO_CANCELLED)return;
    uint32_t now=motion_now_ms();
    if(now<LINE_AUTO_PREPARE_MS ||
       (autostart.phase==LINE_AUTO_RESET_PENDING && now<LINE_AUTO_START_MS))return;
    motion_status_t m={0};ir_runtime_status_t ir={0};
    bool sampled=motion_get_ir_status(&m)==E_OK && ir_runtime_get_status(&ir)==E_OK;
    now=motion_now_ms();
    bool healthy=sampled && m.ready && m.raw_valid && (uint32_t)(now-m.updated_ms)<=LINE_FOLLOW_INPUT_AGE_MS &&
        ir.state==IR_RUNTIME_READY && ir.healthy && !ir.command_pending &&
        (uint32_t)(now-ir.updated_ms)<=LINE_FOLLOW_INPUT_AGE_MS &&
        (uint32_t)(now-ir.raw.sampled_ms)<=LINE_FOLLOW_INPUT_AGE_MS;
    bool reset_ready=healthy && (m.state==MOTION_IDLE || m.state==MOTION_ERROR) &&
        (m.faults & ~MOTION_FAULT_EXTERNAL)==0u;
    bool trace_ready=healthy && m.state==MOTION_IDLE && m.result==MOTION_COMPLETED &&
        !m.faults && m.epoch==auto_reset_epoch && m.request_id==auto_reset_id;
    line_auto_action_t action=line_auto_step(&autostart,motion_now_ms(),reset_ready,trace_ready);
    if(action==LINE_AUTO_RESET) {
        auto_reset_epoch=m.epoch+1u;auto_reset_id=m.request_id+1u;
        if(!auto_reset_id)auto_reset_id=1u;
        ER e=local_request(MOTION_RESET);
        if(e<E_OK)auto_cancel();
        tm_printf((UB*)"[line] AUTO RESET queued=%d\n",(INT)e);
    } else if(action==LINE_AUTO_TRACE) {
        ER e=local_request(MOTION_LINE_TRACE);
        if(e<E_OK)auto_cancel();
        tm_printf((UB*)"[line] AUTO TRACE 30000ms queued=%d at_ms=%u\n",(INT)e,(UINT)motion_now_ms());
    } else if(action==LINE_AUTO_ABORT) {
        auto_cancel();
        tm_printf((UB*)"[line] AUTO cancelled: startup/health/reset not ready; no retry\n");
    }
}
#endif
#endif
bool motion_line_console_key(int key)
{
#if MOTION_LINE_DEMO
    if(key=='x' || key==' ') {
#if MOTION_LINE_AUTOSTART
        autostart.phase=LINE_AUTO_CANCELLED;
#elif MOTION_LINE_CIRCUIT
        button=(line_button_t){0};
#endif
        line_armed=false;
        tm_printf((UB*)"[line] STOP result=%d\n",(INT)motion_stop());return true;
    }
#if MOTION_LINE_AUTOSTART
    if(key=='c' || key=='f' || key=='r' || key=='g') {
        auto_cancel();
        if(key=='r' || key=='g') {
            tm_printf((UB*)"[line] AUTO cancelled; r/g disabled, power-cycle for next run\n");
            return true;
        }
    }
    auto_poll();
#elif MOTION_LINE_CIRCUIT
    /* Called every diagnostic-task iteration, including when no USB input.
     * Calibration commands cancel button permission before processing edges. */
    if(key=='c' || key=='f' || key=='r') {
        bool stopping=false;
        if(button.started) {
            motion_status_t m;
            stopping=motion_get_status(&m)<E_OK || m.state==MOTION_TRACING || m.result==MOTION_ACCEPTED;
            if(stopping)(void)motion_stop();
        }
        button=(line_button_t){0};line_armed=false;
        if(stopping && key=='r') {
            tm_printf((UB*)"[line] STOP requested; wait until stopped before r re-arms\n");
            return true;
        }
    }
    line_button_event_t event=line_button_step(&button,motion_now_ms(),button_pressed());
    if(event==LINE_BUTTON_STOP) {
        line_armed=false;
        tm_printf((UB*)"[line] GP20 STOP result=%d\n",(INT)motion_stop());
    } else if(event==LINE_BUTTON_START) {
        line_armed=false;
        ER result=local_request(MOTION_LINE_TRACE);
        if(result<E_OK)button=(line_button_t){0};
        tm_printf((UB*)"[line] GP20 TRACE 30000ms queued=%d\n",(INT)result);
    }
    if(key=='g') {
        tm_printf((UB*)"[line] Circuit profile: g disabled; use GP20 after r arms one run\n");
        return true;
    }
#endif
    if(key=='r' || key=='g') {
        if(key=='g' && !line_armed) {
            tm_printf((UB*)"[line] TRACE refused: r must reset/arm before each g\n");return true;
        }
        line_armed=false; /* a held/queued g cannot replay a completed run */
        ER result=local_request(key=='r' ? MOTION_RESET : MOTION_LINE_TRACE);
        if(key=='r' && result==E_OK)line_armed=true;
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
        button=(line_button_t){0};
        if(line_armed)line_button_arm(&button,motion_now_ms(),button_pressed());
#endif
        tm_printf((UB*)"[line] %s queued=%d (0=queued; inspect i for actual state/result)\n",
                  key=='r' ? "RESET" : "TRACE 2000ms",(INT)result);
        return true;
    }
#else
    (void)key;
#endif
    return false;
}
void motion_line_console_help(void)
{
#if MOTION_LINE_DEMO
#if MOTION_LINE_AUTOSTART
    tm_printf((UB*)"[line] AUTO CIRCUIT: one 30s run after 5s boot delay, max 20%% duty. Saved IR references. GP20 or x/SPACE stops/cancels. No repeat until power-cycle.\n");
#elif MOTION_LINE_CIRCUIT
    tm_printf((UB*)"[line] CIRCUIT PROFILE: r reset/arm one GP20 press-release run; 30s maximum, 20%% duty. GP20 press or x/SPACE stops. Battery/USB disconnect allowed. No automatic start.\n");
#else
    tm_printf((UB*)"[line] LOCAL LINE PROFILE: r reset/arm after healthy IR; g one 2s trace; x/SPACE stop. Reset/arm each run. Max 20%%. No IR bypass or automatic start.\n");
#endif
#endif
}
void motion_line_console_status(void)
{
#if MOTION_LINE_DEMO
#if MOTION_LINE_AUTOSTART
    tm_printf((UB*)"[line] AUTO phase=%u boot_ms=%u GP20_pressed=%u\n",
              (UINT)autostart.phase,(UINT)motion_now_ms(),(UINT)button_pressed());
#elif MOTION_LINE_CIRCUIT
    tm_printf((UB*)"[line] GP20 pressed=%u armed=%u consumed=%u\n",
              (UINT)button_pressed(),(UINT)button.armed,(UINT)button.started);
#endif
    motion_status_t m;motion_diagnostics_t d;line_msg_t line;uint32_t sampled;
    if(motion_get_status(&m)==E_OK)tm_printf((UB*)"[line] state=%u result=%u faults=0x%x epoch=%u id=%u raw=%u L=%u R=%u duty=%d,%d metric_valid=%u\n",
        (UINT)m.state,(UINT)m.result,(UINT)m.faults,(UINT)m.epoch,(UINT)m.request_id,
        (UINT)m.raw_valid,(UINT)m.left_count,(UINT)m.right_count,
        (INT)motion_left_duty(&m),(INT)motion_right_duty(&m),(UINT)m.valid);
    if(motion_get_diagnostics(&d)==E_OK)tm_printf((UB*)"[line] active=%u step=%u err=%d speed_cps=%u,%u targets_cps=%u,%u\n",
        (UINT)d.line_active,(UINT)d.line_step,(INT)d.line_error,
        (UINT)d.line_speed_left_cps,(UINT)d.line_speed_right_cps,
        (UINT)d.line_target_left_cps,(UINT)d.line_target_right_cps);
    if(motion_debug_line(&line,&sampled)==E_OK)tm_printf((UB*)"[line] IR line_state=%u error=%d fault=%u age_ms=%u\n",
        (UINT)line.state,(INT)line.err,(UINT)line.fault,(UINT)(motion_now_ms()-sampled));
#endif
}

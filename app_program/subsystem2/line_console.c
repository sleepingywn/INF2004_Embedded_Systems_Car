#include "motion.h"
#include "line_follow.h"
#include "line_console.h"
#include "route_settings.h"
#include "line_button.h"
#include "line_autostart.h"
#if MOTION_LINE_REMOTE
#include "../subsystem1/telemetry_transport.h"
#include "../subsystem4/s4_hump_hint.h"
#endif
#include "../subsystem3/ir_runtime.h"
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
#include "line_indicator.h"
#if TM_WIFI_CYW43
#include "../../lib/libwifi/sysdepend/pico_rp2040/cyw43_utk.h"
#endif
#endif
#if TM_CONSOLE_USB_CDC
#include <tm/usb_console.h>
#endif

#if MOTION_LINE_ROUTE
/* Barcode-routing log from the motion task. Drained here (console task),
 * never printed from the motor loop. */
static uint32_t route_bc_seen;
static char route_ch(char c) { return c ? c : '?'; }
static void route_print_widths(const char *label, const bc_msg_t *m)
{
    tm_printf((UB*)"[route] %s:", (UB*)label);
    for (unsigned i = 0; i < m->hdr.count && i < BC_MAX_ELEMS; ++i)
        tm_printf((UB*)" %s%u", (UB*)((i & 1u) ? "S" : "B"), (UINT)m->widths[i]);
    tm_printf((UB*)"\n");
}
/* Read on demand, in pages, so printing does not run in the motor task. */
static void pivot_console_page(void)
{
    static uint32_t previous_turn;
    static unsigned next;
    motion_pivot_sample_t row;
    uint32_t turn;
    unsigned count;
    if (!motion_pivot_sample(0, &row, &turn, &count)) {
        tm_printf((UB*)"[pivot] no capture available\n");
        return;
    }
    if (turn != previous_turn || next >= count) next = 0;
    previous_turn = turn;
    unsigned end = next + 12u;
    if (end > count) end = count;
    tm_printf((UB*)"[pivot] turn=%u rows=%u first=%u end=%u\n",
              (UINT)turn, (UINT)count, (UINT)next, (UINT)end);
    for (; next < end; ++next) {
        if (!motion_pivot_sample(next, &row, &turn, &count) || turn != previous_turn) break;
        int left = line_levels_valid(row.levels) ? (int)line_level_left(row.levels) : -1;
        int right = line_levels_valid(row.levels) ? (int)line_level_right(row.levels) : -1;
        tm_printf((UB*)"[pivot] ms=%u IR=%d,%d counts=%d,%d state=%u edge=%u settle=%u reason=%u\n",
                  (UINT)row.elapsed_ms, (INT)left, (INT)right,
                  (INT)row.left_counts, (INT)row.right_counts, (UINT)row.state,
                  (UINT)row.edge, (UINT)row.settled_phase, (UINT)row.reason);
    }
}
static void route_console_drain(void)
{
    motion_route_event_t e;
    for (unsigned n = 0; n < 8u && motion_route_next_event(&e); ++n) {
        switch (e.kind) {
        case ROUTE_EV_BARCODE: {
            tm_printf((UB*)"[route] SCAN elements=%d decode: distance=%c time=%c -> %c%s\n",
                      (INT)e.v[0], (INT)route_ch((char)e.v[1]), (INT)route_ch((char)e.v[2]),
                      (INT)route_ch(e.code), (UB*)(e.v[3] ? "" : " (not during a V5 run: ignored)"));
            motion_route_barcode_t bc;
            if (motion_route_barcode(&bc, &route_bc_seen)) {
                route_print_widths("widths_ms", &bc.by_time);
                if (bc.distance_valid) route_print_widths("widths_counts", &bc.by_distance);
                else tm_printf((UB*)"[route] widths_counts: unavailable (odometry history)\n");
                tm_printf((UB*)"[route] old rank-of-9 decoder: %c\n", (INT)route_ch(bc.old_code));
                if (bc.one_marker)
                    tm_printf((UB*)"[route] decoded with ONE valid end marker (time and distance agree)\n");
                {   /* raw ADC: darkest of each bar, lightest of each gap */
                    uint16_t lv[BC_MAX_ELEMS];
                    unsigned n = ir_runtime_barcode_levels(lv, BC_MAX_ELEMS, NULL);
                    tm_printf((UB*)"[route] levels_raw:");
                    for (unsigned i = 0; i < n; ++i)
                        tm_printf((UB*)" %s%u", (UB*)((i & 1u) ? "S" : "B"), (UINT)lv[i]);
                    tm_printf((UB*)"\n");
                }
            }
            break;
        }
        case ROUTE_EV_ARMED:
            tm_printf((UB*)"[route] ROUTE: %c armed for the next junction (%s)\n", (INT)e.code,
                      (UB*)(e.code == 'A' ? "right" : (e.code == 'B' || e.code == 'Z') ? "left" :
                            e.code == 'C' ? "straight" : "U-turn"));
            break;
        case ROUTE_EV_NOT_ARMED:
            tm_printf((UB*)"[route] ROUTE: %c not armed (%s)\n", (INT)e.code,
                      (UB*)(e.v[0] == 1 ? "a turn is in progress" :
                            e.v[0] == 3 ? "junction already armed" : "letter has no manoeuvre"));
            break;
        case ROUTE_EV_JUNCTION:
            tm_printf((UB*)"[route] JUNCTION: %c -> %s (A=%d B=%d)\n", (INT)e.code,
                      (UB*)(e.code == 'A' ? "right arc" : (e.code == 'B' || e.code == 'Z') ? "left arc" :
                            e.code == 'C' ? "straight 100 mm" : "180 deg pivot"),
                      (INT)e.v[0], (INT)e.v[1]);
            break;
        case ROUTE_EV_EXIT:
            tm_printf((UB*)"[route] %s EXIT: line acquired; V5 line following (A=%d B=%d err=%d)\n",
                      (UB*)(e.v[3] ? "RIGHT" : "LEFT"), (INT)e.v[0], (INT)e.v[1], (INT)e.v[2]);
            break;
        case ROUTE_EV_LEG_DONE:
            tm_printf((UB*)"[route] %c leg complete (L=%d R=%d counts)\n", (INT)e.code,
                      (INT)e.v[0], (INT)e.v[1]);
            break;
        case ROUTE_EV_RESUMED:
            tm_printf((UB*)"[route] V5 line following resumed after %c\n", (INT)e.code);
            break;
        case ROUTE_EV_FAILED:
            tm_printf((UB*)"[route] FAILED during %c: reason=%d (16 exit not found, 13 leg timeout, "
                      "4 stall, 3 encoder/control, 18 line lost or stale, 19 U-turn line not acquired) phase=%d\n",
                      (INT)route_ch(e.code), (INT)e.v[0], (INT)e.v[1]);
            break;
        case ROUTE_EV_TIME_LIMIT:
            tm_printf((UB*)"[route] RUN ENDED: 30 s limit since the last start/turn (phase=%d)\n",
                      (INT)e.v[0]);
            break;
        default: break;
        }
    }
}
#endif
#if MOTION_LINE_DEMO
static bool line_armed;
/* Explicit, paged readout after a run; never flood USB from the motor loop. */
static unsigned history_cursor;
static uint32_t history_epoch,history_id;
static void line_history_page(void)
{
    motion_line_trace_entry_t entry;
    ER e=motion_get_line_trace(0,&entry);
    if(e<E_OK) {
        history_cursor=0;
        tm_printf((UB*)"[trace] unavailable=%d (stopped run required)\n",(INT)e);return;
    }
    if(entry.epoch!=history_epoch || entry.id!=history_id || history_cursor>=entry.count)
        history_cursor=0;
    history_epoch=entry.epoch;history_id=entry.id;
    unsigned end=history_cursor+8u;
    if(end>entry.count)end=entry.count;
    tm_printf((UB*)"[trace] epoch=%u id=%u count=%u first=%u end=%u\n",
        (UINT)history_epoch,(UINT)history_id,(UINT)entry.count,(UINT)history_cursor,(UINT)end);
    for(;history_cursor<end;++history_cursor) {
        e=motion_get_line_trace(history_cursor,&entry);
        if(e<E_OK || entry.epoch!=history_epoch || entry.id!=history_id) {
            history_cursor=0;tm_printf((UB*)"[trace] interrupted; retry t\n");return;
        }
        motion_line_trace_sample_t *s=&entry.sample;
        tm_printf((UB*)"[trace] n=%u ms=%u state=%u err=%d steer=%d step=%u age=%u flags=%u target=%u,%u speed=%d,%d duty=%d,%d tight=%d search=%u levels=%u adaptive=%u reason=%u\n",
            (UINT)history_cursor,(UINT)s->elapsed_ms,(UINT)s->state,(INT)s->error,(INT)s->steering,
            (UINT)s->step,(UINT)s->age_ms,(UINT)s->flags,(UINT)s->target_left_cps,(UINT)s->target_right_cps,
            (INT)s->speed_left_cps,(INT)s->speed_right_cps,(INT)s->duty_left,(INT)s->duty_right,(INT)s->tight_direction,(UINT)s->search_phase,(UINT)s->levels,(UINT)s->adaptive_mode,(UINT)s->stop_reason);
    }
}
#if TM_CONSOLE_USB_CDC
static bool boot_pending;
static uint32_t boot_requested_ms,boot_previous_epoch;
#endif
#if MOTION_LINE_CIRCUIT
#if !MOTION_LINE_AUTOSTART
static line_button_t button;
static line_indicator_t indicator;
static bool button_boot_pending=true,button_reset_pending;
static bool button_retry_allowed,button_retry_gesture,button_restart_pending,button_retry_waiting;
static uint32_t button_retry_since;
static uint32_t button_reset_ms,button_reset_id,button_reset_epoch;
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
#if MOTION_LINE_REMOTE
static motion_request_t last_local_request;
static bool remote_submitting;
#endif
static ER local_request(motion_command_t command)
{
    motion_status_t m;
    ir_runtime_status_t ir;
    motion_request_t request;
    ER error=
#if MOTION_LINE_CIRCUIT
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
    motion_source_t source=MOTION_LOCAL;
#if MOTION_LINE_REMOTE
    if(remote_submitting && command==MOTION_LINE_TRACE)source=MOTION_NETWORK;
#endif
    error=motion_request_init(&request,command,source,m.epoch,id,
                             command==MOTION_RESET ? 1000u : LINE_FOLLOW_MAX_MS,0,0,0);
#if MOTION_LINE_REMOTE
    if(error==E_OK && remote_submitting && command==MOTION_LINE_TRACE)
        error=motion_remote_watch(request.epoch,request.id,now,now);
#endif
    ER submitted=error<E_OK ? error : motion_submit(&request);
#if MOTION_LINE_REMOTE
    if(submitted==E_OK)last_local_request=request;
#endif
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
    if(submitted==E_OK && command==MOTION_LINE_TRACE)
        {line_indicator_queue(&indicator,request.epoch,request.id,now);button_retry_allowed=true;}
#endif
    return submitted;
}
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
/* Startup may prepare permission, but only a later press/release can drive.
 * Wait for the owner's exact RESET acknowledgement before accepting a press. */
static ER button_queue_reset(void)
{
    motion_status_t m;
    ER e=motion_get_ir_status(&m);
    if(e<E_OK)return e;
    if(button_restart_pending &&
       ((m.state!=MOTION_IDLE && m.state!=MOTION_ERROR) ||
        motion_left_duty(&m) || motion_right_duty(&m) ||
        (uint32_t)(motion_now_ms()-m.updated_ms)>LINE_FOLLOW_INPUT_AGE_MS ||
        (m.faults & ~(MOTION_FAULT_IR|MOTION_FAULT_TIMEOUT|MOTION_FAULT_RANGE))))return E_OBJ;
    e=local_request(MOTION_RESET);
    if(e==E_OK) {
        button_reset_epoch=m.epoch+1u;
        button_reset_id=m.request_id+1u;
        if(!button_reset_id)button_reset_id=1u;
        button_reset_ms=motion_now_ms();button_reset_pending=true;
    }
    return e;
}
static void button_prepare(void)
{
    motion_status_t m;ir_runtime_status_t ir;
    if(!button_boot_pending && !button_reset_pending)return;
    uint32_t now=motion_now_ms();
    if(button_reset_pending && (uint32_t)(now-button_reset_ms)>=1000u) {
        button_reset_pending=false;button_restart_pending=false;line_indicator_fail(&indicator);return;
    }
    if(motion_get_ir_status(&m)<E_OK || ir_runtime_get_status(&ir)<E_OK)return;
    now=motion_now_ms();
    bool healthy=m.ready && m.raw_valid && (uint32_t)(now-m.updated_ms)<=LINE_FOLLOW_INPUT_AGE_MS &&
        ir.state==IR_RUNTIME_READY && ir.healthy && !ir.command_pending &&
        (uint32_t)(now-ir.updated_ms)<=LINE_FOLLOW_INPUT_AGE_MS &&
        (uint32_t)(now-ir.raw.sampled_ms)<=LINE_FOLLOW_INPUT_AGE_MS;
    if(button_boot_pending) {
        if(!healthy)return;
        button_boot_pending=false;
        /* Only the startup external stop may be cleared without a USB reset. */
        if((m.state!=MOTION_IDLE && m.state!=MOTION_ERROR) ||
           (m.faults & ~MOTION_FAULT_EXTERNAL)){line_indicator_fail(&indicator);return;}
        if(button_queue_reset()<E_OK)line_indicator_fail(&indicator);
    } else if(healthy && m.state==MOTION_IDLE && !m.faults &&
              m.result==MOTION_COMPLETED && m.epoch==button_reset_epoch &&
              m.request_id==button_reset_id) {
        button_reset_pending=false;
        if(button_restart_pending) {
            button_restart_pending=false;button_retry_gesture=false;
            ER result=local_request(MOTION_LINE_TRACE);
            button=(line_button_t){.started=result==E_OK};line_armed=false;
            if(result<E_OK)line_indicator_fail(&indicator);
            tm_printf((UB*)"[line] GP20 RESTART queued=%d\n",(INT)result);
            return;
        }
        line_armed=true;
        line_button_arm(&button,now,button_pressed());
        tm_printf((UB*)"[line] GP20 ready: press and release for a line-follow run\n");
    }
}
#endif
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
static bool line_console_key(int key)
{
#if MOTION_LINE_DEMO
#if TM_CONSOLE_USB_CDC
    if(key=='x' || key==' ')boot_pending=false;
    if(key=='b' && !boot_pending) {
        motion_status_t m;
        ER sampled=motion_get_ir_status(&m);
        /* Reuse STOP to cancel every form of button/automatic run permission. */
        (void)motion_line_console_key('x');
        if(sampled==E_OK) {
            boot_previous_epoch=m.epoch;boot_requested_ms=motion_now_ms();
            boot_pending=true;
            tm_printf((UB*)"[line] BOOTSEL requested; waiting for motor STOP acknowledgement\n");
        } else tm_printf((UB*)"[line] BOOTSEL refused: motion snapshot unavailable; stopped/disarmed\n");
        return true;
    }
    if(boot_pending) {
        motion_status_t m;
        uint32_t now=motion_now_ms();
        if((uint32_t)(now-boot_requested_ms)>=1000u) {
            boot_pending=false;
            tm_printf((UB*)"[line] BOOTSEL refused: STOP acknowledgement timeout; remains disarmed\n");
        } else if(motion_get_ir_status(&m)==E_OK) {
            now=motion_now_ms();
            if((uint32_t)(now-boot_requested_ms)<1000u &&
               (uint32_t)(now-m.updated_ms)<=LINE_FOLLOW_INPUT_AGE_MS &&
               /* STOP advances the epoch even while already idle. In that
                * case end_request keeps the prior result. A concurrent
                * health fault may advance it again; both paths stop outputs. */
               m.epoch!=boot_previous_epoch &&
               (uint32_t)(m.updated_ms-boot_requested_ms)<1000u &&
               (m.state==MOTION_IDLE || m.state==MOTION_ERROR) &&
               !motion_left_duty(&m) && !motion_right_duty(&m)) {
                boot_pending=false;
                (void)tm_usb_enter_bootsel();
                tm_printf((UB*)"[line] BOOTSEL ROM entry failed; remains disarmed\n");
            }
        }
        return true; /* no start/reset/calibration processing while stopping */
    }
#endif
    if(key=='x' || key==' ') {
#if MOTION_LINE_AUTOSTART
        autostart.phase=LINE_AUTO_CANCELLED;
#elif MOTION_LINE_CIRCUIT
        button=(line_button_t){0};
        line_indicator_clear(&indicator);
        button_boot_pending=button_reset_pending=false;
        button_restart_pending=false;button_retry_gesture=false;button_retry_waiting=false;
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
        button_retry_allowed=false;
        line_indicator_clear(&indicator);
        button_boot_pending=button_reset_pending=false;
        button_restart_pending=false;button_retry_gesture=false;button_retry_waiting=false;
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
    if(button_restart_pending && button_pressed()) {
        button_restart_pending=button_reset_pending=false;
        button=(line_button_t){0};line_armed=false;
        (void)motion_stop();return true;
    }
    button_prepare();
    line_button_event_t event=line_button_step(&button,motion_now_ms(),button_pressed());
    if(event==LINE_BUTTON_STOP) {
        line_indicator_clear(&indicator);
        line_armed=false;
        tm_printf((UB*)"[line] GP20 STOP result=%d\n",(INT)motion_stop());
    } else if(event==LINE_BUTTON_START) {
        line_armed=false;
        ER result;
        if(button_retry_gesture) {
            button_retry_gesture=false;button=(line_button_t){0};
            button_restart_pending=true;result=button_queue_reset();button_restart_pending=result==E_OK;
        } else result=local_request(MOTION_LINE_TRACE);
        if(result<E_OK){button=(line_button_t){0};line_indicator_fail(&indicator);}
        tm_printf((UB*)"[line] GP20 TRACE queued=%d\n",(INT)result);
    }
    if(key=='g') {
        tm_printf((UB*)"[line] Circuit profile: g disabled; use GP20 when ready\n");
        return true;
    }
#endif
#if MOTION_LINE_ROUTE
    if(key=='u') {pivot_console_page();return true;}
#endif
    if(key=='t') {line_history_page();return true;}
    if(key=='r' || key=='g') {
        if(key=='g' && !line_armed) {
            tm_printf((UB*)"[line] TRACE refused: r must reset/arm before each g\n");return true;
        }
        line_armed=false; /* a held/queued g cannot replay a completed run */
        ER result;
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
        result=button_queue_reset(); /* g returned above; r awaits acknowledgement */
        if(result<E_OK)line_indicator_fail(&indicator);
#else
        result=local_request(key=='r' ? MOTION_RESET : MOTION_LINE_TRACE);
        if(key=='r' && result==E_OK)line_armed=true;
#endif
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
        button=(line_button_t){0};
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
#if MOTION_LINE_REMOTE
/* Only the diagnostic/GP20 owner calls this. WiFi only stages bounded frames.
 * Lease enforcement ALSO runs in the motor owner; a blocked console cannot
 * keep a remote run alive. No remote reset or fault-clear is provided. */
static struct {
    uint32_t session,last_id,sequence,owner,epoch,request_id,started,renewed,keep_issued;
    uint32_t ack_id,pending_ms,stop_epoch;
    uint8_t ack,op,stage,stop_result;
} remote;
static bool mapping_pending;
/* The console owns writes. Each barcode lookup reads exactly one atomic slot. */
static bool mapping_run_state(unsigned state)
{
    return state == MOTION_TRACING || state == MOTION_REACQUIRE;
}
static void remote_stop(uint8_t result,uint32_t now,uint32_t epoch)
{
    (void)line_console_key('x');
    remote.stage=3;remote.pending_ms=now;remote.stop_epoch=epoch;
    remote.stop_result=result;remote.ack=CONTROL_QUEUED;
}
static void remote_poll(void)
{
    uint32_t boot,session;int connected;
    if(!s1_control_context(&boot,&session,&connected))return;
    motion_status_t m={0};uint32_t now=motion_now_ms();
    bool fresh=motion_get_ir_status(&m)==E_OK;
    now=motion_now_ms();fresh=fresh && (uint32_t)(now-m.updated_ms)<=200u;
    if(remote.session!=session) {
        if(remote.stage==1 || remote.stage==2)remote_stop(CONTROL_LINK_LOST,now,remote.epoch);
        remote.session=session;remote.last_id=0;
    }
    if(remote.stage==1 || remote.stage==2) {
        if(!connected || control_expired(now,remote.renewed,remote.started))
            remote_stop(CONTROL_LINK_LOST,now,remote.epoch);
        else if(!button.started || (fresh && m.epoch!=remote.epoch)) {
            remote_stop(CONTROL_FINISHED,now,remote.epoch);
        } else if(fresh && m.request_epoch==remote.epoch && m.request_id==remote.request_id) {
            if(m.state==MOTION_TRACING && m.result==MOTION_ACCEPTED) {
                remote.stage=2;if(remote.ack_id==remote.owner)remote.ack=CONTROL_RUNNING;
            } else if(m.result!=MOTION_ACCEPTED) {
                remote_stop(m.result==MOTION_COMPLETED ? CONTROL_FINISHED : CONTROL_REJECTED,now,m.epoch);
            }
        }
        if(remote.stage==1 && (uint32_t)(now-remote.pending_ms)>=1000u)
            remote_stop(CONTROL_TIMED_OUT,now,remote.epoch);
    }
    if(remote.stage==3) {
        if(fresh && (int32_t)(m.updated_ms-remote.pending_ms)>=0 && m.epoch!=remote.stop_epoch &&
           (m.state==MOTION_IDLE || m.state==MOTION_ERROR) &&
           !motion_left_duty(&m) && !motion_right_duty(&m)) {
            remote.stage=0;remote.owner=0;remote.ack=remote.stop_result;
            if (mapping_pending) {
                uint8_t map[ROUTE_MAP_COUNT];
                route_mapping_export(map);
                mapping_pending=false;
                route_settings_reboot(map);
            }
        } else if((uint32_t)(now-remote.pending_ms)>=1000u) {
            /* Retain the stop guard and retry; do not claim a stopped motor. */
            remote.ack=CONTROL_TIMED_OUT;(void)motion_stop();
        }
    }
    /* Persist after the motion owner reports a terminal state, never during
     * a turn or temporary zero-duty pause. STOP acknowledgement fences reboot. */
    if(mapping_pending && remote.stage==0 && fresh &&
       (m.state==MOTION_IDLE || m.state==MOTION_ERROR) &&
       !motion_left_duty(&m) && !motion_right_duty(&m))
        remote_stop(CONTROL_STOPPED,now,m.epoch);
    bool can_start=s4_drive_ready() && !mapping_pending && !button_retry_gesture && !button_restart_pending && connected && boot && fresh && m.ready && m.raw_valid && !m.faults &&
        m.state==MOTION_IDLE && button.armed && button.ready && !button.pressed_once &&
        !button_pressed() && !button_boot_pending && !button_reset_pending && !boot_pending && remote.stage==0;
    control_command_t c;
    if(s1_control_take(&c,now,remote.last_id,m.epoch,fresh) && connected &&
       control_fresh(&c,boot,session,c.op==CONTROL_STOP && !fresh ? c.epoch : m.epoch,now) &&
       (c.op==CONTROL_STOP || fresh)) {
        if(c.op==CONTROL_KEEP) {
            if((remote.stage==1 || remote.stage==2) && c.id==remote.owner &&
               control_newer(c.issued,remote.keep_issued)) {
                if(motion_remote_watch(remote.epoch,remote.request_id,now,remote.started)==E_OK) {
                    remote.renewed=now;remote.keep_issued=c.issued;
                }
            }
        } else if(control_newer(c.id,remote.last_id)) {
            remote.last_id=c.id;remote.ack_id=c.id;remote.op=c.op;
            if(c.op==CONTROL_STOP){remote_stop(CONTROL_STOPPED,now,fresh ? m.epoch : c.epoch);}
            else if(c.op==CONTROL_MAPPING) {
                bool moving=mapping_run_state(m.state) && (remote.stage==0 || remote.stage==2);
                bool idle=m.state==MOTION_IDLE && remote.stage==0 && !button.started &&
                          !motion_left_duty(&m) && !motion_right_duty(&m);
                if(fresh && !m.faults && (moving || (idle && m.ready)) &&
                   !button_pressed() && !button_reset_pending && !button_boot_pending && !boot_pending &&
                   c.revision==route_mapping_revision && route_actions_valid(&c.mapping)) {
                    route_mapping_apply(&c.mapping);
                    ++route_mapping_revision;
                    route_mapping_persisted=false;
                    mapping_pending=true;
                    remote.ack=CONTROL_QUEUED;
                    if(idle)remote_stop(CONTROL_STOPPED,now,m.epoch);
                } else remote.ack=CONTROL_REJECTED;
            }
            else if(can_start) {
                remote_submitting=true;ER e=local_request(MOTION_LINE_TRACE);remote_submitting=false;
                /* Every accepted Start consumes the same one-shot permission as GP20. */
                if(e==E_OK) {
                    button=(line_button_t){.started=true};line_armed=false;
                    remote.owner=c.id;remote.epoch=last_local_request.epoch;remote.request_id=last_local_request.id;
                    remote.started=remote.renewed=remote.pending_ms=now;remote.keep_issued=c.issued;
                    remote.stage=1;remote.ack=CONTROL_QUEUED;
                } else remote.ack=CONTROL_REJECTED;
            } else remote.ack=CONTROL_REJECTED;
        }
    }
    uint8_t b[CONTROL_STATUS_SIZE]={ 'R','S',2,CONTROL_SUPPORTED | CONTROL_LIVE_MAPPING };
    if(can_start && remote.stage==0)b[3]|=CONTROL_ARMED;
    if(remote.stage==1 || remote.stage==2)b[3]|=CONTROL_ACTIVE;
    if(fresh)b[3]|=CONTROL_MOTION_FRESH;
    if(connected)b[3]|=CONTROL_CONNECTED;
    control_put(b+4,boot);control_put(b+8,session);control_put(b+12,++remote.sequence);
    control_put(b+16,now);control_put(b+20,m.epoch);control_put(b+24,remote.ack_id);
    b[28]=remote.ack;b[29]=(uint8_t)m.state;b[30]=(uint8_t)m.faults;b[31]=remote.op;
    control_put(b+32,remote.owner);control_put(b+36,route_mapping_revision);
    route_mapping_export(b + 40);
    b[66]=route_mapping_persisted ? 1 : 0;
    b[67]=mapping_pending ? 1 : 0;
    (void)s1_control_status_offer(b);
}
#endif
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
/* Prepare only a new physical gesture. Never reset/drive automatically, and
 * retain stopped-run history until that gesture explicitly requests RESET. */
static void button_retry_poll(void)
{
    motion_status_t m;ir_runtime_status_t ir;
    bool blocked=!button_retry_allowed || button.armed || button_boot_pending ||
        button_reset_pending || button_restart_pending;
#if TM_CONSOLE_USB_CDC
    blocked=blocked || boot_pending;
#endif
#if MOTION_LINE_REMOTE
    blocked=blocked || remote.stage!=0 || mapping_pending;
#endif
    if(blocked){button_retry_waiting=false;return;}
    if(motion_get_ir_status(&m)<E_OK || ir_runtime_get_status(&ir)<E_OK){button_retry_waiting=false;return;}
    uint32_t now=motion_now_ms();
    const unsigned recoverable=MOTION_FAULT_IR|MOTION_FAULT_TIMEOUT|MOTION_FAULT_RANGE;
    bool stopped=(m.state==MOTION_IDLE || m.state==MOTION_ERROR) && !motion_left_duty(&m) && !motion_right_duty(&m);
    bool healthy=m.ready && m.raw_valid && !(m.faults & ~recoverable) &&
        (uint32_t)(now-m.updated_ms)<=LINE_FOLLOW_INPUT_AGE_MS &&
        ir.state==IR_RUNTIME_READY && ir.healthy && !ir.command_pending &&
        (uint32_t)(now-ir.updated_ms)<=LINE_FOLLOW_INPUT_AGE_MS &&
        (uint32_t)(now-ir.raw.sampled_ms)<=LINE_FOLLOW_INPUT_AGE_MS;
    /* A queued command is not a completed run. Wait for its result. */
    bool queued=indicator.tracking && m.epoch==indicator.epoch &&
        (m.request_epoch!=indicator.epoch || m.request_id!=indicator.id);
    if(!stopped || !healthy || queued){button_retry_waiting=false;return;}
    if(!button_retry_waiting){button_retry_waiting=true;button_retry_since=now;return;}
    if((uint32_t)(now-button_retry_since)<600u)return;
    line_indicator_clear(&indicator);
    line_button_arm(&button,now,button_pressed());
    button_retry_gesture=true;button_retry_waiting=false;line_armed=false;
    tm_printf((UB*)"[line] GP20 retry ready: release, then press/release for another run; previous history retained until retry.\n");
}
#endif
bool motion_line_console_key(int key)
{
#if MOTION_LINE_ROUTE
    route_console_drain();
#endif
    bool handled=line_console_key(key);
#if MOTION_LINE_REMOTE
    remote_poll();
#endif
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_AUTOSTART
    button_retry_poll();
    motion_status_t m;
    bool sampled=motion_get_status(&m)==E_OK;
    uint32_t now=motion_now_ms();
    line_indicator_observe(&indicator,sampled ? &m : NULL,button.armed,now);
#if TM_WIFI_CYW43
    cyw43_utk_set_led(line_indicator_level(indicator.mode,now));
#endif
#endif
    return handled;
}
void motion_line_console_help(void)
{
#if MOTION_LINE_REMOTE
    tm_printf((UB*)"[line] REMOTE v1: armed Start/Stop, %ums connection lease; GP20 stops. No remote reset.\n", (UINT)CONTROL_LEASE_MS);
#endif
#if MOTION_LINE_DEMO
#if MOTION_LINE_ADAPTIVE
    tm_printf((UB*)"[line] ADAPTIVE v5: v2 steering baseline, bounded straight crossing, rollback-tolerant wheel feedback; no tight-curve deadline. GP20 Start/Stop.\n");
    tm_printf((UB*)"[line] adaptive modes: 0 track, 1 uncertain, 2 search, 3 mark, 4 route required, 5 recovery exhausted, 6 invalid, 7 crossing, 8 crossing exhausted.\n");
    tm_printf((UB*)"[line] reason: 0 none, 1 recovery time, 2 recovery distance, 3 sweep exhausted, 4 mark time, 5 mark distance, 6 crossing time, 7 crossing distance, 8 route command, 9 invalid pair.\n");
#if MOTION_LINE_ROUTE
#if IR_BARCODE_DIGITAL
    tm_printf((UB*)"[barcode] DIGITAL D0 -> GP26, black level=%u, 1 kHz; GP27/28 analogue.\n", (UINT)IR_BARCODE_BLACK_LEVEL);
#else
    tm_printf((UB*)"[barcode] ANALOGUE A0 -> GP26, ADC thresholds; GP27/28 analogue.\n");
#endif
    tm_printf((UB*)"[line] ROUTE profile: barcode arms the next junction: A right, B/Z left, C straight 100 mm, D U-turn. Unarmed junctions: V5 crosses straight.\n");
#else
    tm_printf((UB*)"[line] Crossing policy: try straight within 400 mark / 900 total counts, 1800 / 3500ms; reacquire or stop. Barcode A/B/D stops for route handling (step 15).\n");
#endif
#endif
    tm_printf((UB*)"[line] t read next 8 stopped-run history samples; repeats after final page. RAM only; reset/new run clears.\n");
#if MOTION_LINE_SEARCH
    tm_printf((UB*)"[line] SEARCH PROFILE: ambiguous line permits a bounded 1500ms/1000-count sweep, then STOP unless reacquired.\n");
    tm_printf((UB*)"[line] CURVE PROFILE: 450 cruise, 250/600 full curve at error 60; recent bend holds 250/500 for up to 300ms, otherwise 600ms straight grace.\n");
    tm_printf((UB*)"[line] TIGHT PROFILE: arm at error 55, confirm same side >=45 for 120ms; bounded inside-wheel pause, outside 500.\n");
    tm_printf((UB*)"[line] OVERLAP PROFILE: recent curve only, both-black confirmation <=120ms/80 counts, ramp toward targets <=350; persistent candidate stops. No both-black start.\n");
    tm_printf((UB*)"[line] STARTUP PROFILE: both wheels seed at 8%% duty with matched integral; normal slew and stall checks apply.\n");
#endif
#if TM_CONSOLE_USB_CDC
    tm_printf((UB*)"[line] b stop/disarm then enter USB BOOTSEL for flashing (no button needed)\n");
#endif
#if MOTION_LINE_AUTOSTART
    tm_printf((UB*)"[line] AUTO CIRCUIT: a line-follow run after 5s boot delay, max 20%% duty. Saved IR references. GP20 or x/SPACE stops/cancels. No repeat until power-cycle.\n");
#elif MOTION_LINE_CIRCUIT
#if MOTION_LINE_ROUTE
    tm_printf((UB*)"[line] u pages through the latest U-turn: calibrated IR and relative wheel counts L,R\n");
#endif
    tm_printf((UB*)"[line] Pico W green LED: slow blink=armed, double pulse=queued, solid=run accepted, fast blink=rejected/fault, off=finished/disarmed.\n");
#if MOTION_LINE_UNLIMITED
    tm_printf((UB*)"[line] RUN TIME: no automatic time cutoff; Stop, sensor/encoder faults, distance guards and remote heartbeat still apply.\n");
#else
    tm_printf((UB*)"[line] RUN TIME: 30s segment limit; remote total limit 30s.\n");
#endif
    tm_printf((UB*)"[line] CIRCUIT PROFILE: saved IR references; waits for GP20 press-release after healthy startup. 20%% duty. GP20 press or x/SPACE stops. Battery/USB disconnect allowed. After stopping, wait for slow blink then a NEW GP20 press-release to retry; no automatic movement.\n");
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
    tm_printf((UB*)"[line] LED mode=%u (0=off 1=armed 2=pending 3=running 4=error)\n",(UINT)indicator.mode);
    tm_printf((UB*)"[line] GP20 pressed=%u armed=%u consumed=%u reset_pending=%u\n",
              (UINT)button_pressed(),(UINT)button.armed,(UINT)button.started,(UINT)button_reset_pending);
#endif
    motion_status_t m;motion_diagnostics_t d;line_msg_t line;uint32_t sampled;
    if(motion_get_status(&m)==E_OK)tm_printf((UB*)"[line] state=%u result=%u faults=0x%x epoch=%u id=%u raw=%u L=%u R=%u duty=%d,%d metric_valid=%u\n",
        (UINT)m.state,(UINT)m.result,(UINT)m.faults,(UINT)m.epoch,(UINT)m.request_id,
        (UINT)m.raw_valid,(UINT)m.left_count,(UINT)m.right_count,
        (INT)motion_left_duty(&m),(INT)motion_right_duty(&m),(UINT)m.valid);
    if(motion_get_diagnostics(&d)==E_OK) {
        tm_printf((UB*)"[line] active=%u step=%u err=%d speed_cps=%u,%u targets_cps=%u,%u steer=%d tight=%d search=%u levels=%u adaptive=%u reason=%u\n",
            (UINT)d.line_active,(UINT)d.line_step,(INT)d.line_error,
            (UINT)d.line_speed_left_cps,(UINT)d.line_speed_right_cps,
            (UINT)d.line_target_left_cps,(UINT)d.line_target_right_cps,(INT)d.line_steering_error,(INT)d.line_tight_direction,(UINT)d.line_search_phase,(UINT)d.line_levels,(UINT)d.line_adaptive_mode,(UINT)d.line_stop_reason);
        if(d.line_failure_flags&1u)
            tm_printf((UB*)"[line] failure state=%u error=%d age_ms=%u elapsed_ms=%u flags=0x%x\n",
                (UINT)d.line_failure_state,(INT)d.line_failure_error,
                (UINT)d.line_failure_age_ms,(UINT)d.line_failure_elapsed_ms,(UINT)d.line_failure_flags);
        if(d.encoder_failure.reason) {
            motion_encoder_failure_t *e=&d.encoder_failure;
            tm_printf((UB*)"[line] encoder reason=0x%x speed_reason=0x%x raw_valid=%u invalid=%u,%u dt_ms=%u\n",
                (UINT)e->reason,(UINT)e->speed_reason,(UINT)e->raw_valid,
                (UINT)e->initial_invalid,(UINT)e->invalid,(UINT)e->dt_ms);
            tm_printf((UB*)"[line] encoder raw=%u,%u previous=%u,%u origin=%u,%u\n",
                (UINT)e->raw_left,(UINT)e->raw_right,(UINT)e->previous_left,(UINT)e->previous_right,
                (UINT)e->origin_left,(UINT)e->origin_right);
            tm_printf((UB*)"[line] encoder speed_previous=%d,%d target=%d,%d\n",
                (INT)e->speed_previous_left,(INT)e->speed_previous_right,(INT)e->target_left,(INT)e->target_right);
            tm_printf((UB*)"[line] encoder pause eligible=0x%x used=%u,%u budget=%u (0=unlimited) delta=%d,%d prior_target=%d,%d prior_duty=%u,%u\n",
                (UINT)e->pause_eligible,(UINT)e->pause_used_left,(UINT)e->pause_used_right,(UINT)e->pause_budget,
                (INT)e->delta_left,(INT)e->delta_right,(INT)e->prior_target_left,(INT)e->prior_target_right,
                (UINT)e->prior_duty_left,(UINT)e->prior_duty_right);
            tm_printf((UB*)"[line] encoder pause_age_ms=%u,%u last_settle_age_ms=%u,%u (4294967295=not applicable)\n",
                (UINT)e->pause_age_left_ms,(UINT)e->pause_age_right_ms,(UINT)e->settle_age_left_ms,(UINT)e->settle_age_right_ms);
        }
    }
    if(motion_debug_line(&line,&sampled)==E_OK)tm_printf((UB*)"[line] IR line_state=%u error=%d fault=%u age_ms=%u\n",
        (UINT)line.state,(INT)line.err,(UINT)line.fault,(UINT)(motion_now_ms()-sampled));
#endif
}

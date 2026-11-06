/* Standalone scan bench and junction turn trial. */
#ifndef SCAN_DRIVE_H
#define SCAN_DRIVE_H
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "../../../app_program/subsystem2/motion_config.h"
#include "../../../app_program/subsystem2/line_button.h"
#include "../../../app_program/subsystem2/forward_speed.h"
#include "../../../app_program/subsystem2/motion_logic.h"
#define SCAN_RUN_MS 8000u
#define SCAN_FORWARD_TARGET_CPS 450
#define SCAN_REVERSE_TARGET_CPS 450
#define SCAN_DEMO_DELAY_US 2000000u
#define SCAN_JUNCTION_CONFIRM_US 20000u
#define SCAN_EXIT_CONFIRM_US 10000u
#define SCAN_EXIT_MIN_DARKNESS 35u
#define SCAN_EXIT_MIN_CONTRAST 10
#define SCAN_LINE_GAIN_Q10 3072u
#define SCAN_STRAIGHT_DISTANCE_UM 100000u
#define SCAN_TURN_SETTLE_US 300000u
#define SCAN_TURN_TIMEOUT_MS 6000u
#define SCAN_ARC_OUTER_CPS 750
#define SCAN_ARC_INNER_CPS 200
/* Accept the turn-side sensor after about 5 degrees, then either side after 10. */
#define SCAN_ARC_MIN_LEAD 150
#define SCAN_ARC_EITHER_SIDE_LEAD 300
#define SCAN_LINE_FRESH_US 100000u
#define SCAN_LINE_FOLLOW_US 8000000u
#define SCAN_U_TURN_DEG 180u
static volatile uint32_t scan_counts[2], scan_invalid;
static uint8_t scan_previous[2];
static volatile bool scan_running;
static volatile unsigned scan_reason;
typedef enum {SCAN_IDLE,SCAN_FORWARD,SCAN_WAIT_RESULT,SCAN_WAIT_BACK,
              SCAN_REVERSE,SCAN_WAIT_FORWARD,SCAN_WAIT_DEMO,SCAN_DEMO_FORWARD,
              SCAN_WAIT_COMMAND,SCAN_TURN,SCAN_STRAIGHT,SCAN_RIGHT_ARC,SCAN_LEFT_ARC,SCAN_LINE_FOLLOW} scan_phase_t;
static volatile bool scan_session;
static volatile scan_phase_t scan_phase;
static int scan_direction=1;
static uint32_t scan_pass_origin[2],scan_leg_goal[2],scan_pause_until,scan_retries;
static uint32_t scan_deadline_us, scan_step_us, scan_progress_us[2], scan_origin[2], scan_bad_origin;
static speed_control_t scan_speed;
static line_button_t scan_button;
static repeating_timer_t scan_timer;
static char scan_pending_command;
static char scan_junction_command = 'A';

static bool scan_is_arc(scan_phase_t phase)
{
    return phase == SCAN_RIGHT_ARC || phase == SCAN_LEFT_ARC;
}

static bool scan_line_candidate;
static uint32_t scan_line_since_us;
static int8_t scan_line_error;
static uint32_t scan_line_sample_us;
/* Reason 1=duration, 2=button, 3=encoder/control fault, 4=stalled,
 * 5=calibration/tests, 6=sampling overrun, 7=timer unavailable. */
static void scan_drive_stop(unsigned reason)
{
    uint32_t irq = save_and_disable_interrupts();
    scan_running = false;
    if (reason>=2 && reason!=8) {scan_session=false;scan_phase=SCAN_IDLE;}
    pwm_set_both_levels(4, 0, 0);
    pwm_set_both_levels(5, 0, 0);
    if (reason) scan_reason = reason;
    restore_interrupts(irq);
}
static void scan_encoder_irq(uint gpio, uint32_t events)
{
    (void)gpio; (void)events;
    uint32_t pins = gpio_get_all();
    for (unsigned w=0; w<2; ++w) {
        unsigned a=w ? MOTION_ENCODER_RIGHT_A : MOTION_ENCODER_LEFT_A;
        unsigned b=w ? MOTION_ENCODER_RIGHT_B : MOTION_ENCODER_LEFT_B;
        uint8_t next=(uint8_t)((((pins>>a)&1u)<<1)|((pins>>b)&1u));
        int delta=motion_quadrature_delta(scan_previous[w],next);
        scan_previous[w]=next;
        if (delta==2) ++scan_invalid;
        else scan_counts[w]+=(uint32_t)(delta*(w ? MOTION_RIGHT_ENCODER_SIGN : MOTION_LEFT_ENCODER_SIGN));
    }
}
static void scan_write(unsigned slice, unsigned duty, int sign)
{
    pwm_set_both_levels(slice, sign>0 ? duty:0, sign<0 ? duty:0);
}
static bool scan_motor_tick(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t now=time_us_32();
    if (scan_session && !gpio_get(20)) { scan_drive_stop(2); return true; }
    if (!scan_running) return true;
    const bool turning = scan_phase == SCAN_TURN;
    const bool straight = scan_phase == SCAN_STRAIGHT;
    const bool branch_arc = scan_is_arc(scan_phase);
    const bool line_follow = scan_phase == SCAN_LINE_FOLLOW;
    const bool bounded = turning || straight || scan_direction < 0;
    if ((int32_t)(now - scan_deadline_us) >= 0) {
        if (turning) scan_drive_stop(13);
        else if (straight) scan_drive_stop(15);
        else if (branch_arc) scan_drive_stop(16);
        else if (line_follow) scan_drive_stop(17);
        else scan_drive_stop(scan_direction < 0 ? 10 : 1);
        return true;
    }
    if ((branch_arc || line_follow) && now - scan_line_sample_us > SCAN_LINE_FRESH_US) {
        scan_drive_stop(18);
        return true;
    }
    uint32_t dt = now - scan_step_us;
    if (dt < CONTROL_MIN_US) return true;

    const int wheel_direction[2] = {scan_direction, turning ? -1 : scan_direction};
    int32_t total[2];
    bool reached[2];
    for (unsigned w = 0; w < 2; ++w) {
        total[w] = motion_count_delta(scan_counts[w], scan_origin[w]) * wheel_direction[w];
        reached[w] = bounded && total[w] >= (int32_t)scan_leg_goal[w];
        int32_t prior = w ? scan_speed.previous_right : scan_speed.previous_left;
        if (total[w] > prior) scan_progress_us[w] = now;
        if (!reached[w] && now - scan_progress_us[w] > 400000u) {
            scan_drive_stop(4);
            return true;
        }
    }
    if (scan_invalid != scan_bad_origin) {
        scan_drive_stop(3);
        return true;
    }
    if (reached[0] && reached[1]) {
        scan_drive_stop(turning ? 12 : straight ? 14 : 8);
        return true;
    }

    int32_t target_cps = scan_direction < 0 ? SCAN_REVERSE_TARGET_CPS : SCAN_FORWARD_TARGET_CPS;
    int32_t trim_limit = target_cps / 2 < 60 ? target_cps / 2 : 60;
    int32_t trim = clamp_i64(((int64_t)total[0] - total[1]) * 2, -trim_limit, trim_limit);
    int32_t left_target = reached[0] ? 0 : target_cps - (trim > 0 ? trim : 0);
    int32_t right_target = reached[1] ? 0 : target_cps + (trim < 0 ? trim : 0);
    if (branch_arc) {
        const bool right = scan_phase == SCAN_RIGHT_ARC;
        left_target = right ? SCAN_ARC_OUTER_CPS : SCAN_ARC_INNER_CPS;
        right_target = right ? SCAN_ARC_INNER_CPS : SCAN_ARC_OUTER_CPS;
    } else if (line_follow) {
        int16_t left, right;
        if (!motion_line_targets(scan_line_error, SCAN_FORWARD_TARGET_CPS,
                                 SCAN_ARC_OUTER_CPS, SCAN_LINE_GAIN_Q10, &left, &right)) {
            scan_drive_stop(3);
            return true;
        }
        left_target = left;
        right_target = right;
    }
    if (!speed_control_targets_step(&scan_speed, total[0], total[1], dt,
                                    left_target, right_target)) {
        scan_drive_stop(3);
        return true;
    }
    scan_step_us = now;
    scan_write(4, scan_speed.left.duty, MOTION_LEFT_MOTOR_SIGN * wheel_direction[0]);
    scan_write(5, scan_speed.right.duty, MOTION_RIGHT_MOTOR_SIGN * wheel_direction[1]);
    return true;
}
static bool scan_drive_init(void)
{
    for (unsigned pin=8; pin<=11; ++pin) {gpio_init(pin);gpio_set_dir(pin,GPIO_OUT);gpio_put(pin,0);}
    pwm_config config=pwm_get_default_config();
    pwm_config_set_wrap(&config,999);
    pwm_config_set_clkdiv(&config,(float)clock_get_hz(clk_sys)/10000000.0f);
    for (unsigned slice=4;slice<=5;++slice) {pwm_init(slice,&config,false);pwm_set_both_levels(slice,0,0);}
    for (unsigned pin=8;pin<=11;++pin) gpio_set_function(pin,GPIO_FUNC_PWM);
    pwm_set_enabled(4,true);pwm_set_enabled(5,true);
    gpio_init(20);gpio_set_dir(20,GPIO_IN);gpio_pull_up(20);
    for (unsigned pin=2;pin<=5;++pin) {gpio_init(pin);gpio_set_dir(pin,GPIO_IN);gpio_pull_up(pin);}
    uint32_t pins=gpio_get_all();
    scan_previous[0]=(uint8_t)((((pins>>4)&1u)<<1)|((pins>>5)&1u));
    scan_previous[1]=(uint8_t)((((pins>>2)&1u)<<1)|((pins>>3)&1u));
    gpio_set_irq_enabled_with_callback(2,GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL,true,scan_encoder_irq);
    for (unsigned pin=3;pin<=5;++pin) gpio_set_irq_enabled(pin,GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL,true);
    line_button_arm(&scan_button,to_ms_since_boot(get_absolute_time()),!gpio_get(20));
    bool ok=add_repeating_timer_ms(-5,scan_motor_tick,NULL,&scan_timer);
    if (!ok) scan_drive_stop(7);
    return ok;
}
static bool scan_begin_leg(scan_phase_t phase)
{
    const bool turning = phase == SCAN_TURN;
    const int direction = phase == SCAN_REVERSE ? -1 : 1;
    uint32_t duration_ms = SCAN_RUN_MS;
    if (phase == SCAN_REVERSE) duration_ms += 2000u;
    if (turning) duration_ms = scan_pending_command == 'D' ? 2u * SCAN_TURN_TIMEOUT_MS : SCAN_TURN_TIMEOUT_MS;
    if (phase == SCAN_STRAIGHT || scan_is_arc(phase)) duration_ms = SCAN_TURN_TIMEOUT_MS;

    uint32_t irq = save_and_disable_interrupts();
    if (!scan_session || !gpio_get(20)) {
        restore_interrupts(irq);
        return false;
    }
    speed_control_start(&scan_speed);
    scan_speed.left.kp_milli=scan_speed.right.kp_milli=120;
    scan_speed.left.ki_milli=scan_speed.right.ki_milli=480;
    scan_speed.left.minimum=scan_speed.right.minimum=0;
    scan_speed.left.integral_floor_milli=scan_speed.right.integral_floor_milli=-200000;
    scan_direction=direction;
    scan_phase = phase;
    scan_line_candidate = false;
    if (phase == SCAN_FORWARD) {
        scan_pass_origin[0] = scan_counts[0];
        scan_pass_origin[1] = scan_counts[1];
    }
    scan_origin[0]=scan_counts[0];scan_origin[1]=scan_counts[1];scan_bad_origin=scan_invalid;
    scan_step_us=time_us_32();scan_deadline_us=scan_step_us+duration_ms*1000u;
    scan_progress_us[0]=scan_progress_us[1]=scan_step_us;
    if (scan_is_arc(phase)) {
        scan_line_sample_us = scan_step_us;
        /* Bias the inner wheel before the first speed-control update. */
        wheel_speed_t *inner = phase == SCAN_RIGHT_ARC ? &scan_speed.right : &scan_speed.left;
        inner->duty = START_DUTY * 3u / 4u;
    }
    scan_running=true;
    scan_write(4,direction<0 && !scan_leg_goal[0] ? 0:scan_speed.left.duty,MOTION_LEFT_MOTOR_SIGN*scan_direction);
    scan_write(5,direction<0 && !scan_leg_goal[1] ? 0:scan_speed.right.duty,
               MOTION_RIGHT_MOTOR_SIGN * (turning ? -1 : direction));
    restore_interrupts(irq);
    return true;
}
static bool scan_drive_accept_samples(void)
{
    return !scan_session || scan_phase == SCAN_FORWARD ||
           scan_phase == SCAN_DEMO_FORWARD || scan_phase == SCAN_WAIT_RESULT;
}
static void scan_set_travel_goals(uint64_t distance_um)
{
    scan_leg_goal[0] = (uint32_t)((distance_um * MOTION_LEFT_COUNTS_REV +
                         MOTION_LEFT_CIRCUMFERENCE_UM / 2u) / MOTION_LEFT_CIRCUMFERENCE_UM);
    scan_leg_goal[1] = (uint32_t)((distance_um * MOTION_RIGHT_COUNTS_REV +
                         MOTION_RIGHT_CIRCUMFERENCE_UM / 2u) / MOTION_RIGHT_CIRCUMFERENCE_UM);
}

static void scan_drive_command(char code)
{
    if (!scan_session || (code != 'A' && code != 'B' && code != 'C' && code != 'D')) return;
    if (code == 'A' || code == 'B') {
        scan_begin_leg(code == 'A' ? SCAN_RIGHT_ARC : SCAN_LEFT_ARC);
        return;
    }
    scan_drive_stop(0);
    scan_pending_command = code;
    scan_phase = SCAN_WAIT_COMMAND;
    scan_pause_until = time_us_32() + SCAN_TURN_SETTLE_US;
}

static void scan_drive_observe_junction(bool both_black)
{
    if (!scan_session || !scan_running ||
        (scan_phase != SCAN_FORWARD && scan_phase != SCAN_DEMO_FORWARD) ||
        !both_black) {
        scan_line_candidate = false;
        return;
    }

    uint32_t now = time_us_32();
    if (!scan_line_candidate) {
        scan_line_candidate = true;
        scan_line_since_us = now;
    } else if (now - scan_line_since_us >= SCAN_JUNCTION_CONFIRM_US) {
        scan_drive_command(scan_junction_command);
        const scan_phase_t phase = scan_phase;
        if (scan_is_arc(phase))
            printf("JUNCTION: %c, %s arc started\n", scan_junction_command,
                   phase == SCAN_RIGHT_ARC ? "right" : "left");
    }
}

static void scan_drive_observe_line(bool valid, bool both_black, bool line_lost,
                                    uint8_t a, uint8_t b, int8_t error)
{
    if (!scan_is_arc(scan_phase) && scan_phase != SCAN_LINE_FOLLOW) {
        scan_drive_observe_junction(valid && both_black);
        return;
    }
    if (!scan_session || !scan_running) return;
    if (!valid || (scan_phase == SCAN_LINE_FOLLOW && line_lost)) {
        scan_drive_stop(18);
        return;
    }

    uint32_t now = time_us_32();
    uint32_t irq = save_and_disable_interrupts();
    scan_line_sample_us = now;
    scan_line_error = error;
    bool acquired = false;
    const scan_phase_t phase = scan_phase;
    const bool turning_right = phase == SCAN_RIGHT_ARC;
    if (scan_running && scan_is_arc(phase)) {
        int32_t left = motion_count_delta(scan_counts[0], scan_origin[0]);
        int32_t right = motion_count_delta(scan_counts[1], scan_origin[1]);
        int64_t lead = turning_right ? (int64_t)left - right : (int64_t)right - left;
        bool right_sighting = b >= SCAN_EXIT_MIN_DARKNESS && (int)b - a >= SCAN_EXIT_MIN_CONTRAST;
        bool left_sighting = a >= SCAN_EXIT_MIN_DARKNESS && (int)a - b >= SCAN_EXIT_MIN_CONTRAST;
        bool turn_side_sighting = turning_right ? right_sighting : left_sighting;
        bool either_side_sighting = lead >= SCAN_ARC_EITHER_SIDE_LEAD && (left_sighting || right_sighting);
        if (both_black || lead < SCAN_ARC_MIN_LEAD || !(turn_side_sighting || either_side_sighting)) {
            scan_line_candidate = false;
        } else if (!scan_line_candidate) {
            scan_line_candidate = true;
            scan_line_since_us = now;
        } else if (now - scan_line_since_us >= SCAN_EXIT_CONFIRM_US) {
            scan_phase = SCAN_LINE_FOLLOW;
            scan_deadline_us = now + SCAN_LINE_FOLLOW_US;
            acquired = true;
        }
    }
    restore_interrupts(irq);
    if (acquired)
        printf("%s EXIT: line acquired; tracing (A=%u B=%u err=%+d)\n",
               turning_right ? "RIGHT" : "LEFT", (unsigned)a, (unsigned)b, error);
}

static void scan_drive_start_demo(void)
{
    puts("STARTUP: starts in 2 s, then forward up to 8 s to find a junction. GP20 cancels.");
    scan_pause_until = time_us_32() + SCAN_DEMO_DELAY_US;
    scan_phase = SCAN_WAIT_DEMO;
    scan_session = true;
}

/* Return to the beginning of this pass after an incomplete capture. */
static void scan_drive_result(unsigned count, bool overflow)
{
    if (!scan_session || (scan_phase!=SCAN_FORWARD && scan_phase!=SCAN_WAIT_RESULT)) return;
    if (count<5 || count>=29 || overflow) return;
    scan_drive_stop(0);
    scan_phase=SCAN_WAIT_BACK;
    scan_pause_until=time_us_32()+300000u;
    ++scan_retries;
    printf("RETRY %u: %u/29 elements; brake, return to pass start, rescan\n",
           (unsigned)scan_retries,count);
}
/* Main loop only; motor timer independently enforces motion stop. */
static bool scan_drive_poll(bool calibrated)
{
    uint32_t now=to_ms_since_boot(get_absolute_time());
    if(scan_reason) {
        uint32_t irq=save_and_disable_interrupts();unsigned reason=scan_reason;scan_reason=0;
        restore_interrupts(irq);
        printf("DRIVE stopped: reason=%u (1=forward complete 2=button 3=encoder/control 4=stall 5=calibration/tests 6=sampling 7=timer 8=reverse returned 10=reverse timeout 11=no retryable capture 12=turn complete 13=turn timeout 14=straight complete 15=straight timeout 16=branch exit not found 17=line trace complete 18=line lost or stale)\n",reason);
        if (reason == 1 && scan_phase == SCAN_DEMO_FORWARD) {
            puts("STARTUP: no confirmed junction within 8 s");
            scan_drive_stop(11);
        } else if(reason==1 && scan_phase==SCAN_FORWARD) {
            scan_phase=SCAN_WAIT_RESULT;scan_pause_until=time_us_32()+2000000u;
        } else if(reason==8 && scan_phase==SCAN_REVERSE) {
            scan_phase=SCAN_WAIT_FORWARD;scan_pause_until=time_us_32()+300000u;
        } else if(!scan_session) line_button_arm(&scan_button,now,!gpio_get(20));
    }
    if(scan_session) {
        if(!calibrated){scan_drive_stop(5);return false;}
        if(!gpio_get(20)){scan_drive_stop(2);return false;}
        const bool pause_elapsed = (int32_t)(time_us_32() - scan_pause_until) >= 0;
        if (scan_phase == SCAN_WAIT_DEMO && pause_elapsed) {
            puts("STARTUP: moving forward");
            scan_begin_leg(SCAN_DEMO_FORWARD);
        } else if (scan_phase == SCAN_WAIT_COMMAND && pause_elapsed) {
            if (scan_pending_command == 'C') {
                scan_set_travel_goals(SCAN_STRAIGHT_DISTANCE_UM);
                printf("COMMAND C: straight %u mm\n", SCAN_STRAIGHT_DISTANCE_UM / 1000u);
                scan_begin_leg(SCAN_STRAIGHT);
            } else {
                /* Match the integrated controller's per-wheel pivot arc. */
                uint64_t arc_um = (uint64_t)MOTION_TRACK_WIDTH_MM * 1000u * SCAN_U_TURN_DEG * 355u /
                                  (360u * 113u);
                scan_set_travel_goals(arc_um);
                printf("COMMAND D: pivot %u deg (%u/%u encoder counts)\n",
                       SCAN_U_TURN_DEG, (unsigned)scan_leg_goal[0], (unsigned)scan_leg_goal[1]);
                scan_begin_leg(SCAN_TURN);
            }
        } else if (scan_phase == SCAN_WAIT_RESULT && pause_elapsed) {
            scan_drive_stop(11);
        }
        if(scan_phase==SCAN_WAIT_BACK && (int32_t)(time_us_32()-scan_pause_until)>=0) {
            for(unsigned w=0;w<2;++w) {
                int32_t distance=motion_count_delta(scan_counts[w],scan_pass_origin[w]);
                if(distance<0 || distance>10000){scan_drive_stop(3);return false;}
                scan_leg_goal[w]=(uint32_t)distance;
            }
            printf("REVERSE: return %u left / %u right encoder counts\n",
                   (unsigned)scan_leg_goal[0],(unsigned)scan_leg_goal[1]);
            scan_begin_leg(SCAN_REVERSE);
        } else if(scan_phase==SCAN_WAIT_FORWARD && (int32_t)(time_us_32()-scan_pause_until)>=0) {
            puts("RESCAN: forward up to 8 seconds");
            return scan_begin_leg(SCAN_FORWARD);
        }
        return false;
    }
    line_button_event_t event=line_button_step(&scan_button,now,!gpio_get(20));
    if(event!=LINE_BUTTON_START) return false;
    if(!calibrated) {
        puts("DRIVE rejected: barcode calibration invalid");
        line_button_arm(&scan_button,now,!gpio_get(20));return false;
    }
    printf("DRIVE: forward up to 8 seconds at %u counts/s; incomplete scans retry; GP20 stops\n",
           (unsigned)SCAN_FORWARD_TARGET_CPS);
    scan_session = true;
    scan_retries = 0;
    return scan_begin_leg(SCAN_FORWARD);
}
#endif

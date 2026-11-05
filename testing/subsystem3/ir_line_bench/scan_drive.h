/* Standalone scan bench only. No RTOS/network/route commands. */
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
static volatile uint32_t scan_counts[2], scan_invalid;
static uint8_t scan_previous[2];
static volatile bool scan_running;
static volatile unsigned scan_reason;
typedef enum {SCAN_IDLE,SCAN_FORWARD,SCAN_WAIT_RESULT,SCAN_WAIT_BACK,
              SCAN_REVERSE,SCAN_WAIT_FORWARD} scan_phase_t;
static volatile bool scan_session;
static volatile scan_phase_t scan_phase;
static int scan_direction=1;
static uint32_t scan_pass_origin[2],scan_back_goal[2],scan_pause_until,scan_retries;
static uint32_t scan_deadline_us, scan_step_us, scan_progress_us[2], scan_origin[2], scan_bad_origin;
static speed_control_t scan_speed;
static line_button_t scan_button;
static repeating_timer_t scan_timer;
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
    if ((int32_t)(now-scan_deadline_us)>=0) {
        scan_drive_stop(scan_direction<0 ? 10:1); return true;
    }
    uint32_t dt=now-scan_step_us;
    if (dt<CONTROL_MIN_US) return true;
    int32_t total[2];
    for (unsigned w=0; w<2; ++w) {
        total[w]=motion_count_delta(scan_counts[w],scan_origin[w])*scan_direction;
        int32_t prior=w ? scan_speed.previous_right:scan_speed.previous_left;
        if (total[w]>prior) scan_progress_us[w]=now;
        if (!(scan_direction<0 && total[w]>=(int32_t)scan_back_goal[w]) && now-scan_progress_us[w]>400000u) { scan_drive_stop(4); return true; }
    }
    if (scan_direction<0 && total[0]>=(int32_t)scan_back_goal[0] &&
        total[1]>=(int32_t)scan_back_goal[1]) {scan_drive_stop(8);return true;}
    int32_t target_cps = scan_direction < 0 ? SCAN_REVERSE_TARGET_CPS : SCAN_FORWARD_TARGET_CPS;
    /* Keep steering correction from requesting a stopped or negative wheel target. */
    int32_t trim_limit = target_cps / 2 < 60 ? target_cps / 2 : 60;
    int32_t trim = clamp_i64(((int64_t)total[0] - total[1]) * 2, -trim_limit, trim_limit);
    if (scan_invalid!=scan_bad_origin ||
        !speed_control_targets_step(&scan_speed,total[0],total[1],dt,
                                    scan_direction<0 && total[0]>=(int32_t)scan_back_goal[0] ? 0:target_cps-(trim>0?trim:0),
                                    scan_direction<0 && total[1]>=(int32_t)scan_back_goal[1] ? 0:target_cps+(trim<0?trim:0))) {
        scan_drive_stop(3);return true;
    }
    scan_step_us=now;
    scan_write(4,scan_speed.left.duty,MOTION_LEFT_MOTOR_SIGN*scan_direction);
    scan_write(5,scan_speed.right.duty,MOTION_RIGHT_MOTOR_SIGN*scan_direction);
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
static void scan_begin_leg(int direction)
{
    uint32_t irq=save_and_disable_interrupts();
    speed_control_start(&scan_speed);
    scan_speed.left.kp_milli=scan_speed.right.kp_milli=120;
    scan_speed.left.ki_milli=scan_speed.right.ki_milli=480;
    scan_speed.left.minimum=scan_speed.right.minimum=0;
    scan_speed.left.integral_floor_milli=scan_speed.right.integral_floor_milli=-200000;
    scan_direction=direction;
    scan_phase=direction>0 ? SCAN_FORWARD:SCAN_REVERSE;
    if(direction>0){scan_pass_origin[0]=scan_counts[0];scan_pass_origin[1]=scan_counts[1];}
    scan_origin[0]=scan_counts[0];scan_origin[1]=scan_counts[1];scan_bad_origin=scan_invalid;
    scan_step_us=time_us_32();scan_deadline_us=scan_step_us+(SCAN_RUN_MS+(direction<0 ? 2000u:0u))*1000u;
    scan_progress_us[0]=scan_progress_us[1]=scan_step_us;
    scan_running=true;
    scan_write(4,direction<0 && !scan_back_goal[0] ? 0:scan_speed.left.duty,MOTION_LEFT_MOTOR_SIGN*scan_direction);
    scan_write(5,direction<0 && !scan_back_goal[1] ? 0:scan_speed.right.duty,MOTION_RIGHT_MOTOR_SIGN*scan_direction);
    restore_interrupts(irq);
}
static bool scan_drive_accept_samples(void)
{
    return !scan_session || scan_phase==SCAN_FORWARD || scan_phase==SCAN_WAIT_RESULT;
}
/* Return to the beginning of this pass, not an arbitrary extra distance. */
static void scan_drive_result(unsigned count,bool overflow)
{
    if (!scan_session || (scan_phase!=SCAN_FORWARD && scan_phase!=SCAN_WAIT_RESULT)) return;
    if (count==29 && !overflow) {scan_drive_stop(9);return;}
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
        printf("DRIVE stopped: reason=%u (1=forward complete 2=button 3=encoder/control 4=stall 5=calibration/tests 6=sampling 7=timer 8=reverse returned 9=29 elements 10=reverse timeout 11=no retryable capture)\n",reason);
        if(reason==1 && scan_phase==SCAN_FORWARD) {
            scan_phase=SCAN_WAIT_RESULT;scan_pause_until=time_us_32()+2000000u;
        } else if(reason==8 && scan_phase==SCAN_REVERSE) {
            scan_phase=SCAN_WAIT_FORWARD;scan_pause_until=time_us_32()+300000u;
        } else if(!scan_session) line_button_arm(&scan_button,now,!gpio_get(20));
    }
    if(scan_session) {
        if(!calibrated){scan_drive_stop(5);return false;}
        if(!gpio_get(20)){scan_drive_stop(2);return false;}
        if(scan_phase==SCAN_WAIT_RESULT && (int32_t)(time_us_32()-scan_pause_until)>=0)
            scan_drive_stop(11);
        if(scan_phase==SCAN_WAIT_BACK && (int32_t)(time_us_32()-scan_pause_until)>=0) {
            for(unsigned w=0;w<2;++w) {
                int32_t distance=motion_count_delta(scan_counts[w],scan_pass_origin[w]);
                if(distance<0 || distance>10000){scan_drive_stop(3);return false;}
                scan_back_goal[w]=(uint32_t)distance;
            }
            printf("REVERSE: return %u left / %u right encoder counts\n",
                   (unsigned)scan_back_goal[0],(unsigned)scan_back_goal[1]);
            scan_begin_leg(-1);
        } else if(scan_phase==SCAN_WAIT_FORWARD && (int32_t)(time_us_32()-scan_pause_until)>=0) {
            puts("RESCAN: forward up to 8 seconds");scan_begin_leg(1);return true;
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
    scan_session=true;scan_retries=0;scan_begin_leg(1);return true;
}
#endif

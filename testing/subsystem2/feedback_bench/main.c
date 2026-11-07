/* Supervised, bounded per-wheel speed-PI bench. Never auto-runs. */
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/watchdog.h"
#include "controller.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/structs/io_bank0.h"
#include "../../../app_program/subsystem2/motion_logic.h"

#define M1_A 8u
#define M1_B 9u
#define M2_A 10u
#define M2_B 11u
#define PWM_HZ 10000u
#define PWM_COUNTS 1000u
#define PULSE_US 2000000u
#define SETTLE_US 250000u
#define SAMPLE_US 20000u
#define SAMPLE_CAPACITY 128u
#define DUTY_PERMILLE START_DUTY
static speed_control_t controller;
static unsigned applied_left,applied_right;
#define ENCODER_MASK ((12u<<8)|(12u<<12)|(12u<<16)|(12u<<20))

static uint slice_m1;
static uint slice_m2;

static void stop_motors(void)
{
    // Both driver inputs LOW; brake/coast behavior depends on the driver.
    // This commands the outputs; it does not measure physical stopping time.
    pwm_set_both_levels(slice_m1, 0, 0);
    pwm_set_both_levels(slice_m2, 0, 0);
    applied_left=applied_right=0;
    speed_control_reset(&controller);
}

static bool init_motors(void)
{
    const uint pins[] = {M1_A, M1_B, M2_A, M2_B};
    for (uint i = 0; i < 4; ++i) {
        gpio_init(pins[i]);
        gpio_put(pins[i], 0);
        gpio_set_dir(pins[i], GPIO_OUT);
    }
    slice_m1 = pwm_gpio_to_slice_num(M1_A);
    slice_m2 = pwm_gpio_to_slice_num(M2_A);

    // fPWM = actual system clock / (divider * (TOP + 1)).
    // A modest TOP avoids the invalid <1 divider in the LAB4 example.
    const float divider = (float)clock_get_hz(clk_sys) /
                          ((float)PWM_HZ * (float)PWM_COUNTS);
    if (divider < 1.0f || divider >= 256.0f) {
        return false;  // GPIO outputs remain LOW.
    }
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, divider);
    pwm_config_set_wrap(&config, PWM_COUNTS - 1u);
    pwm_init(slice_m1, &config, false);
    pwm_init(slice_m2, &config, false);
    stop_motors();
    for (uint i = 0; i < 4; ++i) {
        gpio_set_function(pins[i], GPIO_FUNC_PWM);
    }
    pwm_set_enabled(slice_m1, true);
    pwm_set_enabled(slice_m2, true);
    return true;
}

static void drive_motor(uint slice, bool reverse, unsigned duty)
{
    const uint16_t level = (uint16_t)(PWM_COUNTS * duty / 1000u);
    // PWM on one input; the other stays LOW. Positive is electrical A->B,
    // not guaranteed physical forward on a mirrored pair of wheel motors.
    pwm_set_both_levels(slice, reverse ? 0 : level, reverse ? level : 0);
}


typedef struct {
    uint64_t us;
    uint32_t left,right,left_bad,right_bad;
    unsigned duty_left,duty_right;
    int32_t speed_left,speed_right,target_left,target_right,integral_left,integral_right;
    unsigned limits_left,limits_right;
    bool speed_valid;
} snapshot_t;
static volatile uint32_t left_count,right_count,left_bad,right_bad;
static uint8_t previous_left,previous_right;
static snapshot_t samples[SAMPLE_CAPACITY];
static unsigned sample_count;
static bool sample_overflow;
static snapshot_t origin, stopped;

static uint8_t pair_state(uint32_t gpio,unsigned a,unsigned b)
{
    return (uint8_t)((((gpio>>a)&1u)<<1)|((gpio>>b)&1u));
}
static void count_pair(uint32_t gpio,uint32_t edges,unsigned a,unsigned b,
                       uint8_t *previous,volatile uint32_t *count,volatile uint32_t *bad)
{
    uint32_t pair_edges=edges&((12u<<(a*4))|(12u<<(b*4)));
    if(!pair_edges) return;
    uint8_t state=pair_state(gpio,a,b);
    int8_t delta=motion_quadrature_delta(*previous,state);
    if(delta==2 || delta==0 || (pair_edges&(pair_edges-1u))) {
        if(*bad!=UINT32_MAX) ++*bad;
    } else if(delta>0) ++*count;
    else --*count;
    *previous=state;
}
static void encoder_irq(void)
{
    uint32_t edges=io_bank0_hw->intr[0]&ENCODER_MASK;
    io_bank0_hw->intr[0]=edges;
    uint32_t gpio=gpio_get_all();
    count_pair(gpio,edges,4,5,&previous_left,&left_count,&left_bad);
    count_pair(gpio,edges,2,3,&previous_right,&right_count,&right_bad);
}
static void init_encoders(void)
{
    for(unsigned pin=2;pin<=5;++pin) {
        gpio_init(pin);gpio_set_dir(pin,GPIO_IN);gpio_disable_pulls(pin);
    }
    uint32_t gpio=gpio_get_all();
    previous_left=pair_state(gpio,4,5);previous_right=pair_state(gpio,2,3);
    io_bank0_hw->intr[0]=ENCODER_MASK;
    gpio_add_raw_irq_handler_masked(0x3cu,encoder_irq);
    for(unsigned pin=2;pin<=5;++pin) gpio_set_irq_enabled(pin,GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL,true);
    irq_set_enabled(IO_IRQ_BANK0,true);
}
static snapshot_t snapshot(void)
{
    uint32_t irq=save_and_disable_interrupts();
    snapshot_t s={.us=time_us_64(),.left=left_count,.right=right_count,
                  .left_bad=left_bad,.right_bad=right_bad,
                  .duty_left=applied_left,.duty_right=applied_right,
                  .speed_left=controller.left.filtered_milli_cps/1000,
                  .speed_right=controller.right.filtered_milli_cps/1000,
                  .target_left=controller.left.target_cps,.target_right=controller.right.target_cps,
                  .integral_left=controller.left.integral_milli,.integral_right=controller.right.integral_milli,
                  .limits_left=controller.left.limits,.limits_right=controller.right.limits,
                  .speed_valid=controller.valid};
    restore_interrupts(irq);
    return s;
}
static void save_sample(snapshot_t s)
{
    if(sample_count<SAMPLE_CAPACITY) samples[sample_count++]=s;
    else sample_overflow=true;
}
static void help(void)
{
    printf("\nBuddy 2 bounded feedback bench v7: motors GP8/9 left, GP10/11 right; encoders GP4/5 left, GP2/3 right\n"
           "f = ONE forward run; s/SPACE/h = stop; i = raw counts\n"
           "Duration=2000 ms Duty=14..20%% Control=speed-PI Target=1200cps Kp=0.03 Ki=0.24 FF=15.5/18.5%% Slew=0.5%%/20ms CPR=2550/2550 provisional\n"
           "Watchdog=100ms Stall=200ms TravelLimit=5600counts. No automatic sequence.\n");
}
static void report(unsigned id,const char *reason,snapshot_t final)
{
    printf("RESULT id=%u reason=%s powered_us=%llu total_us=%llu left_raw_delta=%ld right_raw_delta=%ld left_bad=%lu right_bad=%lu samples=%u overflow=%u\n",
           id,reason,(unsigned long long)(stopped.us-origin.us),(unsigned long long)(final.us-origin.us),
           (long)motion_count_delta(final.left,origin.left),(long)motion_count_delta(final.right,origin.right),
           (unsigned long)(final.left_bad-origin.left_bad),(unsigned long)(final.right_bad-origin.right_bad),
           sample_count,sample_overflow);
    printf("CSV elapsed_us,left_raw_delta,right_raw_delta,left_bad,right_bad,powered,left_duty_permille,right_duty_permille,left_speed_cps,right_speed_cps,left_target_cps,right_target_cps,left_integral_milli,right_integral_milli,left_limits,right_limits,speed_valid\n");
    for(unsigned i=0;i<sample_count;++i) {
        snapshot_t s=samples[i];
        printf("CSV %llu,%ld,%ld,%lu,%lu,%u,%u,%u,%ld,%ld,%ld,%ld,%ld,%ld,%u,%u,%u\n",(unsigned long long)(s.us-origin.us),
               (long)motion_count_delta(s.left,origin.left),(long)motion_count_delta(s.right,origin.right),
               (unsigned long)(s.left_bad-origin.left_bad),(unsigned long)(s.right_bad-origin.right_bad),
               s.us<stopped.us && (s.duty_left || s.duty_right),s.duty_left,s.duty_right,
               (long)s.speed_left,(long)s.speed_right,(long)s.target_left,(long)s.target_right,
               (long)s.integral_left,(long)s.integral_right,s.limits_left,s.limits_right,s.speed_valid);
    }
    printf("END id=%u motors=STOPPED\n",id);
}
int main(void)
{
    bool ready=init_motors();
    stdio_init_all();
    if(!ready) for(;;) {printf("ERROR motor init; GPIO held LOW\n");sleep_ms(1000);}
    init_encoders();
    bool running=false,capturing=false,report_pending=false,connected_before=false;
    unsigned id=0;
    const char *reason="none";
    uint64_t stop_at=0,settle_at=0,next_sample=0,last_control=0,last_loop=0;
    uint64_t left_advance=0,right_advance=0;
    int32_t best_left=0,best_right=0;
    snapshot_t final={0};
    for(;;) {
        uint64_t now=time_us_64();
        bool connected=stdio_usb_connected();
        int c=connected ? getchar_timeout_us(0) : PICO_ERROR_TIMEOUT;
        bool stop_request=c=='s'||c==' '||c=='h'||c=='?';
        if(running) {
            snapshot_t current=snapshot();
            int64_t left=-(int64_t)motion_count_delta(current.left,origin.left);
            int32_t right=motion_count_delta(current.right,origin.right);
            const char *fault=NULL;
            if(!connected) fault="disconnect";
            else if(stop_request) fault="requested";
            else if(now-last_loop>60000u) fault="loop_late";
            else if(current.left_bad!=origin.left_bad || current.right_bad!=origin.right_bad) fault="encoder_invalid";
            else if(left < -4 || right < -4) fault="encoder_reverse";
            else if(left>=5600 || right>=5600) fault="travel_limit";
            else if(now>=stop_at) fault="complete";
            if(!fault && left>best_left) {best_left=(int32_t)left;left_advance=now;}
            if(!fault && right>best_right) {best_right=right;right_advance=now;}
            if(!fault && (now-left_advance>=200000u || now-right_advance>=200000u)) fault="stall";
            if(!fault && current.us-last_control>=SAMPLE_US) {
                if(!speed_control_step(&controller,(int32_t)left,right,
                                       (uint32_t)(current.us-last_control))) fault="speed_input";
                else {
                    applied_left=controller.left.duty;applied_right=controller.right.duty;
                    drive_motor(slice_m1,true,applied_left);
                    drive_motor(slice_m2,false,applied_right);
                    last_control=current.us;
                }
            }
            if(fault) {
                stop_motors();watchdog_disable();stopped=snapshot();save_sample(stopped);
                running=false;settle_at=stopped.us+SETTLE_US;reason=fault;
            } else {
                watchdog_update();
            }
            last_loop=now;
        }
        if(capturing && now>=next_sample) {
            save_sample(snapshot());next_sample=now+SAMPLE_US;
        }
        if(capturing && !running && now>=settle_at) {
            final=snapshot();save_sample(final);capturing=false;report_pending=true;
        }
        /* Never print while powered, and never replay queued movement commands. */
        if(report_pending && connected) {
            report(id,reason,final);report_pending=false;
            while(getchar_timeout_us(0)!=PICO_ERROR_TIMEOUT) {}
            connected_before=connected;sleep_ms(1);continue;
        }
        if(!capturing && !report_pending && connected) {
            if(!connected_before) {help();c=PICO_ERROR_TIMEOUT;}
            if(stop_request) {
                stop_motors();
                if(c=='h'||c=='?') help();else printf("STOP: requested.\n");
            } else if(c=='i') {
                snapshot_t s=snapshot();
                printf("RAW left=%lu right=%lu left_bad=%lu right_bad=%lu\n",
                       (unsigned long)s.left,(unsigned long)s.right,(unsigned long)s.left_bad,(unsigned long)s.right_bad);
            } else if(c=='f') {
                ++id;printf("RUN id=%u forward duration=2000ms duty_max=20%% control=speed-PI\n",id);
                stop_motors();sample_count=0;sample_overflow=false;
                origin=snapshot();save_sample(origin);
                stop_at=origin.us+PULSE_US;next_sample=origin.us+SAMPLE_US;
                left_advance=right_advance=last_control=last_loop=origin.us;
                best_left=best_right=0;
                watchdog_enable(100,false);
                running=capturing=true;
                speed_control_start(&controller);
                applied_left=applied_right=DUTY_PERMILLE;
                drive_motor(slice_m1,true,applied_left);
                drive_motor(slice_m2,false,applied_right);
            }
        }
        connected_before=connected;sleep_ms(1);
    }
}

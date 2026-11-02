/* Operator-triggered single-wheel encoder count bench. Never auto-runs.
 * Count physical full + fractional output-wheel turns independently.
 * No assumed counts/rev are used to stop the test or to report wheel speed.
 */
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
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
#define DUTY_PERCENT 20u
#define ENCODER_MASK ((12u<<8)|(12u<<12)|(12u<<16)|(12u<<20))

static uint slice_m1;
static uint slice_m2;

static void stop_motors(void)
{
    // Both driver inputs LOW; brake/coast behavior depends on the driver.
    // This commands the outputs; it does not measure physical stopping time.
    pwm_set_both_levels(slice_m1, 0, 0);
    pwm_set_both_levels(slice_m2, 0, 0);
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
    const uint16_t level = (uint16_t)(PWM_COUNTS * duty / 100u);
    // PWM on one input; the other stays LOW. Positive is electrical A->B,
    // not guaranteed physical forward on a mirrored pair of wheel motors.
    pwm_set_both_levels(slice, reverse ? 0 : level, reverse ? level : 0);
}


typedef struct {
    uint64_t us;
    uint32_t left,right,left_bad,right_bad;
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
                  .left_bad=left_bad,.right_bad=right_bad};
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
    printf("\nBuddy 2 encoder count bench v1: motors GP8/9 left, GP10/11 right; encoders GP4/5 left, GP2/3 right\n"
           "r = RIGHT ONLY forward; l = LEFT ONLY forward; s/SPACE = stop; h = help; i = raw counts\n"
           "Duty=20%% Duration=2000 ms Settle=250 ms. Wheels lifted. No automatic sequence.\n");
}
static void print_raw(void)
{
    snapshot_t s=snapshot();
    printf("RAW left=%lu right=%lu left_bad=%lu right_bad=%lu\n",
           (unsigned long)s.left,(unsigned long)s.right,(unsigned long)s.left_bad,(unsigned long)s.right_bad);
}
static void report(unsigned id,char wheel,const char *reason,snapshot_t final)
{
    printf("RESULT id=%u wheel=%c reason=%s powered_us=%llu total_us=%llu left_raw_delta=%ld right_raw_delta=%ld left_bad=%lu right_bad=%lu samples=%u overflow=%u\n",
           id,wheel,reason,(unsigned long long)(stopped.us-origin.us),(unsigned long long)(final.us-origin.us),
           (long)motion_count_delta(final.left,origin.left),(long)motion_count_delta(final.right,origin.right),
           (unsigned long)(final.left_bad-origin.left_bad),(unsigned long)(final.right_bad-origin.right_bad),
           sample_count,sample_overflow);
    printf("CSV elapsed_us,left_raw_delta,right_raw_delta,left_bad,right_bad,powered\n");
    for(unsigned i=0;i<sample_count;++i) {
        snapshot_t s=samples[i];
        printf("CSV %llu,%ld,%ld,%lu,%lu,%u\n",(unsigned long long)(s.us-origin.us),
               (long)motion_count_delta(s.left,origin.left),(long)motion_count_delta(s.right,origin.right),
               (unsigned long)(s.left_bad-origin.left_bad),(unsigned long)(s.right_bad-origin.right_bad),s.us<stopped.us);
    }
    printf("END id=%u wheel=%c motors=STOPPED\n",id,wheel);
}
int main(void)
{
    bool ready=init_motors();
    stdio_init_all();
    if(!ready) for(;;) {printf("ERROR motor init; GPIO held LOW\n");sleep_ms(1000);}
    init_encoders();
    bool running=false,capturing=false,report_pending=false,connected_before=false;
    unsigned id=0;
    char wheel='?';
    const char *reason="none";
    uint64_t stop_at=0,settle_at=0,next_sample=0;
    snapshot_t final={0};
    for(;;) {
        uint64_t now=time_us_64();
        bool connected=stdio_usb_connected();
        int c=connected ? getchar_timeout_us(0) : PICO_ERROR_TIMEOUT;
        bool stop_request=c=='s'||c==' '||c=='h'||c=='?';
        if(running && (!connected || now>=stop_at || stop_request)) {
            stop_motors();stopped=snapshot();save_sample(stopped);
            running=false;settle_at=stopped.us+SETTLE_US;
            reason=!connected ? "disconnect" : stop_request ? "requested" : "complete";
        }
        if(capturing && now>=next_sample) {
            save_sample(snapshot());next_sample=now+SAMPLE_US;
        }
        if(capturing && !running && now>=settle_at) {
            final=snapshot();save_sample(final);capturing=false;report_pending=true;
        }
        /* No printing while powered. Report retained if USB disconnected. */
        if(report_pending && connected) {
            report(id,wheel,reason,final);report_pending=false;
            while(getchar_timeout_us(0)!=PICO_ERROR_TIMEOUT) {}
            connected_before=connected;sleep_ms(1);continue;
        }
        if(!capturing && !report_pending && connected) {
            if(!connected_before) {help();c=PICO_ERROR_TIMEOUT;}
            if(stop_request) {
                stop_motors();
                if(c=='h'||c=='?') help();else printf("STOP: requested.\n");
            } else if(c=='i') print_raw();
            else if(c=='r'||c=='l') {
                wheel=c;++id;
                printf("RUN id=%u wheel=%c duty=20%% duration=2000ms\n",id,wheel);
                sample_count=0;sample_overflow=false;origin=snapshot();save_sample(origin);
                stop_at=origin.us+PULSE_US;next_sample=origin.us+SAMPLE_US;
                running=capturing=true;
                stop_motors();
                if(wheel=='r') drive_motor(slice_m2,false,DUTY_PERCENT);
                else drive_motor(slice_m1,true,DUTY_PERCENT);
            }
        }
        connected_before=connected;
        sleep_ms(1);
    }
}

/* Host simulation executes the actual firmware loop; does not prove hardware timing. */
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
static int bench_printf(const char *format,...);
#define printf bench_printf
#define main bench_main
#include "main.c"
#undef main
#undef printf
static jmp_buf done;
static fake_io_bank_t regs;
fake_io_bank_t *io_bank0_hw=&regs;
static uint64_t clock_us,on_at,off_at,last_feed;
static unsigned scenario,starts,mask,ld,rd,min_left,min_right;
static bool run_sent,extra_sent,stop_sent,wd;
static uint32_t fake_gpio,lfraction,rfraction;
static char result_reason[40];
uint64_t time_us_64(void){return clock_us;}
void sleep_ms(uint ms){
    if(scenario==8 && clock_us>=1000000u)longjmp(done,1);
    unsigned elapsed=ms*1000u;
    if(scenario==7 && clock_us==100000u)elapsed=70000u;
    if(scenario==10 && clock_us==100000u)elapsed=60000u;
    if(ld){
        unsigned amount=(ld>90 ? ld-90 : 0)*14u;
        if(scenario==3)amount=0;

        lfraction+=amount;unsigned delta=lfraction/1000u;lfraction%=1000u;
        if(scenario==4)left_count+=delta;else left_count-=delta;
    }
    if(rd){
        unsigned amount=(rd>90 ? rd-90 : 0)*12u;
        if(scenario==11)amount=0;
        rfraction+=amount;unsigned delta=rfraction/1000u;rfraction%=1000u;
        if(scenario==12)right_count-=delta;else right_count+=delta;
    }
    if(scenario==6 && ld && clock_us==15000u)left_count-=5600;
    if(scenario==9 && ld && clock_us==15000u)left_count-=400;
    clock_us+=elapsed;
    if(scenario==5 && clock_us>=100000u)left_bad=1;
    if(wd)assert(clock_us-last_feed<=100000u);
    assert(clock_us<3000000u);
}
bool stdio_usb_connected(void){return !(scenario==1 && clock_us>=150000u && clock_us<300000u);}
int getchar_timeout_us(uint timeout){
    (void)timeout;
    if(scenario==8)return PICO_ERROR_TIMEOUT;
    if(clock_us>=10000u && !run_sent){run_sent=true;return 'f';}
    if(clock_us>=50000u && !extra_sent){extra_sent=true;return 'f';}
    if(scenario==2 && clock_us>=150000u && !stop_sent){stop_sent=true;return 's';}
    return PICO_ERROR_TIMEOUT;
}
static int bench_printf(const char *format,...){
    assert(ld==0 && rd==0);
    char out[1024];va_list ap;va_start(ap,format);int n=vsnprintf(out,sizeof(out),format,ap);va_end(ap);
    if(!strncmp(out,"CSV ",4) && out[4]>='0' && out[4]<='9') {
        unsigned long long t;long l,r,ls,rs,lt,rt,li,ri;
        unsigned lb,rb,powered,lp,rp,ll,rl,valid;
        assert(sscanf(out,"CSV %llu,%ld,%ld,%u,%u,%u,%u,%u,%ld,%ld,%ld,%ld,%ld,%ld,%u,%u,%u",
                      &t,&l,&r,&lb,&rb,&powered,&lp,&rp,&ls,&rs,&lt,&rt,&li,&ri,&ll,&rl,&valid)==17);
        assert(lp<=MAX_DUTY && rp<=MAX_DUTY && valid<=1 && powered<=1);
        if(valid) {
            assert(powered && ls>=0 && rs>=0 && ls<=MAX_MEASURED_CPS && rs<=MAX_MEASURED_CPS);
            assert(lt>=1080 && lt<=1200 && rt>=1080 && rt<=1200);
            assert(labs(li)<=INTEGRAL_LIMIT_MILLI && labs(ri)<=INTEGRAL_LIMIT_MILLI);
        }
    }
    if(!strncmp(out,"RESULT ",7)){
        char *p=strstr(out,"reason=");assert(p);sscanf(p,"reason=%39s",result_reason);
    }
    if(!strncmp(out,"END ",4))longjmp(done,1);
    return n;
}
void pwm_set_both_levels(uint slice,uint16_t a,uint16_t b){
    assert(a<=200 && b<=200);
    if(slice==4){
        assert(a==0);
        if(b && !ld){++starts;on_at=clock_us;assert(wd);}
        if(!b && ld)off_at=clock_us;
        ld=b;if(b && b<min_left)min_left=b;
    }else if(slice==5){assert(b==0);rd=a;if(a && a<min_right)min_right=a;}
    else assert(0);
}
void watchdog_enable(uint32_t delay,bool debug){assert(delay==100 && !debug);wd=true;last_feed=clock_us;}
void watchdog_update(void){assert(wd);last_feed=clock_us;}
void watchdog_disable(void){assert(!ld && !rd);wd=false;}
uint32_t save_and_disable_interrupts(void){unsigned old=mask;mask=1;return old;}
void restore_interrupts(uint32_t value){mask=value;}
void gpio_init(uint p){(void)p;}void gpio_put(uint p,bool v){(void)p;(void)v;}
void gpio_set_dir(uint p,bool v){(void)p;(void)v;}
void gpio_disable_pulls(uint p){(void)p;}void gpio_set_function(uint p,uint f){(void)p;(void)f;}
uint32_t gpio_get_all(void){return fake_gpio;}
void gpio_add_raw_irq_handler_masked(uint32_t m,void (*h)(void)){(void)m;(void)h;}
void gpio_set_irq_enabled(uint p,uint32_t e,bool b){(void)p;(void)e;(void)b;}
void irq_set_enabled(uint p,bool b){(void)p;(void)b;}
void stdio_init_all(void){}
uint32_t clock_get_hz(uint c){(void)c;return 125000000;}
uint pwm_gpio_to_slice_num(uint p){return p/2;}
pwm_config pwm_get_default_config(void){return (pwm_config){0};}
void pwm_config_set_clkdiv(pwm_config *c,float f){(void)c;(void)f;}
void pwm_config_set_wrap(pwm_config *c,uint16_t v){(void)c;(void)v;}
void pwm_init(uint n,const pwm_config *c,bool b){(void)n;(void)c;(void)b;}
void pwm_set_enabled(uint n,bool b){(void)n;(void)b;}
int main(void){
    const char *expected[]={"complete","disconnect","requested","stall","encoder_reverse","encoder_invalid","travel_limit","loop_late","idle","speed_input","speed_input","stall","encoder_reverse"};
    for(scenario=0;scenario<13;++scenario){
        clock_us=on_at=off_at=last_feed=0;starts=mask=ld=rd=0;
        min_left=min_right=200;left_count=left_bad=right_bad=0;
        right_count=UINT32_MAX-10u; /* exercise forward raw-count wrap */
        lfraction=rfraction=0;run_sent=extra_sent=stop_sent=wd=false;result_reason[0]=0;
        if(setjmp(done)==0)bench_main();
        assert(!ld && !rd && !wd);
        assert(!controller.valid && !controller.left.integral_milli && !controller.right.integral_milli);
        if(scenario==8){assert(starts==0);continue;}
        assert(starts==1 && !strcmp(result_reason,expected[scenario]));
        assert(off_at-on_at<=2000000u && !sample_overflow);
        assert(sample_count<SAMPLE_CAPACITY && clock_us>=off_at+SETTLE_US);
        if(scenario==0){
            assert(off_at-on_at==2000000u && min_left<200 && min_left>=140 && min_right>=140 && min_right<200);
            int32_t mismatch=-motion_count_delta(samples[sample_count-1].left,origin.left)-motion_count_delta(samples[sample_count-1].right,origin.right);
            /* Uncontrolled model mismatch would be 440 counts over 2000ms. */
            printf("End-of-run model mismatch: %ld counts\n",(long)mismatch);fflush(stdout);
            /* Speed PI may leave either wheel slightly ahead; bound magnitude. */
            assert(mismatch>-100 && mismatch<100);
        }
        printf("scenario %u: %s PASS\n",scenario,result_reason);
    }
    puts("PASS: actual loop deadlines, faults, no auto/queued runs, PWM polarity/bounds, watchdog arming, no active printing, feedback direction and convergence");
}

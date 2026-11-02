/* Deterministic host model of the actual bench main loop; no hardware access. */
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
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
static uint64_t clock_us,on_at,off_at;
static unsigned scenario,starts,commands,mask;
static bool left_on,right_on,run_sent,extra_sent,stop_sent;
static uint32_t fake_gpio;
uint64_t time_us_64(void){return clock_us;}
void sleep_ms(uint ms){clock_us+=ms*1000u;assert(clock_us<4000000u);}
bool stdio_usb_connected(void){return !(scenario==1 && clock_us>=500000 && clock_us<800000);}
int getchar_timeout_us(uint timeout){
    (void)timeout;
    if(clock_us>=10000 && !run_sent){run_sent=true;++commands;return 'r';}
    if(clock_us>=100000 && !extra_sent){extra_sent=true;return 'l';} /* must be ignored */
    if(scenario==2 && clock_us>=500000 && !stop_sent){stop_sent=true;return 's';}
    return PICO_ERROR_TIMEOUT;
}
static int bench_printf(const char *format,...){
    assert(!left_on && !right_on); /* printing cannot delay an energised pulse */
    char out[512];va_list ap;va_start(ap,format);int n=vsnprintf(out,sizeof(out),format,ap);va_end(ap);
    if(!strncmp(out,"END ",4)) longjmp(done,1);
    return n;
}
void pwm_set_both_levels(uint slice,uint16_t a,uint16_t b){
    bool active=a||b;
    if(slice==4){left_on=active;assert(!active);} /* no left movement in a right test */
    if(slice==5){
        if(active && !right_on){++starts;on_at=clock_us;assert(a==200 && b==0);}
        if(!active && right_on)off_at=clock_us;
        right_on=active;
    }
}
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
    for(scenario=0;scenario<3;++scenario){
        clock_us=on_at=off_at=0;starts=commands=mask=0;
        left_on=right_on=run_sent=extra_sent=stop_sent=false;
        if(setjmp(done)==0)bench_main();
        assert(starts==1 && commands==1 && !left_on && !right_on);
        assert(off_at-on_at==(scenario==0?2000000u:490000u));
        assert(!sample_overflow && sample_count<SAMPLE_CAPACITY);
        assert(clock_us>=off_at+SETTLE_US);
    }
    uint8_t previous=0;volatile uint32_t count=0,bad=0;
    /* Positive quadrature cycle on right GP2/3, four real edges. */
    count_pair(1u<<2,8u<<8,2,3,&previous,&count,&bad);
    count_pair((1u<<2)|(1u<<3),8u<<12,2,3,&previous,&count,&bad);
    count_pair(1u<<3,4u<<8,2,3,&previous,&count,&bad);
    count_pair(0,4u<<12,2,3,&previous,&count,&bad);
    assert(count==4 && bad==0);
    count_pair((1u<<2)|(1u<<3),(8u<<8)|(8u<<12),2,3,&previous,&count,&bad);
    assert(count==4 && bad==1); /* ambiguous edges rejected */
    previous=0;count=0;bad=0;
    count_pair(1u<<3,8u<<12,2,3,&previous,&count,&bad);
    assert(count==UINT32_MAX && motion_count_delta(count,0)==-1);
    puts("Encoder bench: one wheel only, deadline/disconnect/stop, no active printing, queued request rejection, buffer bounds and quadrature: PASS");
}

/* Deterministic illustrative plants, not fitted hardware or stability proof. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controller.h"

static void invalid_inputs(void)
{
    speed_control_t c,old;
    speed_control_start(&c);old=c;
    assert(!speed_control_step(&c,0,0,0));
    assert(!speed_control_step(&c,0,0,19999));
    assert(!speed_control_step(&c,0,0,60001));
    assert(!speed_control_step(&c,-1,0,20000));
    assert(!speed_control_step(&c,0,-1,20000));
    assert(!speed_control_step(&c,201,0,20000));
    assert(!speed_control_step(&c,0,201,20000));
    assert(!speed_control_step(&c,INT32_MAX,INT32_MAX,20000));
    assert(!memcmp(&c,&old,sizeof(c))); /* rejected sample is atomic */
    assert(speed_control_step(&c,24,24,20000));
    int32_t speed=c.left.filtered_milli_cps;
    assert(speed_control_step(&c,60,60,30000));
    assert(c.left.filtered_milli_cps>speed);
    for(unsigned i=0;i<100;++i) {
        assert(speed_control_step(&c,84+24*(int)i,84+24*(int)i,20000));
    }
    assert(c.left.filtered_milli_cps/1000>=1199);
    assert(c.left.filtered_milli_cps==c.right.filtered_milli_cps);
    speed_control_reset(&c);
    assert(!c.valid && !c.left.duty && !c.right.duty && !c.left.integral_milli);
    speed_control_start(&c);
    assert(c.left.duty==200 && c.right.duty==200 && !c.left.integral_milli);
    assert(c.left.feedforward==155 && c.right.feedforward==185);
}

static void saturation_recovery(void)
{
    wheel_speed_t w={.duty=200,.feedforward=180,.minimum=MIN_DUTY};
    for(unsigned i=0;i<1000;++i)speed_wheel_step(&w,0,1200,20000);
    assert(w.duty==MAX_DUTY && w.integral_milli==0);
    assert(w.limits&SPEED_LIMIT_HIGH);
    /* Transition to a fast wheel must promptly unwind, not retain wind-up. */
    for(unsigned i=0;i<100;++i)speed_wheel_step(&w,4000,1200,20000);
    assert(w.duty==MIN_DUTY && w.integral_milli>=-INTEGRAL_LIMIT_MILLI);
    assert(w.limits&SPEED_LIMIT_LOW);
    for(unsigned i=0;i<100;++i)speed_wheel_step(&w,0,1200,20000);
    assert(w.duty==MAX_DUTY && w.integral_milli<=INTEGRAL_LIMIT_MILLI);
}

static void plant(unsigned scenario)
{
    speed_control_t c;
    speed_control_start(&c);
    double lpos=0,rpos=0,lv=0,rv=0;
    double last_l=0,last_r=0,tail_sum_l=0,tail_sum_r=0;
    double largest_tail_difference=0;
    unsigned tail_n=0;
    bool checked_two_seconds=false;
    uint64_t elapsed=0;
    /* 30 seconds checks long-run drift, while firmware stays capped at 2 s. */
    while(elapsed<30000000u) {
        uint32_t dt=scenario==3 ? (elapsed%3==0 ? 30000u : 21000u) : 20000u;
        double seconds=dt/1000000.0;
        double lg=14,rg=12,ldead=90,rdead=90,tau=0.08;
        if(scenario==1 && elapsed>=1000000u)lg=16; /* faster left after 1 s */
        if(scenario==2 && elapsed>=1000000u)rg=11.2; /* added right load */
        if(scenario==3){lg=12;rg=14;tau=0.15;} /* mirrored asymmetry/jitter */
        if(scenario==4){lg=13.5;rg=12.5;ldead=95;rdead=92;tau=0.20;}
        double desired_l=fmax(0,(double)c.left.duty-ldead)*lg;
        double desired_r=fmax(0,(double)c.right.duty-rdead)*rg;
        lv+=(desired_l-lv)*seconds/(tau+seconds);
        rv+=(desired_r-rv)*seconds/(tau+seconds);
        lpos+=lv*seconds;rpos+=rv*seconds;elapsed+=dt;
        unsigned old_l=c.left.duty,old_r=c.right.duty;
        assert(speed_control_step(&c,(int32_t)lpos,(int32_t)rpos,dt));
        unsigned slew=DUTY_SLEW_PER_SECOND*dt/1000000u;
        assert(c.left.duty>=MIN_DUTY && c.left.duty<=MAX_DUTY);
        assert(c.right.duty>=MIN_DUTY && c.right.duty<=MAX_DUTY);
        assert(abs((int)c.left.duty-(int)old_l)<=(int)slew);
        assert(abs((int)c.right.duty-(int)old_r)<=(int)slew);
        assert(abs(c.left.integral_milli)<=INTEGRAL_LIMIT_MILLI);
        assert(abs(c.right.integral_milli)<=INTEGRAL_LIMIT_MILLI);
        assert(c.left.target_cps<=TARGET_CPS && c.right.target_cps<=TARGET_CPS);
        if(!checked_two_seconds && elapsed>=2000000u) {
            checked_two_seconds=true;
            printf("plant %u at 2 s: speed %.1f/%.1f cps, position lead %.1f\n",scenario,lv,rv,lpos-rpos);
            assert(fabs(lpos-rpos)<100);
            assert(lv<1600 && rv<1600);
        }
        if(elapsed>=25000000u) {
            if(!tail_n){last_l=lpos;last_r=rpos;}
            tail_sum_l+=lv;tail_sum_r+=rv;++tail_n;
            if(fabs(lv-rv)>largest_tail_difference)largest_tail_difference=fabs(lv-rv);
        }
    }
    double mean_l=tail_sum_l/tail_n,mean_r=tail_sum_r/tail_n;
    double extra_mismatch=(lpos-last_l)-(rpos-last_r);
    printf("plant %u: mean speed %.1f/%.1f cps, position lead %.1f, last-5s added mismatch %.1f, peak tail speed difference %.1f\n",
           scenario,mean_l,mean_r,lpos-rpos,extra_mismatch,largest_tail_difference);
    assert(fabs(mean_l-TARGET_CPS)<35 && fabs(mean_r-TARGET_CPS)<35);
    assert(fabs(lpos-rpos)<25 && fabs(extra_mismatch)<10);
    assert(largest_tail_difference<80);
}
int main(void)
{
    invalid_inputs();saturation_recovery();
    for(unsigned i=0;i<5;++i)plant(i);
    puts("PASS: speed/time conversion, reset, saturation recovery, slew/bounds and 30-second asymmetric/load-change models");
}

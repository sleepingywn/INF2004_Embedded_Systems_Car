/* Low-speed line regression plus illustrative plants, not hardware calibration. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../app_program/subsystem2/line_follow.h"

static void rounding_recovery(void)
{
    /* Captured in the recovery regression: old all-or-nothing integration
     * left this wheel permanently at duty 52 despite a 900 count/s deficit. */
    wheel_speed_t w={.duty=52,.feedforward=155,.integral_milli=-129130,
                     .integral_floor_milli=-155000};
    speed_wheel_step(&w,0,900,20000);
    assert(w.duty==57 && w.integral_milli>-129130);
    for(unsigned i=0;i<100;++i) {
        unsigned old=w.duty;
        speed_wheel_step(&w,0,900,20000);
        assert(w.duty>=old && w.duty-old<=5 && w.duty<=MAX_DUTY);
    }
    assert(w.duty==MAX_DUTY);
}

static void overspeed_regression(void)
{
    line_follow_t f;
    speed_control_t legacy;
    line_follow_begin(&f,0,0,0,0,0);
    speed_control_start(&legacy);
    /* Replay the observed speed plateau. Fixed measurements intentionally do
     * not respond to PWM: persistent overspeed must keep reducing power. */
    for(unsigned i=0;i<100;++i) {
        unsigned old_l=f.speed.left.duty,old_r=f.speed.right.duty;
        speed_wheel_step(&f.speed.left,865,400,20000);
        speed_wheel_step(&f.speed.right,1200,780,20000);
        speed_wheel_step(&legacy.left,865,400,20000);
        speed_wheel_step(&legacy.right,1200,780,20000);
        int dl=(int)f.speed.left.duty-(int)old_l,dr=(int)f.speed.right.duty-(int)old_r;
        assert(dl>=-10 && dl<=5 && dr>=-10 && dr<=5);
    }
    assert(f.speed.left.duty<90 && f.speed.right.duty<120);
    assert(f.speed.left.integral_milli < -40000);
    assert(f.speed.right.integral_milli < -40000);
    assert(legacy.left.integral_milli>=-INTEGRAL_LIMIT_MILLI);
    assert(legacy.right.integral_milli>=-INTEGRAL_LIMIT_MILLI);
    assert(legacy.left.duty>=MIN_DUTY && legacy.right.duty>=MIN_DUTY);
    /* Prolonged impossible feedback cannot wind past available power. */
    for(unsigned i=0;i<1000;++i) {
        speed_wheel_step(&f.speed.left,4000,300,20000);
        speed_wheel_step(&f.speed.right,4000,300,20000);
    }
    assert(f.speed.left.duty==0 && f.speed.right.duty==0);
    assert(f.speed.left.integral_milli>=-(int32_t)LEFT_FEEDFORWARD*1000);
    assert(f.speed.right.integral_milli>=-(int32_t)RIGHT_FEEDFORWARD*1000);
    for(unsigned i=0;i<200;++i) {
        speed_wheel_step(&f.speed.left,0,900,20000);
        speed_wheel_step(&f.speed.right,0,900,20000);
    }
    assert(f.speed.left.duty==MAX_DUTY && f.speed.right.duty==MAX_DUTY);
    assert(f.speed.left.integral_milli<=INTEGRAL_LIMIT_MILLI);
    assert(f.speed.right.integral_milli<=INTEGRAL_LIMIT_MILLI);
    speed_wheel_step(&f.speed.left,900,0,20000);
    speed_wheel_step(&f.speed.right,900,0,20000);
    assert(!f.speed.left.duty && !f.speed.right.duty);
    assert(!f.speed.left.integral_milli && !f.speed.right.integral_milli);
    line_follow_begin(&f,0,0,0,0,0);
    assert(!f.speed.left.integral_milli && !f.speed.right.integral_milli);
}

static void plant(unsigned scenario)
{
    /* Several independent, illustrative gain/deadband/load assumptions.
     * These cannot predict floor performance or validate a motor model. */
    const int errors[]={0,80,-80,60,-60,0}; /* sustained rolling-mode response */
    line_follow_t f;
    line_msg_t line={.state=LINE_ON};
    line_follow_begin(&f,0,0,0,0,0);
    double lv=0,rv=0,lp=0,rp=0;
    uint32_t now=0;
    unsigned steps=0;
    for(unsigned phase=0;phase<6;++phase) {
        uint32_t phase_start=now;
        double sum_l=0,sum_r=0;unsigned samples=0;
        line.err=errors[phase];
        int32_t target_l,target_r;
        assert(line_follow_targets(line.err,&target_l,&target_r));
        while(now-phase_start<4000u) {
            uint32_t dt=scenario==2 ? 20u+20u*(steps++%3u) : 20u;
            double seconds=dt/1000.0;
            double lg=scenario==0 ? 12:14,rg=scenario==0 ? 14:12;
            double ld=scenario==0 ? 30:90,rd=scenario==0 ? 45:90;
            double tau=scenario==2 ? .15:.08;
            /* A changed load after returning from a full turn. */
            if(scenario==2 && phase>=4){lg=12;rg=14;}
            lv+=(fmax(0,(double)f.speed.left.duty-ld)*lg-lv)*seconds/(tau+seconds);
            rv+=(fmax(0,(double)f.speed.right.duty-rd)*rg-rv)*seconds/(tau+seconds);
            lp+=lv*seconds;rp+=rv*seconds;now+=dt;
            unsigned old_l=f.speed.left.duty,old_r=f.speed.right.duty;
            assert(line_follow_step(&f,0u-(uint32_t)lp,(uint32_t)rp,0,now,now,
                                    &line,now,7,7,true,true)==LINE_STEP_OK);
            int up=(int)(DUTY_SLEW_PER_SECOND*dt/1000u),down=(int)(LINE_DECEL_PER_SECOND*dt/1000u);
            int dl=(int)f.speed.left.duty-(int)old_l,dr=(int)f.speed.right.duty-(int)old_r;
            assert(dl>=-down && dl<=up && dr>=-down && dr<=up);
            assert(f.speed.left.duty<=MAX_DUTY && f.speed.right.duty<=MAX_DUTY);
            assert(f.speed.left.integral_milli>=-(int32_t)LEFT_FEEDFORWARD*1000);
            assert(f.speed.right.integral_milli>=-(int32_t)RIGHT_FEEDFORWARD*1000);
            assert(f.speed.left.integral_milli<=INTEGRAL_LIMIT_MILLI);
            assert(f.speed.right.integral_milli<=INTEGRAL_LIMIT_MILLI);
            if(now-phase_start>=3500u){sum_l+=lv;sum_r+=rv;++samples;}
        }
        double mean_l=sum_l/samples,mean_r=sum_r/samples;
        printf("plant %u error %+d: target %d/%d, tail speed %.1f/%.1f cps\n",
               scenario,errors[phase],target_l,target_r,mean_l,mean_r);
        assert(fabs(mean_l-target_l)<50 && fabs(mean_r-target_r)<50);
    }
}
int main(void)
{
    rounding_recovery();overspeed_regression();
    for(unsigned i=0;i<3;++i)plant(i);
    puts("PASS: line low-speed authority, unchanged straight limits, saturation/recovery, smooth curves and illustrative loads");
}

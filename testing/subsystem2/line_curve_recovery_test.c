/* Regression from the 23:03 video: late wheel separation, then straight grace.
 * Host checks verify commands/guards; illustrative plants cannot prove grip. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static line_step_t step(line_follow_t *f,int err,uint32_t t,uint32_t start)
{
    uint32_t counts=(t-start)/2;
    line_msg_t line={.state=LINE_ON,.err=err};
    return line_follow_step(f,0u-counts,counts,0,t,t,&line,t,1,1,true,true);
}
static void curve_gap(int sign,uint32_t start)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,start,start);
    for(unsigned dt=20;dt<=200;dt+=20)assert(step(&f,40*sign,start+dt,start)==LINE_STEP_OK);
    assert(!f.tight.direction);
    for(unsigned dt=220;dt<520;dt+=20) {
        assert(step(&f,0,start+dt,start)==LINE_STEP_OK);
        assert(f.search.curve_grace && !f.search.active);
        assert(sign>0 ? f.speed.left.target_cps<f.speed.right.target_cps:
                       f.speed.right.target_cps<f.speed.left.target_cps);
        assert(f.search.curve_ms==start+200); /* never refresh on synthetic input */
    }
    assert(step(&f,0,start+520,start)==LINE_STEP_OK && f.search.active);
    for(unsigned dt=540;dt<2020;dt+=20)assert(step(&f,0,start+dt,start)==LINE_STEP_OK);
    assert(step(&f,0,start+2020,start)==LINE_STEP_SEARCH_TIMEOUT);

    line_follow_begin(&f,0,0,0,start,start);
    assert(step(&f,60*sign,start+20,start)==LINE_STEP_OK);
    assert(step(&f,0,start+40,start)==LINE_STEP_OK && f.search.curve_grace);
    assert(step(&f,-sign,start+60,start)==LINE_STEP_OK);
    assert(f.steering_error==-sign && !f.search.curve_direction);
    assert(step(&f,0,start+80,start)==LINE_STEP_OK && f.steering_error==0);
    /* Opposite input cancels old memory without renewing the grace deadline. */
    for(unsigned dt=100;dt<=320;dt+=20)assert(step(&f,0,start+dt,start)==LINE_STEP_OK);
    assert(step(&f,0,start+340,start)==LINE_STEP_OK && f.search.active);
    line_follow_begin(&f,0,0,0,start,start);
    assert(!f.search.curve_direction && !f.search.curve_grace);
}
static void stale_bend(void)
{
    line_search_t s={0};int32_t l,r;
    line_search_step(&s,60,false,false,100,0,0,&l,&r);
    /* Weak same-side evidence keeps tracking, but cannot refresh bend memory. */
    line_search_step(&s,21,false,false,400,0,0,&l,&r);
    assert(line_search_step(&s,0,false,false,420,0,0,&l,&r)==LINE_SEARCH_TRACK);
    assert(!s.curve_grace);
    assert(line_search_step(&s,0,false,false,1019,0,0,&l,&r)==LINE_SEARCH_TRACK);
    assert(line_search_step(&s,0,false,false,1020,0,0,&l,&r)==LINE_SEARCH_SWEEP);
}
static void commands(void)
{
    int32_t l,r;
    for(int e=0;e<=100;++e) {
        line_follow_targets(e,&l,&r);
        assert(l>=250 && r<=600 && l<=r);
        if(e>=60)assert(l==250 && r==600 && (l+r)/2<LINE_CRUISE_CPS);
        int32_t ml,mr;line_follow_targets(-e,&ml,&mr);assert(ml==r && mr==l);
    }
    assert(line_target_ramp(450,250,20)==370);
    assert(line_target_ramp(250,600,20)==290); /* acceleration stays limited */
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    for(unsigned side=0;side<2;++side) {
        wheel_speed_t w=side ? f.speed.right:f.speed.left;
        w.filtered_milli_cps=450000;w.target_cps=450;w.duty=85;
        w.integral_milli=(85-(int)w.feedforward)*1000;
        wheel_speed_t old=w;old.kp_milli=60;old.decel_per_second=500;
        speed_wheel_step(&w,450,250,20000);
        speed_wheel_step(&old,450,250,20000);
        assert(w.duty<old.duty && w.duty>=65);
        speed_wheel_step(&w,450,0,20000);assert(!w.duty);
    }
}
static double model(bool previous,unsigned variant,unsigned side)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    wheel_speed_t w=side ? f.speed.right:f.speed.left;
    if(previous){w.kp_milli=60;w.decel_per_second=500;}
    double gain=variant==0 ? 12:14,dead=variant==0 ? 30:70;
    double tau=variant==2 ? .15:.08,speed=450,area=0;
    w.duty=(unsigned)(dead+450/gain);w.target_cps=450;
    w.filtered_milli_cps=450000;
    w.integral_milli=((int)w.duty-(int)w.feedforward)*1000;
    for(unsigned t=20;t<=1500;t+=20) {
        speed_wheel_step(&w,(int32_t)speed,250,20000);
        speed+=(fmax(0,(double)w.duty-dead)*gain-speed)*.02/(tau+.02);
        if(t<=400)area+=fabs(speed-250)*.02;
        assert(w.duty<=200);
    }
    assert(fabs(speed-250)<50);
    return area;
}
static void repeated_turns(unsigned variant)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    double lv=0,rv=0,lp=0,rp=0;
    uint32_t now=0;unsigned iteration=0;
    for(unsigned phase=0;phase<8;++phase) {
        uint32_t begin=now;
        while(now-begin<1800) {
            uint32_t dt=variant==2 ? 20+20*(iteration++%3):20;
            double sec=dt/1000.0,tau=variant==2 ? .15:.08;
            double dead=variant==0 ? 30:90;
            double lg=phase<4 ? 12:14,rg=phase<4 ? 14:12;
            lv+=(fmax(0,(double)f.speed.left.duty-dead)*lg-lv)*sec/(tau+sec);
            rv+=(fmax(0,(double)f.speed.right.duty-dead)*rg-rv)*sec/(tau+sec);
            lp+=lv*sec;rp+=rv*sec;now+=dt;
            /* Include opposite bends, short ambiguous gaps and a load change. */
            /* Rolling-only case: sustained stronger readings now deliberately
             * enter the bounded pause, exercised in line_curve_trigger_test. */
            line_msg_t line={.state=LINE_ON,.err=phase%2 ? -54:54};
            if(phase && now-begin<=100)line.err=0;
            unsigned ld=f.speed.left.duty,rd=f.speed.right.duty;
            line_step_t result=line_follow_step(&f,0u-(uint32_t)lp,(uint32_t)rp,0,now,now,
                                               &line,now,1,1,true,true);
            if(result!=LINE_STEP_OK)fprintf(stderr,"plant %u phase %u time %u step %u speed %.1f/%.1f duty %u/%u\n",
                variant,phase,now,result,lv,rv,f.speed.left.duty,f.speed.right.duty);
            assert(result==LINE_STEP_OK);
            assert(f.speed.left.duty<=ld+250*dt/1000 && f.speed.right.duty<=rd+250*dt/1000);
            assert(f.speed.left.duty<=200 && f.speed.right.duty<=200);
            assert(!f.tight.direction && !f.search.active);
        }
        double inside=phase%2 ? rv:lv,outside=phase%2 ? lv:rv;
        printf("Repeated-turn plant %u phase %u: %.1f / %.1f cps\n",variant,phase,inside,outside);
        assert(fabs(inside-284)<70 && fabs(outside-585)<70 && inside<outside*.6);
    }
}
int main(void)
{
    commands();stale_bend();
    for(int sign=-1;sign<=1;sign+=2){curve_gap(sign,0);curve_gap(sign,UINT32_MAX-300u);}
    for(unsigned v=0;v<3;++v)for(unsigned s=0;s<2;++s) {
        double before=model(true,v,s),after=model(false,v,s);
        printf("Illustrative plant %u side %u: first-400ms speed-error area %.2f -> %.2f\n",v,s,before,after);
        assert(after<before);
    }
    for(unsigned v=0;v<3;++v)repeated_turns(v);
    puts("PASS: earlier stronger curves, faster deceleration, bounded gap continuation, weak opposite priority, reset/wrap, timeout and illustrative response");
}

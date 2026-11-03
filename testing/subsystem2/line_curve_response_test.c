/* Illustrative response tests; these do not predict a calibrated turn radius. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../app_program/subsystem2/line_follow.h"

static void exit_and_direction(void)
{
    line_follow_t f;
    int32_t l,r;
    line_follow_begin(&f,0,0,0,0,0);
    assert(line_follow_targets(0,&l,&r));line_curve_exit_limit(&f,0,100,&l,&r);
    assert(l==700 && r==700); /* no artificial slowdown before any curve */
    assert(line_follow_targets(60,&l,&r) && l==250 && r==750);
    line_curve_exit_limit(&f,60,100,&l,&r);
    assert(l==250 && r==750);
    assert(line_follow_targets(0,&l,&r));line_curve_exit_limit(&f,0,120,&l,&r);
    assert(l==400 && r==400); /* white/centred gap must not restore cruise */
    assert(line_follow_targets(-20,&l,&r));line_curve_exit_limit(&f,-20,200,&l,&r);
    assert(l==400 && r<l); /* current opposite error always wins */
    assert(line_follow_targets(0,&l,&r));line_curve_exit_limit(&f,0,1099,&l,&r);
    assert(l==400 && r==400);
    assert(line_follow_targets(0,&l,&r));line_curve_exit_limit(&f,0,1100,&l,&r);
    assert(l==700 && r==700);
    assert(line_follow_targets(-60,&l,&r));line_curve_exit_limit(&f,-60,1120,&l,&r);
    assert(l==750 && r==250 && f.last_curve_ms==1120);
    assert(line_follow_targets(0,&l,&r));line_curve_exit_limit(&f,0,2119,&l,&r);
    assert(l==400 && r==400);
    line_follow_begin(&f,0,0,0,0,0);
    assert(!f.curve_seen);
    uint32_t start=UINT32_MAX-10u;
    line_follow_targets(60,&l,&r);line_curve_exit_limit(&f,60,start,&l,&r);
    line_follow_targets(0,&l,&r);line_curve_exit_limit(&f,0,start+999u,&l,&r);
    assert(l==400 && r==400);
    line_follow_targets(0,&l,&r);line_curve_exit_limit(&f,0,start+1000u,&l,&r);
    assert(l==700 && r==700);
    /* Increasing error gives a tighter ratio in either direction, without
     * the old zero-target restart discontinuity. Full response starts at 60. */
    for(int e=1;e<=100;++e) {
        line_follow_targets(e,&l,&r);assert(l>0 && l<r);
        if(e>=60)assert(l==250 && r==750);
        int32_t ml,mr;line_follow_targets(-e,&ml,&mr);
        assert(ml==r && mr==l);
    }
}

static double response(unsigned side,bool old,unsigned model)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    wheel_speed_t w=side ? f.speed.right:f.speed.left;
    if(old){w.kp_milli=w.ki_milli=0;w.decel_per_second=250;}
    double speed=0,gain=model==0 ? 12:14,dead=model==0 ? 30:90;
    double tau=model==2 ? .15:.08;
    for(unsigned t=20;t<=4000;t+=20) {
        speed+=(fmax(0,(double)w.duty-dead)*gain-speed)*.02/(tau+.02);
        speed_wheel_step(&w,(int32_t)speed,t<=3000 ? 700:250,20000);
        if(t==3600)return fabs(speed-250);
    }
    assert(0);return 0;
}
static void bounded_steering_memory(void)
{
    line_follow_t f;line_msg_t line={.state=LINE_ON};int32_t l,r;
    line_follow_begin(&f,0,0,0,0,0);
    assert(line_steering_error(&f,&line,100)==0); /* straight startup */
    for(int sign=-1;sign<=1;sign+=2) {
        uint32_t start=UINT32_MAX-100u;
        line_follow_begin(&f,0,0,0,start,start);
        line.err=60*sign;
        assert(line_steering_error(&f,&line,start)==60*sign);
        line_follow_targets(line.err,&l,&r);line_curve_exit_limit(&f,line.err,start,&l,&r);
        line.err=0;
        int previous=LINE_STEERING_HOLD_ERROR;
        for(uint32_t dt=20;dt<=1500;dt+=20) {
            int error=line_steering_error(&f,&line,start+dt);
            assert(error*sign>=0 && error*sign<=previous);
            previous=error*sign;
            assert(f.last_steering_ms==start); /* zero cannot renew itself */
            if(dt>=500)assert(error==0);
            line_follow_targets(error,&l,&r);line_curve_exit_limit(&f,0,start+dt,&l,&r);
            if(dt<1000)assert(l<=400 && r<=400 && l>=250 && r>=250);
            else assert(l==700 && r==700);
            if(error)assert(sign>0 ? l<r:r<l);
        }
        line.err=60*sign;assert(line_steering_error(&f,&line,2000)==60*sign);
        line.err=-sign;assert(line_steering_error(&f,&line,2020)==-sign);
        line.err=0;assert(line_steering_error(&f,&line,2040)==0); /* weak opposite cancels */
        line.err=-60*sign;assert(line_steering_error(&f,&line,2060)==-60*sign);
        line.err=0;assert(line_steering_error(&f,&line,2080)*sign<0); /* new strong direction */
        line.state=LINE_EDGE_ONLY;assert(line_steering_error(&f,&line,2100)==0);
        line.state=LINE_ON;
        line_follow_begin(&f,0,0,0,0,0);
        assert(line_steering_error(&f,&line,2120)==0);
    }
    puts("PASS: fading steering memory, no renewal on zero, opposite priority, clock wrap and reset");
}
static void repeated_curves(unsigned model)
{
    line_follow_t f;line_msg_t line={.state=LINE_ON};
    line_follow_begin(&f,0,0,0,0,0);
    double lv=0,rv=0,lp=0,rp=0;
    uint32_t now=0;unsigned iteration=0;
    for(unsigned phase=0;phase<9;++phase) {
        uint32_t duration=phase ? 1200:3000,start=now;
        double last_l=0,last_r=0;
        while(now-start<duration) {
            uint32_t dt=model==2 ? 20+20*(iteration++%3):20;
            double seconds=dt/1000.0;
            double lg=model==0 ? 12:14,rg=model==0 ? 14:12;
            double ld=model==0 ? 30:90,rd=model==0 ? 45:90,tau=model==2 ? .15:.08;
            if(model==2 && phase>=5){lg=12;rg=14;}
            lv+=(fmax(0,(double)f.speed.left.duty-ld)*lg-lv)*seconds/(tau+seconds);
            rv+=(fmax(0,(double)f.speed.right.duty-rd)*rg-rv)*seconds/(tau+seconds);
            lp+=lv*seconds;rp+=rv*seconds;now+=dt;
            /* Brief centred gaps between alternating left/right curves. */
            line.err=(!phase || now-start<=100) ? 0 : (phase%2 ? 60:-60);
            unsigned old_l=f.speed.left.duty,old_r=f.speed.right.duty;
            int32_t old_lt=f.speed.left.target_cps,old_rt=f.speed.right.target_cps;
            assert(line_follow_step(&f,0u-(uint32_t)lp,(uint32_t)rp,0,now,now,
                                    &line,now,7,7,true,true)==LINE_STEP_OK);
            int dl=(int)f.speed.left.duty-(int)old_l,dr=(int)f.speed.right.duty-(int)old_r;
            assert(dl>=-(int)(500*dt/1000) && dl<=(int)(250*dt/1000));
            assert(dr>=-(int)(500*dt/1000) && dr<=(int)(250*dt/1000));
            assert(abs(f.speed.left.target_cps-old_lt)<=(int)(4000*dt/1000));
            assert(abs(f.speed.right.target_cps-old_rt)<=(int)(4000*dt/1000));
            assert(f.speed.left.duty<=200 && f.speed.right.duty<=200);
            assert(f.speed.left.target_cps>0 && f.speed.right.target_cps>0);
            last_l=lv;last_r=rv;
        }
        if(phase) {
            double inner=phase%2 ? last_l:last_r,outer=phase%2 ? last_r:last_l;
            printf("model %u curve %u: inner %.1f outer %.1f cps\n",model,phase,inner,outer);
            assert(inner<outer*.5); /* substantial differential before next curve */
            assert(fabs(inner-250)<65 && fabs(outer-750)<65);
        }
    }
    /* A lost-line report still stops the controller immediately. */
    line.state=LINE_LOST;
    assert(line_follow_step(&f,0u-(uint32_t)lp,(uint32_t)rp,0,now,now,
                           &line,now,7,7,true,true)==LINE_STEP_IR);
}
int main(void)
{
    exit_and_direction();
    bounded_steering_memory();
    for(unsigned model=0;model<3;++model) {
        for(unsigned side=0;side<2;++side) {
            double old=response(side,true,model),current=response(side,false,model);
            printf("model %u side %u speed error at 600ms: previous %.1f, current %.1f cps\n",model,side,old,current);
            assert(current<old && current<120);
        }
        repeated_curves(model);
    }
    puts("PASS: earlier tighter rolling turns, exit hold/wrap/reset, faster model response and repeated opposite curves");
    return 0;
}

#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static line_step_t step(line_follow_t *f,int32_t l,int32_t r,int err,uint32_t t)
{
    line_msg_t line={.state=LINE_ON,.err=err};
    return line_follow_step(f,0u-(uint32_t)l,(uint32_t)r,0,t,t,&line,t,1,1,true,true);
}
static void paused_restart(int sign)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    int32_t l=0,r=0;
    for(unsigned t=20;t<=140;t+=20) {
        l+=10;r+=10;
        assert(step(&f,l,r,sign*100,t)==LINE_STEP_OK);
    }
    assert(f.tight.direction==sign);
    for(unsigned t=160;t<=240;t+=20) {
        if(sign>0)r+=10;else l+=10;
        assert(step(&f,l,r,0,t)==LINE_STEP_OK);
        assert(f.tight.direction==sign && !f.search.active);
        assert(sign>0 ? !f.speed.left.duty:!f.speed.right.duty);
    }
    if(sign>0)r+=10;else l+=10;
    assert(step(&f,l,r,0,260)==LINE_STEP_OK);
    assert(!f.tight.direction && !f.search.active && f.steering_error*sign>0);
    wheel_speed_t *inside=sign>0 ? &f.speed.left:&f.speed.right;
    assert(inside->duty>0 && inside->duty<=115 && inside->target_cps==40);
    for(unsigned t=280;t<=440;t+=20) {
        unsigned before=inside->duty;l+=7;r+=7;
        assert(step(&f,l,r,0,t)==LINE_STEP_OK);
        assert(!f.search.active && !f.tight.direction && f.steering_error*sign>0);
        assert((int)inside->duty-(int)before<=5 && (int)inside->duty-(int)before>=-20);
        if(t>=420)assert(sign>0 ? f.speed.left.target_cps<f.speed.right.target_cps:
                                                 f.speed.right.target_cps<f.speed.left.target_cps);
    }
    l+=7;r+=7;
    assert(step(&f,l,r,0,460)==LINE_STEP_OK && f.search.active);
    /* A later restart that genuinely cannot turn still hits the stall guard. */
    line_follow_begin(&f,0,0,0,0,0);l=r=0;
    for(unsigned t=20;t<=140;t+=20){l+=10;r+=10;assert(step(&f,l,r,sign*100,t)==LINE_STEP_OK);}
    for(unsigned t=160;t<460;t+=20) {
        if(sign>0)r+=10;else l+=10;
        assert(step(&f,l,r,0,t)==LINE_STEP_OK);
    }
    if(sign>0)r+=10;else l+=10;
    assert(step(&f,l,r,0,460)==LINE_STEP_STALL);
}
static void targets_and_seed(void)
{
    int32_t l,r;
    assert(line_follow_targets(0,&l,&r) && l==450 && r==450);
    assert(line_follow_targets(60,&l,&r) && l==250 && r==600);
    assert(line_follow_targets(100,&l,&r) && l==250 && r==600);
    assert(line_follow_targets(-100,&l,&r) && l==600 && r==250);
    for(unsigned prev=0;prev<=200;prev+=10)for(int target=40;target<=550;target+=170) {
        line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
        wheel_speed_t *w=&f.speed.left;
        speed_wheel_step(w,0,0,20000);
        line_soft_restart(w,target,prev);
        assert(w->duty>=80 && w->duty<=110);
        speed_wheel_step(w,0,target,20000);
        assert(w->duty<=115 && w->duty>0);
        assert(w->integral_milli>=w->integral_floor_milli);
        speed_wheel_step(w,0,0,20000);assert(!w->duty); /* STOP is immediate */
    }
}
int main(void)
{
    targets_and_seed();paused_restart(1);paused_restart(-1);
    puts("PASS: curve targets, both paused-wheel restarts capped, 100ms debounce, bounded curve continuation, slew, stall and STOP");
}

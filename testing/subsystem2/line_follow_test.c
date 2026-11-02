#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"
#include "../../app_program/subsystem3/sensor_line.h"

static line_msg_t centered(void){line_msg_t l={.state=LINE_ON};return l;}
static line_step_t step(line_follow_t *f,uint32_t left,uint32_t right,uint32_t bad,
                       uint32_t time,line_msg_t *line,uint32_t line_time)
{
    return line_follow_step(f,left,right,bad,time,time,line,line_time,7,7,true,true);
}
static void input_guards(void)
{
    line_msg_t l=centered();
    assert(line_follow_usable(&l,100,100,7,7,true));
    assert(!line_follow_usable(&l,100,100,7,8,true));
    assert(!line_follow_usable(&l,201,100,7,7,true));
    assert(!line_follow_usable(&l,100,101,7,7,true));
    assert(!line_follow_usable(&l,100,100,7,7,false));
    unsigned states[]={LINE_CALIBRATING,LINE_LOST_HOLD,LINE_LOST,LINE_BOTH_BLACK,LINE_FAULT,7};
    for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);++i){
        l.state=states[i];assert(!line_follow_usable(&l,100,100,7,7,true));
    }
    l=centered();l.fault=1;assert(!line_follow_usable(&l,100,100,7,7,true));
    l=centered();l.err=101;assert(!line_follow_usable(&l,100,100,7,7,true));
}
static void steering_and_sensors(void)
{
    /* Exercise Buddy 3's real normalisation/line-error convention. */
    ir_cal_t cal={.white=500,.black=3500};line_tracker_t tracker;line_state_t state;
    line_init(&tracker);
    int error=line_update(&tracker,ir_normalise(&cal,3500),ir_normalise(&cal,500),10,&state);
    int32_t lt,rt;
    assert(error==100 && line_follow_targets(error,&lt,&rt) && lt==0 && rt==1200);
    error=line_update(&tracker,0,100,10,&state);
    assert(error==-100 && line_follow_targets(error,&lt,&rt) && lt==1200 && rt==0);
    assert(line_follow_targets(50,&lt,&rt) && lt==600 && rt==1200);
    assert(line_follow_targets(-50,&lt,&rt) && lt==1200 && rt==600);
    assert(!line_follow_targets(101,&lt,&rt));

    line_follow_t f;line_msg_t l=centered();
    line_follow_begin(&f,0,0,0,0,0);
    assert(step(&f,0,0,0,0,&l,0)==LINE_STEP_WAIT);
    l.err=100;
    assert(step(&f,0,24,0,20,&l,20)==LINE_STEP_OK);
    assert(f.speed.left.duty==0 && f.speed.left.integral_milli==0);
    for(uint32_t t=40;t<300;t+=20)assert(step(&f,0,t*12/10,0,t,&l,t)==LINE_STEP_OK);
    /* The stopped inner wheel is allowed to restart without a false stall. */
    l.err=0;
    assert(step(&f,0,360,0,300,&l,300)==LINE_STEP_OK);
    assert(f.speed.left.duty>0);
    assert(f.speed.left.target_cps==1200 && f.speed.right.target_cps==1200);
    /* Unequal travel from a curve must not invoke straight-line equalisation. */
    assert(f.previous_total[0]!=f.previous_total[1]);
}
static void faults_and_wrap(void)
{
    line_follow_t f;line_msg_t l=centered();
    line_follow_begin(&f,0,0,0,0,0);
    assert(step(&f,0,0,1,20,&l,20)==LINE_STEP_ENCODER);
    assert(step(&f,1,24,0,20,&l,20)==LINE_STEP_ENCODER);
    assert(step(&f,UINT32_MAX-200,24,0,20,&l,20)==LINE_STEP_ENCODER);
    assert(step(&f,(uint32_t)-LINE_FOLLOW_COUNT_LIMIT,24,0,20,&l,20)==LINE_STEP_LIMIT);
    assert(step(&f,0,0,0,61,&l,61)==LINE_STEP_TIME);
    assert(line_follow_step(&f,0,0,0,20,20,&l,20,7,7,true,false)==LINE_STEP_DISCONNECTED);
    line_follow_begin(&f,0,0,0,0,0); /* a fault terminates the preceding request */
    for(uint32_t t=20;t<200;t+=20)assert(step(&f,0,t,0,t,&l,t)==LINE_STEP_OK);
    assert(step(&f,0,200,0,200,&l,200)==LINE_STEP_STALL);
    line_follow_begin(&f,0,0,0,0,0);
    assert(step(&f,0,0,0,LINE_FOLLOW_MAX_MS,&l,LINE_FOLLOW_MAX_MS)==LINE_STEP_DONE);

    uint32_t start=UINT32_MAX-10u,next=start+20u;
    line_follow_begin(&f,10,UINT32_MAX-5u,0,start,start);
    assert(step(&f,(uint32_t)(10-24),18,0,next,&l,next)==LINE_STEP_OK);
    assert(f.previous_total[0]==24 && f.previous_total[1]==24);
}
static void full_duration(void)
{
    line_follow_t f;line_msg_t l=centered();
    uint32_t start=UINT32_MAX-500u;
    line_follow_begin(&f,0,0,0,start,start);
    for(uint32_t elapsed=20;elapsed<LINE_FOLLOW_MAX_MS;elapsed+=20) {
        uint32_t count=elapsed*1200u/1000u;
        assert(step(&f,0u-count,count,0,start+elapsed,&l,start+elapsed)==LINE_STEP_OK);
        assert(f.speed.left.duty<=200 && f.speed.right.duty<=200);
    }
    assert(step(&f,0,0,0,start+LINE_FOLLOW_MAX_MS,&l,start+LINE_FOLLOW_MAX_MS)==LINE_STEP_DONE);
}
int main(void)
{
    input_guards();steering_and_sensors();faults_and_wrap();full_duration();
    puts("PASS: actual IR steering sign, stale/invalid/lost/junction inputs, independent targets, zero/restart, stalls, limits, raw/time wrap and deadline");
}

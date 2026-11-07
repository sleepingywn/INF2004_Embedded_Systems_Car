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
    assert(error==100 && line_follow_targets(error,&lt,&rt) && lt==250 && rt==750);
    error=line_update(&tracker,0,100,10,&state);
    assert(error==-100 && line_follow_targets(error,&lt,&rt) && lt==750 && rt==250);
    assert(line_follow_targets(0,&lt,&rt) && lt==700 && rt==700);
    assert(!line_follow_targets(101,&lt,&rt));

    line_follow_t f;line_msg_t l=centered();
    line_follow_begin(&f,0,0,0,0,0);
    assert(step(&f,0,0,0,0,&l,0)==LINE_STEP_WAIT);
    l.err=60; /* full rolling turn, below separate tight-turn entry */
    for(uint32_t t=20;t<300;t+=20)
        assert(step(&f,0u-t/2u,t,0,t,&l,t)==LINE_STEP_OK);
    assert(f.speed.left.target_cps==250 && f.speed.right.target_cps==750);
    int32_t previous=f.speed.left.target_cps;
    l.err=0;
    assert(step(&f,(uint32_t)-150,300,0,300,&l,300)==LINE_STEP_OK);
    assert(f.speed.left.target_cps>previous && f.speed.left.target_cps<=previous+80);
    /* Unequal travel from a curve must not invoke straight-line equalisation. */
    assert(f.previous_total[0]!=f.previous_total[1]);

    /* A stationary inner wheel is still a real stall at the existing 200 ms,
     * even when its target is reduced for a curve. No guard was relaxed. */
    line_follow_begin(&f,0,0,0,0,0);l.err=60;
    for(uint32_t t=20;t<200;t+=20)assert(step(&f,0,t,0,t,&l,t)==LINE_STEP_OK);
    assert(step(&f,0,200,0,200,&l,200)==LINE_STEP_STALL);
}
static void curve_response(void)
{
    int32_t previous_outer=LINE_CRUISE_CPS,previous_inner=LINE_CRUISE_CPS;
    for(int error=0;error<=100;++error) {
        int32_t l,r,ml,mr;
        assert(line_follow_targets(error,&l,&r));
        assert(line_follow_targets(-error,&ml,&mr));
        assert(l==mr && r==ml && l>=250 && l<=r && r<=750);
        /* Average speed must decrease and curvature increase even when the
         * outer wheel rises slightly above the straight target. */
        assert(l+r<=previous_inner+previous_outer);
        assert((int64_t)l*previous_outer<=(int64_t)previous_inner*r);
        previous_inner=l;previous_outer=r;
    }
    /* Previously 49<->51 crossed the immediate-stop/restart boundary.
     * Include full sign changes and centered/edge jumps, with dt jitter. */
    const int errors[]={49,51,0,100,-100,-49,-51,0};
    line_follow_t f;line_msg_t line=centered();
    line_follow_begin(&f,0,0,0,0,0);
    assert(f.speed.left.duty==LEFT_FEEDFORWARD && f.speed.right.duty==RIGHT_FEEDFORWARD);
    uint32_t now=0;
    for(unsigned i=0;i<36;++i) {
        uint32_t dt=20u+20u*(i%3u);now+=dt;
        line.err=errors[i%8u];
        int old_left=(int)f.speed.left.duty,old_right=(int)f.speed.right.duty;
        int32_t old_lt=f.speed.left.target_cps,old_rt=f.speed.right.target_cps;
        assert(step(&f,0u-now,now,0,now,&line,now)==LINE_STEP_OK);
        int dl=(int)f.speed.left.duty-old_left,dr=(int)f.speed.right.duty-old_right;
        int up_bound=(int)(250u*dt/1000u),down_bound=(int)(500u*dt/1000u);
        assert(dl>=-down_bound && dl<=up_bound && dr>=-down_bound && dr<=up_bound);
        int32_t lt=f.speed.left.target_cps,rt=f.speed.right.target_cps;
        int32_t target_bound=(int32_t)(4000u*dt/1000u);
        assert(lt>=250 && lt<=750 && rt>=250 && rt<=750);
        assert(lt-old_lt<=target_bound && old_lt-lt<=target_bound);
        assert(rt-old_rt<=target_bound && old_rt-rt<=target_bound);
        assert(f.speed.left.duty<=200 && f.speed.right.duty<=200);
    }
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
static void ambiguous_curve_gap(void)
{
    line_follow_t f;line_tracker_t tracker;line_state_t state;
    line_msg_t line=centered();line_init(&tracker);
    line_follow_begin(&f,0,0,0,0,0);
    line.err=line_update(&tracker,0,100,20,&state);line.state=state;
    assert(step(&f,(uint32_t)-20,20,0,20,&line,20)==LINE_STEP_OK);
    for(uint32_t t=40;t<=600;t+=20) {
        line.err=line_update(&tracker,0,0,20,&state);line.state=state;
        assert(line.err==0 && line.state==LINE_ON);
        assert(step(&f,0u-t,t,0,t,&line,t)==LINE_STEP_OK);
        if(t<500)assert(f.steering_error<0);
        if(t>=520)assert(f.steering_error==0);
        if(t>=200 && t<500)assert(f.speed.left.target_cps>f.speed.right.target_cps);
    }
    /* Guards take precedence even while curve-memory correction is armed. */
    line.err=-100;line.state=LINE_EDGE_ONLY;
    assert(step(&f,(uint32_t)-620,620,0,620,&line,620)==LINE_STEP_OK);
    line.err=line_update(&tracker,0,0,2000,&state);line.state=state;
    assert(line.state==LINE_LOST);
    assert(step(&f,(uint32_t)-640,640,0,640,&line,640)==LINE_STEP_IR);
    line=centered();line.state=LINE_BOTH_BLACK;
    assert(step(&f,(uint32_t)-640,640,0,640,&line,640)==LINE_STEP_IR);
    line=centered();line.fault=1;
    assert(step(&f,(uint32_t)-640,640,0,640,&line,640)==LINE_STEP_IR);
    line=centered();
    assert(step(&f,(uint32_t)-640,640,0,640,&line,500)==LINE_STEP_IR);
    assert(line_follow_step(&f,(uint32_t)-640,640,0,640,640,&line,640,7,8,true,true)==LINE_STEP_IR);
    puts("PASS: actual S3 curve-to-white gap gets bounded steering; LOST/junction/fault/stale/epoch guards prevail");
}
int main(void)
{
    input_guards();steering_and_sensors();curve_response();faults_and_wrap();full_duration();ambiguous_curve_gap();
    puts("PASS: actual IR steering sign, stale/invalid/lost/junction inputs, independent rolling targets, bounded PWM/target changes, stalls, limits, raw/time wrap and deadline");
}

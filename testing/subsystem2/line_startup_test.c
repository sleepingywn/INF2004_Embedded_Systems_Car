#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static void starts(void)
{
    const int errors[]={-100,-21,0,21,100};
    for(unsigned e=0;e<sizeof(errors)/sizeof(errors[0]);++e)
    for(unsigned dt=20;dt<=60;dt+=20) {
        line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
        assert(f.speed.left.duty==80 && f.speed.right.duty==80);
        assert(f.speed.left.integral_milli<0 && f.speed.right.integral_milli<0);
        line_msg_t line={.state=LINE_ON,.err=errors[e]};
        assert(line_follow_step(&f,0,0,0,dt,dt,&line,dt,1,1,true,true)==LINE_STEP_OK);
        assert(f.speed.left.duty<=80+250*dt/1000 && f.speed.right.duty<=80+250*dt/1000);
        /* Repeated no-motion feedback cannot silently bypass stall protection. */
        for(unsigned t=dt+20;t<200;t+=20) {
            unsigned l=f.speed.left.duty,r=f.speed.right.duty;
            assert(line_follow_step(&f,0,0,0,t,t,&line,t,1,1,true,true)==LINE_STEP_OK);
            assert(f.speed.left.duty<=l+5 && f.speed.right.duty<=r+5);
        }
        assert(line_follow_step(&f,0,0,0,200,200,&line,200,1,1,true,true)==LINE_STEP_STALL);
    }
}
static void overspeed_and_reset(void)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    line_msg_t line={.state=LINE_ON};
    int32_t total=0;
    for(unsigned t=20;t<=180;t+=20) {
        /* Replay strong early movement: controller must reduce power rather
         * than restoring the old feed-forward when its integral is updated. */
        total+=20;unsigned l=f.speed.left.duty,r=f.speed.right.duty;
        assert(line_follow_step(&f,0u-(uint32_t)total,total,0,t,t,&line,t,1,1,true,true)==LINE_STEP_OK);
        assert(f.speed.left.duty<=l && f.speed.right.duty<=r);
    }
    line_follow_begin(&f,123,456,0,UINT32_MAX-9u,UINT32_MAX-9u);
    assert(f.speed.left.duty==80 && f.speed.right.duty==80);
    assert(line_follow_step(&f,123,456,0,10,10,&line,10,1,1,true,true)==LINE_STEP_OK);
    assert(f.speed.left.duty<=85 && f.speed.right.duty<=85);
    speed_control_t bench;speed_control_start(&bench);
    assert(bench.left.duty==START_DUTY && bench.right.duty==START_DUTY);
    assert(!bench.left.integral_milli && !bench.right.integral_milli);
}
int main(void)
{
    starts();overspeed_and_reset();
    puts("PASS: initial low-power output and integral, dt/turn variants, ordinary slew, unchanged stall deadline, overspeed response, reset/wrap and isolated bench");
}

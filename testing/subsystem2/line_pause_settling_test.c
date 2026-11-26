#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static line_step_t step(line_follow_t *f,unsigned dt,int sign,int inside_delta,
                        uint32_t invalid,int error)
{
    uint32_t now=f->previous_ms+dt;
    int32_t l=f->previous_total[0],r=f->previous_total[1];
    if(sign>0){l+=inside_delta;r+=12;}else {r+=inside_delta;l+=12;}
    line_msg_t line={.state=LINE_EDGE_ONLY,.err=error};
    unsigned left=error>0 ? 95:error<0 ? 5:45;
    unsigned right=error<0 ? 95:error>0 ? 5:45;
    return line_follow_step_observed(f,f->origin_left-(uint32_t)l,
        f->origin_right+(uint32_t)r,invalid,now,now,&line,now,7,7,true,true,
        line_levels_pack(left,right));
}
static line_follow_t pause(int sign,unsigned dt,uint32_t start)
{
    line_follow_t f;line_follow_begin(&f,5,UINT32_MAX-5u,0,start,start);
    unsigned inside=sign>0 ? 0:1;
    for(unsigned n=0;n<40;++n) {
        assert(step(&f,dt,sign,12,0,sign*90)==LINE_STEP_OK);
        if(line_pause_eligible(&f)&(1u<<inside)) {
            return f;
        }
    }
    assert(!"pause must be reached");return f;
}
static void bounds(int sign,unsigned dt,uint32_t start)
{
    unsigned i=sign>0 ? 0:1;
    line_follow_t initial=pause(sign,dt,start),f=initial;
    /* The actual sweep swap rolled back three ticks at 60 ms. Continue
     * on either side, even with repeated rollback past the old time limit. */
    for(unsigned n=0;n<15;++n)
        assert(step(&f,20,sign,-3,0,sign*90)==LINE_STEP_OK);
    assert(f.pause_reverse_used[i]==45 && f.last_settle_ms[i]==f.previous_ms);
    assert(step(&f,20,sign,-1,0,0)==LINE_STEP_OK);
    assert(step(&f,20,sign,-1,0,0)==LINE_STEP_OK);
    line_step_t result=LINE_STEP_OK;
    for(unsigned n=0;n<15 && result==LINE_STEP_OK;++n)
        result=step(&f,20,sign,n%2 ? 1:-1,0,0);
    assert(result==LINE_STEP_OK);
    f=initial;
    assert(step(&f,20,sign,-300,0,sign*90)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_SPEED);
    f=initial;
    assert(step(&f,20,sign,-3,1,sign*90)==LINE_STEP_OK);
    assert(!f.encoder_reject);
}
int main(void)
{
    for(int sign=-1;sign<=1;sign+=2)for(unsigned dt=20;dt<=60;dt+=20) {
        bounds(sign,dt,0);bounds(sign,dt,UINT32_MAX-300u);
    }
    puts("PASS V5 pause settling: rollback across sweep changes, powered bounce tolerance, rate guard and discarded-edge tolerance, wrap");
}

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
            assert(f.pause_window_active[inside]);
            assert(f.pause_window_ms[inside]==f.previous_ms);
            return f;
        }
    }
    assert(!"pause must be reached");return f;
}
static void bounds(int sign,unsigned dt,uint32_t start)
{
    unsigned i=sign>0 ? 0:1;
    line_follow_t initial=pause(sign,dt,start),f=initial;
    /* The observed two counts can be accepted in one or separate samples. */
    assert(step(&f,20,sign,0,0,sign*90)==LINE_STEP_OK);
    assert(step(&f,20,sign,-1,0,sign*90)==LINE_STEP_OK);
    assert(step(&f,20,sign,-1,0,sign*90)==LINE_STEP_OK);
    assert(f.pause_reverse_used[i]==2 && f.last_settle_ms[i]==f.previous_ms);
    assert(step(&f,20,sign,0,0,sign*90)==LINE_STEP_OK);
    assert(step(&f,20,sign,-1,0,sign*90)==LINE_STEP_ENCODER);
    f=initial;assert(step(&f,dt,sign,-2,0,sign*90)==LINE_STEP_OK);
    f=initial;assert(step(&f,dt,sign,-3,0,sign*90)==LINE_STEP_ENCODER);
    /* Gross reverse travel, not net displacement: forward bounce cannot refill. */
    f=initial;assert(step(&f,20,sign,-2,0,sign*90)==LINE_STEP_OK);
    assert(step(&f,20,sign,2,0,sign*90)==LINE_STEP_OK);
    assert(step(&f,20,sign,-1,0,sign*90)==LINE_STEP_ENCODER);
    /* Window is strict: 119 ms accepted, 120 ms rejected. */
    f=initial;assert(step(&f,59,sign,0,0,sign*90)==LINE_STEP_OK);
    assert(step(&f,60,sign,-1,0,sign*90)==LINE_STEP_OK);
    /* No delayed drift exemption, even with all count allowance unused. */
    f=initial;
    for(unsigned t=20;t<120;t+=20)assert(step(&f,20,sign,0,0,sign*90)==LINE_STEP_OK);
    assert(step(&f,20,sign,-1,0,sign*90)==LINE_STEP_ENCODER);
    assert(f.pause_reverse_used[i]==0);
    /* An unchanged wheel can remain paused after the settling deadline. */
    f=initial;
    for(unsigned t=0;t<400;t+=20)assert(step(&f,20,sign,0,0,sign*90)==LINE_STEP_OK);
    f=initial;assert(step(&f,dt,sign,-1,1,sign*90)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_INVALID_EDGES && !f.pause_reverse_used[i]);
    /* Moving output is never exempt, including immediately after a restart. */
    f=initial;assert(step(&f,20,sign,-1,0,0)==LINE_STEP_OK);
    assert(!(line_pause_eligible(&f)&(1u<<i)) && f.pause_reverse_used[i]==1);
    assert(step(&f,20,sign,-1,0,0)==LINE_STEP_ENCODER);
    /* One restart followed by no progress cannot replenish count/time budget. */
    f=initial;assert(step(&f,20,sign,-2,0,0)==LINE_STEP_OK);
    uint32_t window=f.pause_window_ms[i];
    for(unsigned n=0;n<8;++n) {
        assert(step(&f,20,sign,0,0,sign*90)==LINE_STEP_OK);
        assert(f.pause_reverse_used[i]==2 && f.pause_window_ms[i]==window);
    }
    assert(line_pause_eligible(&f)&(1u<<i));
    assert(step(&f,20,sign,-1,0,sign*90)==LINE_STEP_ENCODER);
    /* Real forward ticks during a brief command pulse also cannot rearm. */
    f=initial;assert(step(&f,20,sign,-2,0,0)==LINE_STEP_OK);
    window=f.pause_window_ms[i];
    for(unsigned n=0;n<15;++n) {
        assert(step(&f,20,sign,6,0,sign*90)==LINE_STEP_OK);
        assert(f.pause_reverse_used[i]==2 && f.pause_window_ms[i]==window);
        if(line_pause_eligible(&f)&(1u<<i))break;
    }
    assert(line_pause_eligible(&f)&(1u<<i));
    assert(step(&f,20,sign,-1,0,sign*90)==LINE_STEP_ENCODER);
    /* Measured powered forward rolling legitimately rearms a later pause. */
    f=initial;assert(step(&f,20,sign,-2,0,0)==LINE_STEP_OK);
    bool rearmed=false;
    for(unsigned n=0;n<30;++n) {
        assert(step(&f,20,sign,6,0,0)==LINE_STEP_OK);
        if(!f.pause_window_active[i]) {
            assert(f.pause_forward_ms[i]>=60 && f.pause_forward_counts[i]>=3);
            assert(!f.pause_reverse_used[i]);rearmed=true;break;
        }
    }
    assert(rearmed);
    for(unsigned n=0;n<30;++n) {
        assert(step(&f,20,sign,6,0,sign*90)==LINE_STEP_OK);
        if(line_pause_eligible(&f)&(1u<<i))break;
    }
    assert(line_pause_eligible(&f)&(1u<<i));
    assert(f.pause_window_ms[i]!=window);
    assert(step(&f,20,sign,-2,0,sign*90)==LINE_STEP_OK);
}
int main(void)
{
    for(int sign=-1;sign<=1;sign+=2)for(unsigned dt=20;dt<=60;dt+=20) {
        bounds(sign,dt,0);bounds(sign,dt,UINT32_MAX-300u);
    }
    puts("PASS V5 pause settling: gross count/time bounds, no bounce/short restart renewal, powered/invalid rejection, qualified rearm and wrap");
}

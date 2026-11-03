#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static void baseline(unsigned dt,int sign,uint32_t start)
{
    line_adaptive_t s={0};line_adaptive_output_t o={0};
    for(unsigned t=0;t<600;t+=dt)
        o=line_adaptive_step(&s,line_levels_pack(sign>0 ? 95:5,sign>0 ? 5:95),start+t,dt,t/4,t/4);
    assert(sign>0 ? o.left==0 && o.right==600:o.right==0 && o.left==600);
    /* v2 uses filtered-demand hysteresis; v3 instead immediately released
     * on raw error 55. Restoring this distinction is deliberate. */
    o=line_adaptive_step(&s,line_levels_pack(sign>0 ? 60:5,sign>0 ? 5:60),start+600,dt,150,150);
    assert(sign>0 ? o.left==0:o.right==0);
    for(unsigned t=600+dt;t<1200;t+=dt)
        o=line_adaptive_step(&s,line_levels_pack(50,50),start+t,dt,t/4,t/4);
    assert(o.left>0 && o.right>0 && la_abs(o.left-o.right)<=4);
    /* v2 can confirm a sustained error 60; v3's added raw 75 gate could not. */
    s=(line_adaptive_t){0};
    for(unsigned t=0;t<600;t+=dt)
        o=line_adaptive_step(&s,line_levels_pack(sign>0 ? 65:5,sign>0 ? 5:65),start+t,dt,t/4,t/4);
    assert(sign>0 ? o.left==0 && o.right>=580:o.right==0 && o.left>=580);
}
static void immediate_pause(int sign)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    int32_t l=0,r=0;bool entered=false;
    line_msg_t line={.state=LINE_EDGE_ONLY,.err=sign*90};
    for(unsigned t=20;t<600;t+=20) {
        int32_t before=sign>0 ? f.speed.left.target_cps:f.speed.right.target_cps;
        l+=f.speed.left.target_cps/50;r+=f.speed.right.target_cps/50;
        assert(line_follow_step_observed(&f,0u-(uint32_t)l,(uint32_t)r,0,t,t,&line,t,1,1,true,true,
            line_levels_pack(sign>0 ? 95:5,sign>0 ? 5:95))==LINE_STEP_OK);
        int32_t after=sign>0 ? f.speed.left.target_cps:f.speed.right.target_cps;
        if(before>0 && after==0) {
            assert(before>=250); /* immediate pause, not v3's multi-step ramp */
            entered=true;
            assert(sign>0 ? !f.speed.left.duty:!f.speed.right.duty);
        }
    }
    assert(entered);
}
int main(void)
{
    for(unsigned dt=20;dt<=60;dt+=20)for(int sign=-1;sign<=1;sign+=2) {
        baseline(dt,sign,0);baseline(dt,sign,UINT32_MAX-500);
    }
    immediate_pause(1);immediate_pause(-1);
    puts("PASS restored v2 steering: 600 outer cap, filtered hysteresis, moderate sustained error, immediate pause");
}

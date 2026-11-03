#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static void budgets(uint32_t start)
{
    line_overlap_t o={0};int32_t l,r;
    assert(line_overlap_step(&o,true,false,false,start,0,0,450,450,&l,&r)==LINE_OVERLAP_UNSAFE);
    assert(!o.active);
    assert(line_overlap_step(&o,true,true,false,start,100,100,0,500,&l,&r)==LINE_OVERLAP_HOLD);
    assert(l==0 && r==350);
    assert(line_overlap_step(&o,true,true,false,start+119,100,159,0,500,&l,&r)==LINE_OVERLAP_HOLD);
    assert(line_overlap_step(&o,true,true,false,start+120,100,160,0,500,&l,&r)==LINE_OVERLAP_TIMEOUT);
    o=(line_overlap_t){0};
    line_overlap_step(&o,true,true,false,start,100,100,600,250,&l,&r);
    assert(l==350 && r==250);
    assert(line_overlap_step(&o,true,true,false,start+20,179,100,600,250,&l,&r)==LINE_OVERLAP_HOLD);
    assert(line_overlap_step(&o,true,true,false,start+40,180,100,600,250,&l,&r)==LINE_OVERLAP_RANGE);
    o=(line_overlap_t){0};
    line_overlap_step(&o,true,true,false,start,0,0,40,450,&l,&r);
    assert(l==40 && r==350); /* do not kick a low target upward */
    for(unsigned dt=20;dt<=100;dt+=20) {
        bool black=dt%40==0;
        assert(line_overlap_step(&o,black,true,!black,start+dt,0,0,40,450,&l,&r)==
               (black ? LINE_OVERLAP_HOLD:LINE_OVERLAP_NONE));
    }
    assert(line_overlap_step(&o,true,true,false,start+120,0,0,40,450,&l,&r)==LINE_OVERLAP_TIMEOUT);
    o=(line_overlap_t){0};
    line_overlap_step(&o,true,true,false,start,0,0,250,600,&l,&r);
    for(unsigned dt=20;dt<=80;dt+=20)
        assert(line_overlap_step(&o,false,true,true,start+dt,0,0,250,600,&l,&r)==LINE_OVERLAP_NONE);
    assert(!o.active); /* only confirmed directional evidence gives a new budget */
    line_overlap_step(&o,true,true,false,start+100,0,0,250,600,&l,&r);
    assert(o.started_ms==start+100);
    for(unsigned dt=120;dt<=220;dt+=20)
        line_overlap_step(&o,false,true,false,start+dt,0,0,250,600,&l,&r);
    assert(o.active); /* white cannot renew the overlap budget */
    assert(line_overlap_step(&o,true,true,false,start+240,0,0,250,600,&l,&r)==LINE_OVERLAP_TIMEOUT);
}
typedef struct {line_follow_t f;uint32_t now;int32_t l,r;} fixture_t;
static line_step_t step(fixture_t *x,int state,int err)
{
    x->now+=20;
    if(x->f.speed.left.target_cps)x->l+=10;
    if(x->f.speed.right.target_cps)x->r+=10;
    line_msg_t line={.state=state,.err=err};
    return line_follow_step(&x->f,0u-(uint32_t)x->l,(uint32_t)x->r,0,x->now,x->now,
                           &line,x->now,1,1,true,true);
}
static fixture_t prepared(int sign)
{
    fixture_t x={0};line_follow_begin(&x.f,0,0,0,0,0);
    while(x.now<140)assert(step(&x,LINE_EDGE_ONLY,100*sign)==LINE_STEP_OK);
    assert(x.f.tight.direction==sign);
    assert(step(&x,LINE_EDGE_ONLY,11*sign)==LINE_STEP_OK);
    return x;
}
static void controller(int sign)
{
    fixture_t x=prepared(sign);
    line_msg_t black={.state=LINE_BOTH_BLACK};
    assert(!line_follow_usable(&black,0,0,1,1,true)); /* never start on black */
#if MOTION_LINE_SEARCH
    uint32_t curve_ms=x.f.last_curve_ms;
    for(unsigned n=0;n<2;++n) {
        assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_OK);
        assert(x.f.tight.direction==sign && x.f.overlap.active && !x.f.search.active);
        assert(sign>0 ? !x.f.speed.left.duty:!x.f.speed.right.duty);
        assert(x.f.last_curve_ms==curve_ms);
    }
    assert(sign>0 ? x.f.speed.right.target_cps==350:x.f.speed.left.target_cps==350);
    assert(step(&x,LINE_ON,-20*sign)==LINE_STEP_OK && !x.f.tight.direction);
    assert(x.f.steering_error==-20*sign);
    assert(sign>0 ? x.f.speed.left.duty<=115:x.f.speed.right.duty<=115);
    for(unsigned n=0;n<3;++n)assert(step(&x,LINE_ON,-40*sign)==LINE_STEP_OK);
    assert(!x.f.overlap.active);
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_OK); /* new confirmed segment */

    x=prepared(sign);
    for(unsigned n=0;n<6;++n)assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_OK);
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_OVERLAP_TIMEOUT);
    x=prepared(sign);assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_OK);
    if(sign>0)x.r+=80;else x.l+=80;
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_OVERLAP_RANGE);

    x=prepared(sign);x.f.tight.started_ms=x.now+20-LINE_TIGHT_MAX_MS;
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_TIGHT_TIMEOUT);
    x=prepared(sign);x.f.search.active=true;
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_IR);
    x=prepared(sign);x.f.last_curve_ms=x.now-LINE_OVERLAP_RECENT_MS;
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_IR);
    x=prepared(sign);x.f.started_ms=x.now+20-LINE_FOLLOW_MAX_MS;
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_DONE);

    /* Real sensor faults, malformed states, missing/stale data and epochs
     * still win before any overlap motion can be considered. */
    x=prepared(sign);uint32_t now=x.now+20;
    for(unsigned guard=0;guard<7;++guard) {
        line_follow_t f=x.f;line_msg_t input=black;
        if(guard==0)input.fault=1;
        if(guard==1)input.err=1;
        if(guard==2)input.state=LINE_FAULT;
        line_step_t result=line_follow_step(&f,0u-(uint32_t)x.l,(uint32_t)x.r,0,now,now,
            guard==3 ? NULL:&input,guard==4 ? now-101:now,1,guard==5 ? 2:1,guard!=6,true);
        assert(result==LINE_STEP_IR && !f.overlap.active);
    }
    x=prepared(sign);assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_OK);
    now=x.now+20;
    assert(line_follow_step(&x.f,0u-(uint32_t)x.l,x.r,1,now,now,&black,now,1,1,true,true)==LINE_STEP_ENCODER);
    assert(line_follow_step(&x.f,0u-(uint32_t)x.l,x.r,0,now,now,&black,now,1,1,true,false)==LINE_STEP_DISCONNECTED);
    line_follow_begin(&x.f,0,0,0,now,now);assert(!x.f.overlap.active);
    x=(fixture_t){0};line_follow_begin(&x.f,0,0,0,0,0);
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_IR); /* no established curve */
#else
    assert(step(&x,LINE_BOTH_BLACK,0)==LINE_STEP_IR);
#endif
}
int main(void)
{
    budgets(0);budgets(UINT32_MAX-60u);controller(1);controller(-1);
    puts("PASS: bounded both-black handling, no start, preserved pause, real opposite priority, flicker/deadline/travel, guards and wrap");
}

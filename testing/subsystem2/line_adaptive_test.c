#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static void scenarios(uint32_t start,unsigned dt,int sign)
{
    line_adaptive_t s={0};
    line_adaptive_output_t out;
    uint32_t now=start;
    int32_t l=0,r=0;
    /* Same configuration: visible straight, sustained curve far beyond the
     * former 800ms cap, then reverse curvature, then straighten. */
    for(unsigned elapsed=0;elapsed<6000;elapsed+=dt) {
        unsigned a=50,b=50;
        if(elapsed>=500 && elapsed<3500){a=95;b=5;}
        if(elapsed>=3500 && elapsed<5000){a=5;b=95;}
        if(sign<0){unsigned tmp=a;a=b;b=tmp;}
        out=line_adaptive_step(&s,line_levels_pack(a,b),now,dt,l,r);
        assert(out.mode==LINE_ADAPT_TRACK);
        assert(out.left>=0 && out.right>=0 && out.left<=600 && out.right<=600);
        if(elapsed>1000 && elapsed<3500)assert(sign>0 ? out.left<out.right:out.left>out.right);
        if(elapsed>4000 && elapsed<5000)assert(sign>0 ? out.left>out.right:out.left<out.right);
        if(elapsed>5500)assert(la_abs(out.left-out.right)<=4);
        l+=out.left*(int32_t)dt/1000;r+=out.right*(int32_t)dt/1000;now+=dt;
    }
    /* An initially centered/outside pair is uncertain, never declared seen.
     * Reacquisition needs sustained evidence; weak flashes don't reset bounds. */
    s=(line_adaptive_t){0};now=start;l=r=0;
    out=line_adaptive_step(&s,line_levels_pack(3,4),now,dt,l,r);
    assert(out.mode==LINE_ADAPT_UNCERTAIN);
    bool searched=false;
    for(unsigned elapsed=dt;elapsed<=4000;elapsed+=dt) {
        l+=out.left*(int32_t)dt/1000;r+=out.right*(int32_t)dt/1000;now+=dt;
        unsigned a=elapsed%240<dt ? 60:3;
        out=line_adaptive_step(&s,line_levels_pack(a,4),now,dt,l,r);
        if(out.mode==LINE_ADAPT_SEARCH)searched=true;
        if(out.mode==LINE_ADAPT_LOST)break;
    }
    assert(searched && out.mode==LINE_ADAPT_LOST && !out.left && !out.right);
    out=line_adaptive_step(&s,line_levels_pack(80,5),now+dt,dt,l,r);
    assert(out.mode==LINE_ADAPT_LOST); /* explicit new run required */

    s=(line_adaptive_t){0};now=start;
    out=line_adaptive_step(&s,line_levels_pack(2,2),now,dt,0,0);
    out=line_adaptive_step(&s,line_levels_pack(2,2),now+dt,dt,120,120);
    assert(out.mode==LINE_ADAPT_SEARCH);
    for(unsigned t=2*dt;t<=200;t+=dt)
        out=line_adaptive_step(&s,line_levels_pack(5,80),now+t,dt,120+t/3,120+t/3);
    assert(out.mode==LINE_ADAPT_TRACK && !s.uncertain && out.left>out.right);

    s=(line_adaptive_t){0};
    out=line_adaptive_step(&s,line_levels_pack(90,90),start,dt,0,0);
    assert(out.mode==LINE_ADAPT_CROSSING && out.left==300 && out.right==300);
    out=line_adaptive_step(&s,line_levels_pack(5,80),start+dt,dt,0,0);
    assert(out.mode==LINE_ADAPT_TRACK); /* short mark clears */
    out=line_adaptive_step(&s,line_levels_pack(90,90),start+2*dt,dt,0,0);
    out=line_adaptive_step(&s,line_levels_pack(90,90),start+2*dt+100,dt,0,0);
    assert(out.mode==LINE_ADAPT_CROSSING && out.left && out.right);
    out=line_adaptive_step(&s,line_levels_pack(90,90),start+4000,dt,0,0);
    assert(out.mode==LINE_ADAPT_CROSSING_FAILED && !out.left && !out.right);
    s=(line_adaptive_t){.route_blocked=true};
    out=line_adaptive_step(&s,line_levels_pack(50,50),start,dt,0,0);
    assert(out.mode==LINE_ADAPT_ROUTE_REQUIRED && s.stop_reason==LINE_STOP_ROUTE_COMMAND);
}
static void owner_wrapper(void)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    line_msg_t line={.state=LINE_EDGE_ONLY,.err=90};
    unsigned l=0,r=0;
    /* Exercise actual encoder/PI pipeline, not just detached target math. */
    for(unsigned now=20;now<2500;now+=20) {
        l+=(unsigned)f.speed.left.target_cps/50;
        r+=(unsigned)f.speed.right.target_cps/50;
        assert(line_follow_step_observed(&f,0u-l,r,0,now,now,&line,now,1,1,true,true,
            line_levels_pack(95,5))==LINE_STEP_OK);
        assert(f.speed.left.duty<=200 && f.speed.right.duty<=200);
    }
    line_follow_t before=f;
    assert(f.adaptive.paused[0] && !f.speed.left.duty && !f.speed.left.target_cps);
    /* V5 accepts settling only near pause entry, never delayed reverse drift. */
    assert(line_follow_step_observed(&f,0u-(l-1),r+12,0,2500,2500,&line,2500,1,1,true,true,
        line_levels_pack(95,5))==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_LEFT_RECEDING && !f.pause_reverse_used[0]);
    f=before;
    assert(line_follow_step_observed(&f,0u-l,r,0,2500,2500,&line,2300,1,1,true,true,
        line_levels_pack(95,5))==LINE_STEP_IR);
    f=before;
    assert(line_follow_step_observed(&f,0u-l,r,0,2500,2500,&line,2500,1,2,true,true,
        line_levels_pack(95,5))==LINE_STEP_IR);
    f=before;
    assert(line_follow_step_observed(&f,0u-l,r,1,2500,2500,&line,2500,1,1,true,true,
        line_levels_pack(95,5))==LINE_STEP_ENCODER);
    f=before;
    assert(line_follow_step_observed(&f,0u-l,r,0,2500,2500,&line,2500,1,1,true,false,
        line_levels_pack(95,5))==LINE_STEP_DISCONNECTED);
    f=before;
    assert(line_follow_step_observed(&f,0u-l,r,0,2500,2500,&line,2500,1,1,true,true,0)==LINE_STEP_IR);
    line_follow_begin(&f,0,0,0,0,0);
    for(unsigned t=20;t<200;t+=20)
        assert(line_follow_step_observed(&f,0,0,0,t,t,&line,t,1,1,true,true,
            line_levels_pack(95,5))==LINE_STEP_OK);
    assert(line_follow_step_observed(&f,0,0,0,200,200,&line,200,1,1,true,true,
        line_levels_pack(95,5))==LINE_STEP_STALL);
}
int main(void)
{
    for(unsigned l=0;l<=100;++l)for(unsigned r=0;r<=100;++r) {
        uint16_t levels=line_levels_pack(l,r);
        assert(line_levels_valid(levels) && line_level_left(levels)==l && line_level_right(levels)==r);
    }
    assert(!line_levels_valid(0) && !line_levels_valid(0xffff) && !line_levels_pack(101,0));
    for(unsigned dt=20;dt<=60;dt+=20)for(int sign=-1;sign<=1;sign+=2) {
        scenarios(0,dt,sign);scenarios(UINT32_MAX-500,dt,sign);
    }
    owner_wrapper();
    puts("PASS adaptive: sustained/reversed curves, uncertainty, noisy recovery, marks, guards, clock wrap, 20/40/60ms");
}

#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"
#include "../../app_program/subsystem3/sensor_line.h"

static void decisions(void)
{
    line_search_t s={0};int32_t l=0,r=0;
    assert(line_search_step(&s,0,false,false,100,0,0,&l,&r)==LINE_SEARCH_TRACK);
    assert(line_search_step(&s,0,false,false,699,0,0,&l,&r)==LINE_SEARCH_TRACK);
    assert(line_search_step(&s,0,false,false,700,0,0,&l,&r)==LINE_SEARCH_SWEEP && l==250 && r==550);
    assert(line_search_step(&s,0,true,false,1199,0,0,&l,&r)==LINE_SEARCH_SWEEP && l==250);
    assert(line_search_step(&s,0,true,false,1200,0,0,&l,&r)==LINE_SEARCH_SWEEP && r==250);
    assert(line_search_step(&s,0,true,false,2199,0,0,&l,&r)==LINE_SEARCH_SWEEP);
    assert(line_search_step(&s,0,true,false,2200,0,0,&l,&r)==LINE_SEARCH_TIMEOUT);
    for(int sign=-1;sign<=1;sign+=2) {
        s=(line_search_t){0};uint32_t start=UINT32_MAX-100;
        assert(line_search_step(&s,40*sign,false,false,start-320,0,0,&l,&r)==LINE_SEARCH_TRACK);
        assert(line_search_step(&s,0,true,false,start-300,0,0,&l,&r)==LINE_SEARCH_TRACK);
        assert(line_search_step(&s,0,true,false,start,0,0,&l,&r)==LINE_SEARCH_SWEEP);
        assert(sign>0 ? l<r:r<l);
        assert(line_search_step(&s,-30*sign,false,false,start+20,0,0,&l,&r)==LINE_SEARCH_SWEEP);
        assert(sign>0 ? l>r:r>l); /* actual opposite reading immediately steers */
        assert(line_search_step(&s,0,true,false,start+40,0,0,&l,&r)==LINE_SEARCH_SWEEP);
        assert(line_search_step(&s,30*sign,false,false,start+60,0,0,&l,&r)==LINE_SEARCH_SWEEP);
        assert(line_search_step(&s,-30*sign,false,false,start+80,0,0,&l,&r)==LINE_SEARCH_SWEEP);
        assert(line_search_step(&s,-30*sign,false,false,start+139,0,0,&l,&r)==LINE_SEARCH_SWEEP);
        assert(line_search_step(&s,-30*sign,false,false,start+140,0,0,&l,&r)==LINE_SEARCH_TRACK);
        assert(!s.active && !s.phase && s.last_direction==-sign);
    }
    s=(line_search_t){0};line_search_step(&s,0,true,false,UINT32_MAX-599u,0,0,&l,&r);
    line_search_step(&s,0,true,false,0,0,0,&l,&r);
    for(unsigned t=20;t<1500;t+=20) {
        int noise=t%40 ? 21:0;
        assert(line_search_step(&s,noise,false,false,t,0,0,&l,&r)==LINE_SEARCH_SWEEP);
        assert(s.started_ms==0);
    }
    assert(line_search_step(&s,25,false,false,1500,0,0,&l,&r)==LINE_SEARCH_TIMEOUT);
    s=(line_search_t){0};line_search_step(&s,0,true,false,UINT32_MAX-599u,0,0,&l,&r);
    line_search_step(&s,0,true,false,0,0,0,&l,&r);
    assert(line_search_step(&s,0,true,false,20,1000,0,&l,&r)==LINE_SEARCH_RANGE);
    puts("PASS: two arcs, last direction, confirmation/noise, fixed deadline/travel budget and wrap");
}
static void sensor_and_controller(void)
{
    line_follow_t f;line_tracker_t tracker;line_state_t state;line_msg_t line={0};
    line_init(&tracker);line.err=line_update(&tracker,0,0,3000,&state);line.state=state;
    assert(state==LINE_LOST && line_follow_usable(&line,0,0,7,7,true));
    line_follow_begin(&f,0,0,0,0,0);
    for(uint32_t t=20;t<2120;t+=20) {
        assert(line_follow_step(&f,0u-t/2,t/2,0,t,t,&line,t,7,7,true,true)==LINE_STEP_OK);
        assert(f.search.active==(t>=620));
        assert(f.speed.left.target_cps>0 && f.speed.right.target_cps>0);
        if(t>=100 && t<620)assert(f.speed.left.target_cps==350 && f.speed.right.target_cps==350);
        assert(f.speed.left.duty<=200 && f.speed.right.duty<=200);
    }
    assert(line_follow_step(&f,(uint32_t)-1060,1060,0,2120,2120,&line,2120,7,7,true,true)==LINE_STEP_SEARCH_TIMEOUT);
    line_follow_begin(&f,0,0,0,0,0);
    for(uint32_t t=20;t<=1200;t+=20) {
        line.err=line_update(&tracker,t<800 ? 0:80,0,20,&state);line.state=state;
        assert(line_follow_step(&f,0u-t/2,t/2,0,t,t,&line,t,7,7,true,true)==LINE_STEP_OK);
        if(t>=860)assert(!f.search.active);
    }
    /* Ambiguity does not immediately search, even if a pause was active. */
    line_follow_begin(&f,0,0,0,0,0);f.tight.direction=1;line=(line_msg_t){.state=LINE_ON};
    assert(line_follow_step(&f,(uint32_t)-10,10,0,20,20,&line,20,7,7,true,true)==LINE_STEP_OK);
    assert(!f.search.active && f.tight.direction==1 && !f.speed.left.target_cps);
    line.state=LINE_LOST;line.fault=1;
    assert(!line_follow_usable(&line,40,40,7,7,true));line.fault=0;
    line.err=1;assert(!line_follow_usable(&line,40,40,7,7,true));line.err=0;
    assert(line_follow_step(&f,(uint32_t)-20,20,0,40,40,&line,40,7,8,true,true)==LINE_STEP_IR);
    assert(line_follow_step(&f,(uint32_t)-20,20,0,140,140,&line,20,7,7,true,true)==LINE_STEP_IR);
    assert(line_follow_step(&f,(uint32_t)-20,20,1,40,40,&line,40,7,7,true,true)==LINE_STEP_ENCODER);
    assert(line_follow_step(&f,(uint32_t)-8,20,0,40,40,&line,40,7,7,true,true)==LINE_STEP_ENCODER);
    line.state=LINE_BOTH_BLACK;
    assert(line_follow_step(&f,(uint32_t)-20,20,0,40,40,&line,40,7,7,true,true)==LINE_STEP_IR);
    line.state=LINE_LOST;
    assert(line_follow_step(&f,(uint32_t)-20,20,0,40,40,&line,40,7,7,true,false)==LINE_STEP_DISCONNECTED);
    line_follow_begin(&f,0,0,0,0,0);
    for(uint32_t t=20;t<200;t+=20)
        assert(line_follow_step(&f,0,t/2,0,t,t,&line,t,7,7,true,true)==LINE_STEP_OK);
    assert(line_follow_step(&f,0,100,0,200,200,&line,200,7,7,true,true)==LINE_STEP_STALL);
    line_follow_begin(&f,0,0,0,0,0);
    assert(!f.search.active && !f.search.last_direction);
    puts("PASS: actual S3 centered LOST gets bounded search, reacquisition, pause handoff and all fault guards");
}
int main(void){decisions();sensor_and_controller();return 0;}

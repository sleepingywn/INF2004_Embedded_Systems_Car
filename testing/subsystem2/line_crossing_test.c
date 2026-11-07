#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static void crossing(uint32_t start)
{
    line_adaptive_t s={0};line_adaptive_output_t o;
    o=line_adaptive_step(&s,line_levels_pack(100,80),start,20,0,0);
    assert(o.mode==LINE_ADAPT_CROSSING && o.left==300 && o.right==300);
    o=line_adaptive_step(&s,line_levels_pack(100,91),start+100,20,25,40);
    assert(o.mode==LINE_ADAPT_CROSSING && o.left>o.right); /* catch up left */
    assert(s.crossing.started_ms==start);
    o=line_adaptive_step(&s,line_levels_pack(4,4),start+200,20,60,60);
    assert(o.mode==LINE_ADAPT_UNCERTAIN && s.crossing.active);
    for(unsigned t=220;t<=300;t+=20)
        o=line_adaptive_step(&s,line_levels_pack(5,60),start+t,20,70,70);
    assert(o.mode==LINE_ADAPT_TRACK && !s.crossing.active && !s.uncertain);
    /* A distinct, confirmed later encounter receives a new finite budget. */
    o=line_adaptive_step(&s,line_levels_pack(90,90),start+400,20,100,100);
    assert(s.crossing.started_ms==start+400);
    o=line_adaptive_step(&s,line_levels_pack(90,90),start+500,20,500,500);
    assert(o.mode==LINE_ADAPT_CROSSING_FAILED && !o.left && !o.right &&
        s.stop_reason==LINE_STOP_MARK_DISTANCE);
    o=line_adaptive_step(&s,line_levels_pack(5,90),start+520,20,500,500);
    assert(o.mode==LINE_ADAPT_CROSSING_FAILED); /* terminal latch */

    s=(line_adaptive_t){0};
    o=line_adaptive_step(&s,line_levels_pack(90,90),start,20,0,0);
    for(unsigned t=20;t<1800;t+=20) {
        /* One-sample sightings and gray/gray cannot reset encounter limits. */
        uint16_t pair=t%100==20 ? line_levels_pack(80,5):line_levels_pack(74,74);
        o=line_adaptive_step(&s,pair,start+t,20,t/10,t/10);
        assert(s.crossing.active && s.crossing.started_ms==start);
    }
    o=line_adaptive_step(&s,line_levels_pack(90,90),start+1800,20,180,180);
    assert(o.mode==LINE_ADAPT_CROSSING_FAILED && s.stop_reason==LINE_STOP_MARK_TIME);

    s=(line_adaptive_t){0};
    o=line_adaptive_step(&s,line_levels_pack(90,90),start,20,0,0);
    o=line_adaptive_step(&s,line_levels_pack(3,3),start+100,20,50,50);
    o=line_adaptive_step(&s,line_levels_pack(3,3),start+200,20,900,900);
    /* Either enclosing recovery bound can stop first; no unbounded crossing. */
    assert(!o.left && !o.right && s.stop_reason==LINE_STOP_RECOVERY_DISTANCE);

    s=(line_adaptive_t){0};
    o=line_adaptive_step(&s,line_levels_pack(90,90),start,20,0,0);
    /* Qualified sightings below the 60 ms confirmation length interleaved
     * with gray prevent line-loss expiry but not the crossing deadline. */
    for(unsigned t=20;t<3500;t+=20)
        o=line_adaptive_step(&s,t%100==20 ? line_levels_pack(80,5):line_levels_pack(60,60),start+t,20,0,0);
    o=line_adaptive_step(&s,line_levels_pack(80,5),start+3500,20,0,0);
    assert(o.mode==LINE_ADAPT_CROSSING_FAILED && s.stop_reason==LINE_STOP_CROSSING_TIME);
    s=(line_adaptive_t){0};
    o=line_adaptive_step(&s,line_levels_pack(90,90),start,20,0,0);
    o=line_adaptive_step(&s,line_levels_pack(80,5),start+100,20,900,900);
    assert(o.mode==LINE_ADAPT_CROSSING_FAILED && s.stop_reason==LINE_STOP_CROSSING_DISTANCE);
}
int main(void)
{
    crossing(0);crossing(UINT32_MAX-1000);
    puts("PASS crossing: bounded advance/reacquisition, non-renewing budgets, reason codes, clock wrap");
}

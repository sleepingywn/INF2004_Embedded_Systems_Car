#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_adaptive.h"

static void intermittent(uint32_t start,unsigned dt,int sign)
{
    line_adaptive_t s={0};
    uint32_t now=start;
    /* Representative of the saved straight-run history: opposite weak edge
     * sightings separated by both-white. Not a full-rate replay of that run. */
    const unsigned pairs[][2]={{50,50},{35,3},{3,3},{3,40},{3,3},
        {32,3},{3,3},{3,45},{3,3},{80,3},{3,3},{3,80},{3,3}};
    for(unsigned n=0;n<sizeof(pairs)/sizeof(pairs[0]);++n) {
        unsigned l=pairs[n][sign>0 ? 0:1],r=pairs[n][sign>0 ? 1:0];
        line_adaptive_output_t out=line_adaptive_step(&s,line_levels_pack(l,r),now,dt,0,0);
        if(out.left==0 || out.right==0)
            fprintf(stderr,"single observation paused wheel: sample=%u dt=%u sign=%d pair=%u,%u target=%ld,%ld\n",
                n,dt,sign,l,r,(long)out.left,(long)out.right);
        assert(out.left>=250 && out.right>=250);
        assert(out.mode==LINE_ADAPT_TRACK || out.mode==LINE_ADAPT_UNCERTAIN);
        now+=dt;
    }
}
static void sustained(uint32_t start,unsigned dt,int sign)
{
    line_adaptive_t s={0};line_adaptive_output_t out={0};
    bool paused=false;
    for(unsigned t=0;t<2000;t+=dt) {
        out=line_adaptive_step(&s,line_levels_pack(sign>0 ? 95:5,sign>0 ? 5:95),start+t,dt,t/4,t/4);
        assert(out.mode==LINE_ADAPT_TRACK);
        if(t<60)assert(out.left && out.right);
        if(t>=300){assert(sign>0 ? out.left==0:out.right==0);paused=true;}
    }
    assert(paused); /* real sustained curves retain tight-turn authority */
    out=line_adaptive_step(&s,line_levels_pack(50,50),start+2020,dt,505,505);
    /* A raw direction reversal must not use the old pause candidate. */
    out=line_adaptive_step(&s,line_levels_pack(sign>0 ? 5:95,sign>0 ? 95:5),start+2020+dt,dt,510,510);
    assert(out.left && out.right);
}
int main(void)
{
    for(unsigned dt=20;dt<=60;dt+=20)for(int sign=-1;sign<=1;sign+=2) {
        intermittent(0,dt,sign);intermittent(UINT32_MAX-200,dt,sign);
        sustained(0,dt,sign);sustained(UINT32_MAX-200,dt,sign);
    }
    puts("PASS: brief/intermittent edge sightings keep rolling; sustained curves retain confirmed pauses");
}

/* Loaded-track regression: rolling 250/600 was followed, yet tight mode never
 * entered on the retained 45..76 errors. Tests verify decisions, not geometry. */
#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static void decisions(int sign,uint32_t start)
{
    line_tight_turn_t t={0};
    assert(line_tight_step(&t,54*sign,start)==LINE_TIGHT_OFF && !t.pending_direction);
    assert(line_tight_step(&t,58*sign,start+20)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,50*sign,start+100)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,45*sign,start+139)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,50*sign,start+140)==LINE_TIGHT_ACTIVE && t.direction==sign);
    assert(line_tight_step(&t,40*sign,start+160)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,40*sign,start+259)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,40*sign,start+260)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,76*sign,start+559)==LINE_TIGHT_OFF); /* cooldown */
    assert(line_tight_step(&t,76*sign,start+560)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,76*sign,start+680)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,76*sign,start+1479)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,76*sign,start+1480)==LINE_TIGHT_TIMEOUT);

    t=(line_tight_turn_t){0};
    line_tight_step(&t,58*sign,start);
    assert(line_tight_step(&t,44*sign,start+100)==LINE_TIGHT_OFF && !t.pending_direction);
    assert(line_tight_step(&t,50*sign,start+120)==LINE_TIGHT_OFF && !t.pending_direction);
    for(unsigned dt=140;dt<=1000;dt+=20)
        assert(line_tight_step(&t,dt%40 ? 76:-76,start+dt)==LINE_TIGHT_OFF);
    t=(line_tight_turn_t){0};
    line_tight_step(&t,58*sign,start);
    line_tight_step(&t,58*sign,start+120);
    assert(line_tight_step(&t,-sign,start+140)==LINE_TIGHT_OFF && !t.direction);
}
static void captured_entry(int sign)
{
    /* 100ms retained samples from firmware times 6106..6906, held between
     * samples as a regression input, not a reconstruction of unseen samples. */
    const int errors[]={38,51,58,55,45,50,50,49,54};
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    int32_t l=0,r=0;unsigned first_pause=0;
    for(unsigned now=20;now<=900;now+=20) {
        if(f.speed.left.target_cps)l+=6;
        if(f.speed.right.target_cps)r+=10;
        line_msg_t line={.state=LINE_ON,.err=sign*errors[(now-20)/100]};
        assert(line_follow_step(&f,0u-(uint32_t)l,(uint32_t)r,0,now,now,
                               &line,now,1,1,true,true)==LINE_STEP_OK);
        if(f.tight.direction) {
            if(!first_pause)first_pause=now;
            assert(f.tight.direction==sign);
            assert(sign>0 ? f.speed.left.duty==0:f.speed.right.duty==0);
            assert(sign>0 ? f.speed.right.target_cps>0:f.speed.left.target_cps>0);
        }
    }
    assert(first_pause==340); /* 120ms after first qualifying 58 sample */
    line_follow_begin(&f,0,0,0,0,0);
    assert(!f.tight.direction && !f.tight.pending_direction);
}
int main(void)
{
    for(int sign=-1;sign<=1;sign+=2) {
        decisions(sign,0);decisions(sign,UINT32_MAX-100u);captured_entry(sign);
    }
    puts("PASS: captured curve triggers inside pause, both directions, noise rejection, dwell hysteresis, exit/cooldown, opposite priority, deadline, reset and wrap");
}

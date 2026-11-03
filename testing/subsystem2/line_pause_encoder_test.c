#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_follow.h"

static line_step_t tick(line_follow_t *f,int32_t l,int32_t r,int err,uint32_t invalid)
{
    uint32_t now=f->previous_ms+20;
    line_msg_t line={.state=LINE_EDGE_ONLY,.err=err};
    return line_follow_step(f,f->origin_left-(uint32_t)l,f->origin_right+(uint32_t)r,
                           invalid,now,now,&line,now,7,7,true,true);
}
static line_follow_t pause(int sign)
{
    line_follow_t f;
    /* Exercise raw and time wrap while entering a real confirmed pause. */
    line_follow_begin(&f,5,UINT32_MAX-5u,0,UINT32_MAX-70u,UINT32_MAX-70u);
    for(int n=1;n<=7;++n)assert(tick(&f,n*20,n*20,100*sign,0)==LINE_STEP_OK);
    assert(f.tight.direction==sign);
    return f;
}
static void both_sides(int sign)
{
    unsigned inside=sign>0 ? 0:1;
    line_follow_t f=pause(sign),saved=f;
    int32_t l=sign>0 ? 139:160,r=sign<0 ? 139:160;
    assert(tick(&f,l,r,sign*97,0)==LINE_STEP_OK);
    assert(f.pause_reverse_used[inside]==1 && !f.encoder_reject);
    assert(f.previous_total[0]==l && f.previous_total[1]==r);
    assert(f.speed.previous_left==l && f.speed.previous_right==r);
    assert(inside ? !f.speed.right.duty:!f.speed.left.duty);
    line_follow_t settled=f;
    /* Second count, even after holding still, exceeds the whole-pause budget. */
    assert(tick(&f,l,r,sign*97,0)==LINE_STEP_OK);
    assert(tick(&f,l-(sign>0),r-(sign<0),sign*97,0)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==(inside ? LINE_ENCODER_RIGHT_RECEDING:LINE_ENCODER_LEFT_RECEDING));
    f=settled;
    assert(tick(&f,l+(sign>0),r+(sign<0),sign*97,0)==LINE_STEP_OK);
    assert(tick(&f,l,r,sign*97,0)==LINE_STEP_ENCODER); /* bounce does not replenish */
    f=saved;
    assert(tick(&f,sign>0 ? 138:160,sign<0 ? 138:160,sign*97,0)==LINE_STEP_ENCODER);
    f=saved;
    assert(tick(&f,139,139,sign*97,0)==LINE_STEP_ENCODER); /* powered outside strict */
    f=saved;
    assert(tick(&f,l,r,sign*97,1)==LINE_STEP_ENCODER && f.encoder_reject==LINE_ENCODER_INVALID_EDGES);
    f=saved;
    assert(tick(&f,sign>0 ? 139:1000,sign<0 ? 139:1000,sign*97,0)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_SPEED && !f.pause_reverse_used[inside]);
    assert(f.speed.previous_left==saved.speed.previous_left && f.speed.previous_right==saved.speed.previous_right);
    /* Exit on the settling sample: account for the previously stopped interval,
     * then restart normally. Subsequent powered reversal must still fault. */
    f=saved;
    assert(tick(&f,l,r,-sign,0)==LINE_STEP_OK && !f.tight.direction);
    assert(f.speed.left.target_cps>0 && f.speed.right.target_cps>0);
    assert(!f.pause_reverse_used[inside]);
    line_follow_t resumed=f;
    assert(tick(&f,l,r,0,0)==LINE_STEP_OK); /* no hidden negative delta on restart */
    f=resumed;
    assert(tick(&f,l-(sign>0),r-(sign<0),0,0)==LINE_STEP_ENCODER);
    /* Complete cooldown and enter another pause; it gets a new one-count budget. */
    f=resumed;
    for(int n=0;n<30;++n) {
        l=f.previous_total[0]+(f.speed.left.target_cps ? 10:0);
        r=f.previous_total[1]+(f.speed.right.target_cps ? 10:0);
        assert(tick(&f,l,r,sign*100,0)==LINE_STEP_OK);
        if(f.tight.direction)break;
    }
    assert(f.tight.direction==sign);
    assert(tick(&f,l-(sign>0),r-(sign<0),sign*100,0)==LINE_STEP_OK);
    assert(f.pause_reverse_used[inside]==1);
    line_follow_begin(&f,0,0,0,0,0);
    assert(!f.pause_reverse_used[0] && !f.pause_reverse_used[1]);
    assert(tick(&f,10,10,sign*100,0)==LINE_STEP_OK);
    assert(tick(&f,9,9,sign*100,0)==LINE_STEP_ENCODER); /* pending is not paused */
}
int main(void)
{
    both_sides(1);both_sides(-1);
    puts("PASS: paused-wheel one-count budget, no renewal on bounce, strict powered/invalid/rate guards, restart and wrap");
}

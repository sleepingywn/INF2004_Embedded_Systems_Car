#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../app_program/subsystem2/line_follow.h"

static void speed_reasons(void)
{
    speed_control_t c,before;speed_control_start(&c);before=c;unsigned reason;
    assert(!speed_control_targets_step_report(&c,0,0,0,700,700,&reason) && reason==SPEED_REJECT_TIME);
    assert(!memcmp(&c,&before,sizeof(c)));
    assert(!speed_control_targets_step_report(&c,0,0,20000,-1,700,&reason) && reason==SPEED_REJECT_TARGET);
    assert(!speed_control_targets_step_report(&c,-1,-1,20000,700,700,&reason) &&
           reason==(SPEED_REJECT_LEFT_REVERSE|SPEED_REJECT_RIGHT_REVERSE));
    assert(!speed_control_targets_step_report(&c,201,0,20000,700,700,&reason) && reason==SPEED_REJECT_LEFT_RATE);
    assert(!speed_control_targets_step_report(&c,0,201,20000,700,700,&reason) && reason==SPEED_REJECT_RIGHT_RATE);
    assert(!memcmp(&c,&before,sizeof(c))); /* rejection never partly updates one wheel */
    speed_control_t wrapper=c;
    for(int i=1;i<=20;++i) {
        assert(speed_control_targets_step_report(&c,i*20,i*24,20000,700,500,&reason) && reason==0);
        assert(speed_control_targets_step(&wrapper,i*20,i*24,20000,700,500));
        assert(!memcmp(&c,&wrapper,sizeof(c)));
    }
}
static void line_reasons(void)
{
    line_follow_t f;line_msg_t line={.state=LINE_ON};
    line_follow_begin(&f,0,0,7,0,0);
    assert(line_follow_step(&f,0,0,8,20,20,&line,20,1,1,true,true)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_INVALID_EDGES && !f.speed_reject);
    line_follow_begin(&f,0,0,0,0,0);
    assert(line_follow_step(&f,1,0,0,20,20,&line,20,1,1,true,true)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==(LINE_ENCODER_LEFT_NEGATIVE|LINE_ENCODER_LEFT_RECEDING));
    line_follow_begin(&f,0,0,0,0,0);
    assert(line_follow_step(&f,0,UINT32_MAX,0,20,20,&line,20,1,1,true,true)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==(LINE_ENCODER_RIGHT_NEGATIVE|LINE_ENCODER_RIGHT_RECEDING));
    line_follow_begin(&f,0,0,0,0,0);
    assert(line_follow_step(&f,(uint32_t)-20,20,0,20,20,&line,20,1,1,true,true)==LINE_STEP_OK);
    line_follow_t saved=f;
    assert(line_follow_step(&f,(uint32_t)-19,40,0,40,40,&line,40,1,1,true,true)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_LEFT_RECEDING);
    f=saved;
    assert(line_follow_step(&f,(uint32_t)-40,19,0,40,40,&line,40,1,1,true,true)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_RIGHT_RECEDING);
    f=saved;
    assert(line_follow_step(&f,(uint32_t)-40,1000,0,40,40,&line,40,1,1,true,true)==LINE_STEP_ENCODER);
    assert(f.encoder_reject==LINE_ENCODER_SPEED && f.speed_reject==SPEED_REJECT_RIGHT_RATE);
    assert(f.rejected_target_left==700 && f.rejected_target_right==700);
    /* A later non-encoder result cannot inherit an earlier reason. */
    line.state=LINE_LOST;
    assert(line_follow_step(&f,0,0,0,40,40,&line,40,1,1,true,true)==LINE_STEP_IR);
    assert(!f.encoder_reject && !f.speed_reject);
    line.state=LINE_ON;
    line_follow_begin(&f,5,UINT32_MAX-5u,0,UINT32_MAX-10u,UINT32_MAX-10u);
    assert(line_follow_step(&f,UINT32_MAX-14u,14,0,9,9,&line,9,1,1,true,true)==LINE_STEP_OK);
    assert(!f.encoder_reject && !f.speed_reject); /* normal count/time wrap */
}
int main(void)
{
    speed_reasons();line_reasons();
    puts("PASS: exact encoder rejection flags, one-count reversal, invalid edges/rates, unchanged speed wrapper and wrap");
}

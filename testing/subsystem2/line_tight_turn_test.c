/* Bounded tight-turn decisions and motor-loop regressions, not track geometry. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../../app_program/subsystem2/line_follow.h"

static void decisions(void)
{
    line_tight_turn_t t={0};
    assert(line_tight_step(&t,85,0)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,84,100)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,85,120)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,85,239)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,85,240)==LINE_TIGHT_ACTIVE && t.direction==1);
    assert(line_tight_step(&t,80,260)==LINE_TIGHT_ACTIVE); /* no threshold chatter */
    assert(line_tight_step(&t,45,280)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,46,360)==LINE_TIGHT_ACTIVE); /* reset exit dwell */
    assert(line_tight_step(&t,45,400)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,45,500)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,100,600)==LINE_TIGHT_OFF); /* cooldown */
    assert(line_tight_step(&t,100,800)==LINE_TIGHT_OFF);
    assert(line_tight_step(&t,100,920)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,0,940)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,0,1139)==LINE_TIGHT_ACTIVE);
    assert(line_tight_step(&t,0,1140)==LINE_TIGHT_UNSEEN); /* zero is not successful exit */
    for(int sign=-1;sign<=1;sign+=2) {
        uint32_t start=UINT32_MAX-50u;t=(line_tight_turn_t){0};
        assert(line_tight_step(&t,100*sign,start)==LINE_TIGHT_OFF);
        assert(line_tight_step(&t,100*sign,start+120)==LINE_TIGHT_ACTIVE && t.direction==sign);
        assert(line_tight_step(&t,100*sign,start+919)==LINE_TIGHT_ACTIVE);
        assert(line_tight_step(&t,100*sign,start+920)==LINE_TIGHT_TIMEOUT);
        t=(line_tight_turn_t){0};
        line_tight_step(&t,100*sign,start);line_tight_step(&t,100*sign,start+120);
        assert(line_tight_step(&t,-sign,start+140)==LINE_TIGHT_OFF && !t.direction);
    }
    t=(line_tight_turn_t){0};
    for(unsigned now=0;now<1000;now+=20)
        assert(line_tight_step(&t,now%40 ? -100:100,now)==LINE_TIGHT_OFF);
    puts("PASS: confirmation, hysteresis, exit dwell, cooldown, opposite priority, unseen/deadline and wrap");
}

static void guards(void)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    line_msg_t line={.state=LINE_EDGE_ONLY,.err=100};
    uint32_t left=0;
    for(uint32_t now=20;now<=400;now+=20) {
        if(f.speed.left.target_cps)left+=20;
        assert(line_follow_step(&f,0u-left,now,0,now,now,&line,now,7,7,true,true)==LINE_STEP_OK);
        if(now>=140)assert(f.tight.direction==1 && !f.speed.left.duty && !f.speed.left.target_cps);
    }
    line_follow_t saved=f;
    line.state=LINE_LOST;
    assert(line_follow_step(&f,0u-left,420,0,420,420,&line,420,7,7,true,true)==LINE_STEP_IR);
    line.state=LINE_BOTH_BLACK;
    assert(line_follow_step(&f,0u-left,420,0,420,420,&line,420,7,7,true,true)==LINE_STEP_IR);
    line.state=LINE_EDGE_ONLY;line.fault=1;
    assert(line_follow_step(&f,0u-left,420,0,420,420,&line,420,7,7,true,true)==LINE_STEP_IR);
    line.fault=0;
    assert(line_follow_step(&f,0u-left,420,0,420,420,&line,300,7,7,true,true)==LINE_STEP_IR);
    assert(line_follow_step(&f,0u-left,420,0,420,420,&line,420,7,8,true,true)==LINE_STEP_IR);
    assert(line_follow_step(&f,0u-left,420,1,420,420,&line,420,7,7,true,true)==LINE_STEP_ENCODER);
    assert(line_follow_step(&f,0u-left,420,0,420,420,&line,420,7,7,true,false)==LINE_STEP_DISCONNECTED);
    assert(line_follow_step(&f,0u-left,420,0,461,461,&line,461,7,7,true,true)==LINE_STEP_TIME);
    f=saved;
    for(uint32_t now=420;now<600;now+=20)
        assert(line_follow_step(&f,0u-left,400,0,now,now,&line,now,7,7,true,true)==LINE_STEP_OK);
    assert(line_follow_step(&f,0u-left,400,0,600,600,&line,600,7,7,true,true)==LINE_STEP_STALL);
    line_follow_begin(&f,0,0,0,0,0);
    assert(!f.tight.direction && !f.tight.cooldown && !f.tight.pending_direction);
    puts("PASS: deliberate inside pause, independent outer stall, all input/encoder/USB/time guards and reset");
}

static void alternating_model(unsigned model)
{
    line_follow_t f;line_follow_begin(&f,0,0,0,0,0);
    line_msg_t line={.state=LINE_ON};
    double lv=0,rv=0,lp=0,rp=0;
    uint32_t now=0;unsigned ticks=0,entries=0,restarts=0;int previous_direction=0;
    bool left_pause=false,right_pause=false;
    while(now<1800) {
        uint32_t dt=model==2 ? 20+20*(ticks++%3):20;
        double sec=dt/1000.0,tau=model==2 ? .15:.08;
        double dead=model==0 ? 30:90,gain=model==0 ? 12:14;
        lv+=(fmax(0,(double)f.speed.left.duty-dead)*gain-lv)*sec/(tau+sec);
        rv+=(fmax(0,(double)f.speed.right.duty-dead)*gain-rv)*sec/(tau+sec);
        lp+=lv*sec;rp+=rv*sec;now+=dt;
        line.err=now<400 ? 100:now<560 ? 40:now<900 ? 0:now<1300 ? -100:now<1460 ? -40:0;
        unsigned old_l=f.speed.left.duty,old_r=f.speed.right.duty;
        int32_t old_lt=f.speed.left.target_cps,old_rt=f.speed.right.target_cps;
        assert(line_follow_step(&f,0u-(uint32_t)lp,(uint32_t)rp,0,now,now,&line,now,7,7,true,true)==LINE_STEP_OK);
        if(f.tight.direction && !previous_direction)++entries;
        if(!f.tight.direction && previous_direction)++restarts;
        previous_direction=f.tight.direction;
        if(f.tight.direction>0){left_pause=true;assert(!f.speed.left.duty && !f.speed.left.target_cps);}
        if(f.tight.direction<0){right_pause=true;assert(!f.speed.right.duty && !f.speed.right.target_cps);}
        wheel_speed_t *wheels[]={&f.speed.left,&f.speed.right};
        unsigned old_duty[]={old_l,old_r};int32_t old_target[]={old_lt,old_rt};
        for(unsigned side=0;side<2;++side) {
            wheel_speed_t *w=wheels[side];assert(w->duty<=200 && w->target_cps>=0);
            if(w->target_cps && old_target[side]) {
                int delta=(int)w->duty-(int)old_duty[side];
                assert(delta>=-(int)(500*dt/1000) && delta<=(int)(250*dt/1000));
            }
            /* Zero commands stop immediately; one bounded feed-forward restart
             * per exit is deliberate, not claimed to obey the usual PWM slew. */
        }
    }
    assert(left_pause && right_pause && entries==2 && restarts==2);
    printf("PASS: model %u repeated opposite tight turns, two deliberate restarts, capped outputs and ordinary slew\n",model);
}
int main(void)
{
    decisions();guards();
    for(unsigned i=0;i<3;++i)alternating_model(i);
    return 0;
}

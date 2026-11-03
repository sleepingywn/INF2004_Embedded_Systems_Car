/* Actual motion.c owner loop with fake kernel, time, motors and encoder. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "motion_host_shim.h"
#include "../../app_program/subsystem2/motion.c"
#undef memset
#undef memcpy
static jmp_buf done;
static uint32_t clock_ms,flag_bits;
static int electrical_left,electrical_right;
static encoder_raw_t fake_raw;
static unsigned scenario,energised;
static int first_electrical_left,first_electrical_right;
static bool sent_request,stop_injected;
static uint32_t stopped_at,completed_at;
static uint32_t idle_stop_epoch;
static motion_result_t idle_stop_result;
UW in_w(UW reg){uint64_t t=(uint64_t)clock_ms*1000;return reg==TIMER_TIMERAWH?(UW)(t>>32):(UW)t;}
UW tm_usb_state(void){return scenario==3 && clock_ms>=300 ? 2:3;}
static unsigned lock_busy_polls,lock_calls,lock_waits;
static ER lock_wait_result,lock_forced_error;
static void (*lock_hook)(ID);
ER tk_loc_mtx(ID a,int b){
    if(lock_hook)lock_hook(a);
    (void)a;++lock_calls;
    if(lock_forced_error)return lock_forced_error;
    if(b!=TMO_POL){assert(b==2);++lock_waits;return lock_wait_result;}
    if(lock_busy_polls){--lock_busy_polls;return E_TMOUT;}
    return E_OK;
}
ER tk_unl_mtx(ID a){(void)a;return E_OK;}
ER tk_set_flg(ID a,UINT b){(void)a;flag_bits|=b;return E_OK;}
ER tk_clr_flg(ID a,UINT b){(void)a;flag_bits&=b;return E_OK;}
ER tk_ref_flg(ID a,T_RFLG *b){(void)a;b->flgptn=flag_bits;return E_OK;}
ID tk_cre_mtx(const T_CMTX*a){(void)a;return 1;}
ID tk_cre_flg(const T_CFLG*a){(void)a;return 1;}
ID tk_cre_cyc(const T_CCYC*a){(void)a;return 1;}
ID app_start_task(FP a,int b,int c,const char*d){(void)a;(void)b;(void)c;(void)d;return 1;}
ER tk_sta_cyc(ID a){(void)a;return E_OK;}
ER motor_hw_init(void){return E_OK;}
ER encoder_hw_init(void){return E_OK;}
void encoder_hw_snapshot(encoder_raw_t *r){
    *r=fake_raw;
    /* Cross a millisecond boundary during acquisition, after admission. */
    if(scenario==14 && clock_ms>=100u)++clock_ms;
}
void motor_hw_stop(void){if(electrical_left||electrical_right)stopped_at=clock_ms;electrical_left=electrical_right=0;}
void motor_hw_write(int16_t l,int16_t r){
    assert(l>=-200 && l<=0 && r>=0 && r<=200);
    if((l||r) && !energised){first_electrical_left=l;first_electrical_right=r;}
    if(l||r)++energised;
    electrical_left=l;electrical_right=r;
}
#if MOTION_LINE_ADAPTIVE
static bc_msg_t fixture_barcode(char command)
{
    bc_msg_t bc={0};bc.hdr.count=29;
    const unsigned code[]={0x109u,0x049u,0x148u,0x019u};
    for(unsigned i=0;i<29;++i)bc.widths[i]=10;
    for(unsigned n=0;n<3;++n) {
        unsigned mask=n==1 ? code[command-'A']:0x094u;
        for(unsigned i=0;i<9;++i)bc.widths[n*10+i]=(mask&(1u<<(8-i))) ? 30:10;
    }
    assert(motion_barcode_decode(&bc)==command);return bc;
}
#endif
ER tk_wai_flg(ID a,UINT b,UINT c,UINT*d,int e){
    (void)a;(void)b;(void)c;(void)e;
    /* Completion time is not necessarily the last nonzero PWM time: a
     * correctly reducing speed loop may already have coasted to zero. */
    if(sent_request && state.result==MOTION_COMPLETED && !completed_at)
        completed_at=clock_ms;
    if(clock_ms>=LINE_FOLLOW_MAX_MS+400u)longjmp(done,1);
    clock_ms+=20;
    unsigned ticks_per_step=scenario>=39 ? 6u:24u;
    if(electrical_left && scenario!=4)fake_raw.left-=ticks_per_step;
    if(electrical_right && !(scenario==23 && clock_ms>=200))fake_raw.right+=ticks_per_step;
    if(scenario==5 && clock_ms>=300)fake_raw.invalid=1;
    line_msg_t line={.state=LINE_ON};
    if(scenario>=30) {
        line.state=LINE_LOST;
        if((scenario==31 || scenario==32) && clock_ms>=900) {line.state=LINE_ON;line.err=40;}
        if(scenario==32 && clock_ms>=1500){line.state=LINE_LOST;line.err=0;}
        if(scenario==34 && clock_ms>=300)fake_raw.invalid=1;
        if(scenario==35 && clock_ms>=300)line.state=LINE_BOTH_BLACK;
        if(scenario==37 && clock_ms>=300)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
        if(scenario==38 && clock_ms%1000>=400 && clock_ms%1000<=500){line.state=LINE_ON;line.err=40;}
        if((scenario==36 && clock_ms>=300) || (scenario==31 && clock_ms>=1300) ||
           (scenario==32 && clock_ms>=2600)) {
            if(!stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
        }
    }
    if(scenario>=39) {
        line=(line_msg_t){.state=LINE_ON,.err=clock_ms<240 ? 100:11};
        if(clock_ms>=300)line=(line_msg_t){.state=LINE_BOTH_BLACK};
        if(scenario==39 && clock_ms>=360)line=(line_msg_t){.state=LINE_ON,.err=-40};
        if(scenario==41 && clock_ms==320)fake_raw.right+=80;
        if(scenario==44 && clock_ms>=320)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
        if(scenario==45 && clock_ms>=320)line.fault=1;
        if((scenario==39 && clock_ms>=600) || (scenario==42 && clock_ms>=320)) {
            if(!stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
        }
    }
    if((scenario==2 || scenario==15) && clock_ms>=300)line.state=LINE_LOST;
    if(scenario==11 && clock_ms>=LINE_FOLLOW_MAX_MS-500u)line.state=LINE_LOST;
    if(scenario==12 && clock_ms>=LINE_FOLLOW_MAX_MS/2u)line.state=LINE_BOTH_BLACK;
    if(scenario==8)line.err=60; /* full rolling curve below tight-turn threshold */
    if(scenario==9)line.err=-60;
    if(scenario>=16 && scenario<=29) {
        line.err=(scenario==17 || scenario==25 || scenario==29) ? -100:100;
        if(scenario==16 || scenario==17)
            line.err=clock_ms<400 ? line.err:clock_ms<600 ? (scenario==17 ? -40:40):0;
        if(scenario==19 && clock_ms>=400)line.err=0;
        if(scenario==21 && clock_ms>=400)line.state=LINE_LOST;
        if(scenario==22 && clock_ms>=400)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
        if(scenario==20 && clock_ms>=400 && !stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
        if(clock_ms==200) {
            if(scenario==24 || scenario==28)++fake_raw.left;
            if(scenario==25 || scenario==29)--fake_raw.right;
            if(scenario==26)++fake_raw.invalid;
            if(scenario==27)fake_raw.right+=1000;
        }
        if(clock_ms==220) {
            if(scenario==24)++fake_raw.left; /* second backward tick must fault */
            if(scenario==25)--fake_raw.right;
        }
        if(scenario>=24 && clock_ms==300){assert(motion_stop()==E_OK);stop_injected=true;}
    }
#if MOTION_LINE_ADAPTIVE
    if(scenario>=50) {
        line=(line_msg_t){.state=LINE_EDGE_ONLY,.err=90};
        if(scenario==51 || scenario==73 || scenario==75 || (scenario>=77 && scenario<=88 && scenario%2==0))line.err=-90;
        if(scenario==52 && clock_ms>=300)line=(line_msg_t){.state=LINE_BOTH_BLACK};
        if(scenario==53 && clock_ms>=300)line=(line_msg_t){.state=LINE_LOST};
        if((scenario==50 || scenario==51) && clock_ms>=2200 && !stop_injected) {
            assert(motion_stop()==E_OK);stop_injected=true;
        }
        if(scenario==55 && clock_ms>=300)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
        if(scenario==56 && clock_ms>=300)fake_raw.invalid=1;
        if(scenario==57 && clock_ms>=300)line.fault=1;
        if(scenario>=59 && scenario<=63 && clock_ms>=300) {
            line=(line_msg_t){.state=LINE_BOTH_BLACK};
            if(scenario==59 && clock_ms>=600)line=(line_msg_t){.state=LINE_LOST};
            if(scenario==59 && clock_ms>=700)line=(line_msg_t){.state=LINE_EDGE_ONLY,.err=40};
            if(scenario==62 && clock_ms>=320)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
            if(scenario==63 && clock_ms>=320)fake_raw.invalid=1;
        }
        if((scenario==60 && clock_ms>=320) ||
           ((scenario==59 || scenario==65 || scenario==66 || scenario==67 || scenario==70 || scenario==71) && clock_ms>=1000)) {
            if(!stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
        }
        if((scenario>=64 && scenario<=70 && clock_ms==300) ||
           (scenario==70 && clock_ms==320) || (scenario==71 && clock_ms==40)) {
            char command=scenario==65 || (scenario==70 && clock_ms==300) ? 'C':scenario==68 ? 'B':scenario==69 ? 'D':'A';
            shared.barcode=fixture_barcode(command);shared.have_barcode=true;
            shared.barcode_ms=scenario==66 ? 0:scenario==70 ? 300:scenario==71 ? 20:clock_ms;
            shared.barcode_epoch=scenario==67 ? state.epoch+1:state.epoch;
        }
        if(scenario>=72 && scenario<=75 && clock_ms==500) {
            assert(scenario%2 ? line_pause_eligible(&line_trial)==2u:line_pause_eligible(&line_trial)==1u);
            unsigned amount=scenario>=74 ? 2u:1u;
            if(scenario%2)fake_raw.right-=amount;else fake_raw.left+=amount;
        }
        if((scenario==72 || scenario==73) && clock_ms==520) {
            if(scenario%2)--fake_raw.right;else ++fake_raw.left;
        }
        if(scenario==76 && clock_ms==500)fake_raw.right=state.right_count-1u;
        if(scenario>=77 && scenario<=88) {
            unsigned inside=scenario%2 ? 0u:1u;
            if(line_pause_eligible(&line_trial)&(1u<<inside)) {
                uint32_t age=clock_ms-line_trial.output_pause_ms[inside];
                unsigned reverse=0;
                if(scenario<=80 && (age==40 || age==60 || (scenario>=79 && age==80)))reverse=1;
                if((scenario==81 || scenario==82) && age==40)reverse=3;
                if((scenario==83 || scenario==84) && age==120)reverse=1;
                if(scenario>=85 && age==40)reverse=1;
                if(reverse) {if(inside)fake_raw.right-=reverse;else fake_raw.left+=reverse;}
                if(scenario==85 && age==40)++fake_raw.invalid;
                if(scenario==86 && age==40)fake_raw.left=state.left_count+1u; /* powered outside */
                if(scenario==87 && age==60 && !stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
                if(scenario==88 && age==60)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
            }
            if((scenario==77 || scenario==78) && clock_ms>=1000 && !stop_injected) {
                assert(line_trial.pause_reverse_used[inside]==2);
                assert(motion_stop()==E_OK);stop_injected=true;
            }
        }
    }
#endif
    if(!(((scenario==7 || scenario==33) && clock_ms>=200) || (scenario==43 && clock_ms>=320)
        || (scenario==54 && clock_ms>=300) || (scenario==61 && clock_ms>=320))) {
        shared.line=line;shared.have_line=true;shared.line_ms=clock_ms;shared.line_epoch=state.epoch;
#if MOTION_LINE_ADAPTIVE
        if(scenario>=50) {
            shared.line_levels=line.state==LINE_BOTH_BLACK ? line_levels_pack(90,90):
                line.state==LINE_LOST ? line_levels_pack(3,3):
                scenario==59 && clock_ms>=700 ? line_levels_pack(45,5):
                line.err>0 ? line_levels_pack(95,5):line_levels_pack(5,95);
            if(scenario==58 && clock_ms>=300)shared.line_levels=0;
        }
#endif
    }
    if(clock_ms==40 && scenario==13) {
        idle_stop_epoch=state.epoch;idle_stop_result=state.result;
        assert(motion_stop()==E_OK);
    }
    if(clock_ms==40 && scenario!=13) {
        motion_request_t request;
        assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_LOCAL,state.epoch,1,LINE_FOLLOW_MAX_MS,0,0,0)==E_OK);
        assert(motion_submit(&request)==E_OK);sent_request=true;
    }
    if(clock_ms==100 && scenario!=13) {
        motion_request_t request;
        assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_LOCAL,state.epoch,2,LINE_FOLLOW_MAX_MS,0,0,0)==E_OK);
        assert(motion_submit(&request)==E_OBJ); /* no queued/replayed motion */
        assert(motion_renew(state.epoch,1,MOTION_LOCAL,clock_ms,LINE_FOLLOW_MAX_MS)==E_OBJ);
    }
    if(scenario==1 && clock_ms>=300)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
    if(scenario==10 && clock_ms>=LINE_FOLLOW_MAX_MS/2u)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
    if(scenario==6 && clock_ms>=300 && !stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
    if(scenario==15 && clock_ms>=400 && !stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
    *d=flag_bits;return E_OK;
}
static void reset_fixture(void){
    clock_ms=flag_bits=energised=stopped_at=completed_at=0;electrical_left=electrical_right=0;
    sent_request=stop_injected=false;fake_raw=(encoder_raw_t){0};
    first_electrical_left=first_electrical_right=0;
    memset(&shared,0,sizeof(shared));memset(&owner_flags,0,sizeof(owner_flags));
    memset(&state,0,sizeof(state));memset(&diagnostics,0,sizeof(diagnostics));
    memset(&active,0,sizeof(active));memset(wheels,0,sizeof(wheels));
    previous_counts=(encoder_raw_t){0};output_rest_until=last_invalid=owner_rejections=0;
    stop_generation=fault_generation=handled_stop_generation=handled_fault_generation=0;
    line_history_clear(0,0);
    reset_control();assert(motion_init()==E_OK);assert(motion_application_ready()==E_OK);
}
static void history_cases(void){
    reset_fixture();line_history_clear(7,8);
    line_follow_begin(&line_trial,0,0,0,0,0);
    line_msg_t line={.state=LINE_ON,.err=-60};state.epoch=7;
    owner_flags.active_valid=true;
    motion_line_trace_entry_t entry={.id=999};
    for(uint32_t t=100;t<=7000;t+=100) {
        line_trial.steering_error=t<2000 ? -20:0;
        line_trial.speed.left.target_cps=700;line_trial.speed.right.target_cps=400;
        line_trial.speed.left.filtered_milli_cps=690000;line_trial.speed.right.filtered_milli_cps=410000;
        line_history_record(t,&line,t-3,7,true,LINE_STEP_OK);
        unsigned count=line_history.count,next=line_history.next;
        line_history_record(t+20,&line,t+17,7,true,LINE_STEP_OK);
        assert(line_history.count==count && line_history.next==next);
    }
    assert(line_history.count==64);
    publish_observer_status();assert(motion_get_line_trace(0,&entry)==E_OBJ && entry.id==999);
    line.state=LINE_LOST;line.err=0;
    line_history_record(7020,&line,7017,7,true,LINE_STEP_IR); /* forced final record */
    end_request(MOTION_FAILED);publish_observer_status();
    for(unsigned i=0;i<64;++i) {
        assert(motion_get_line_trace(i,&entry)==E_OK && entry.count==64 && entry.epoch==7 && entry.id==8);
        assert(entry.sample.elapsed_ms==(i==63 ? 7020:800+i*100));
        assert(entry.sample.age_ms==3 && entry.sample.flags==6);
        assert(entry.sample.target_left_cps==700 && entry.sample.target_right_cps==400);
        assert(entry.sample.speed_left_cps==690 && entry.sample.speed_right_cps==410);
        assert(entry.sample.state==(i==63 ? LINE_LOST:LINE_ON));
        assert(entry.sample.step==(i==63 ? LINE_STEP_IR:LINE_STEP_OK));
    }
    assert(motion_get_line_trace(64,&entry)==E_NOEXS);
    assert(motion_get_line_trace(0,NULL)==E_PAR);
    lock_busy_polls=1;entry.id=999;
    assert(motion_get_line_trace(0,&entry)==E_TMOUT && entry.id==999);
    line_history_clear(8,9);publish_observer_status();
    assert(motion_get_line_trace(0,&entry)==E_NOEXS);
    assert(!energised);
    puts("PASS: bounded chronological history, sampling throttle, terminal input, stopped-only snapshot, contention and clear");
}
static void admission_cases(void){
    reset_fixture();motion_request_t request;
    assert(!calibrated());
    assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_NETWORK,1,1,LINE_FOLLOW_MAX_MS,0,0,0)==E_PAR);
    assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_LOCAL,1,1,LINE_FOLLOW_MAX_MS+1u,0,0,0)==E_PAR);
    assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_LOCAL,1,1,2000,0,1,0)==E_PAR);
    assert(motion_request_init(&request,MOTION_FOLLOW,MOTION_LOCAL,1,1,2000,0,90,0)==E_PAR);
    assert(motion_request_init(&request,MOTION_DISTANCE,MOTION_LOCAL,1,1,2000,100,90,0)==E_PAR);
    assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_LOCAL,1,1,LINE_FOLLOW_MAX_MS,0,0,0)==E_OK);
    state.raw_valid=state.ready=1;owner_flags.motor_ready=owner_flags.encoder_ready=true;
    line_msg_t line={.state=LINE_CALIBRATING};
    admit(&request,0,0,&line,0,1,true);assert(!owner_flags.active_valid && !energised);
    line.state=LINE_ON;
    admit(&request,0,SOURCE_BIT(MOTION_HEALTH_IR),&line,0,1,true);assert(!owner_flags.active_valid);
    admit(&request,0,0,&line,0,2,true);assert(!owner_flags.active_valid);
    diagnostics.line_failure_flags=7;
    admit(&request,0,0,&line,0,1,true);assert(owner_flags.active_valid && state.state==MOTION_TRACING);
    assert(!diagnostics.line_failure_flags);
    assert(!state.calibrated && !state.valid);end_request(MOTION_CANCELLED);
}
static void snapshot_contention_cases(void){
    reset_fixture();
    observer_status.epoch=37;
    motion_status_t snapshot={.epoch=99};
    lock_busy_polls=1;
    assert(motion_get_status(&snapshot)==E_TMOUT && snapshot.epoch==99);
    lock_busy_polls=3;
    assert(motion_get_ir_status(&snapshot)==E_OK && snapshot.epoch==37);
    lock_busy_polls=50;lock_waits=0;lock_wait_result=E_OK;
    assert(motion_get_ir_status(&snapshot)==E_OK && lock_waits==1);
    lock_busy_polls=50;lock_calls=0;lock_wait_result=E_TMOUT;snapshot.epoch=99;
    assert(motion_get_ir_status(&snapshot)==E_TMOUT && snapshot.epoch==99 && lock_calls==51);
    lock_forced_error=E_OBJ;lock_calls=0;
    assert(motion_get_ir_status(&snapshot)==E_OBJ && lock_calls==1);
    lock_forced_error=0;lock_wait_result=E_OK;lock_busy_polls=0;
    assert(motion_get_ir_status(NULL)==E_PAR);
    puts("PASS: IR snapshot transient contention, bounded wait, persistent failure, observer remains nonblocking");
}
int main(void){
    snapshot_contention_cases();
    if(!MOTION_LINE_DEMO) {
        reset_fixture();motion_request_t r;
        assert(motion_request_init(&r,MOTION_LINE_TRACE,MOTION_LOCAL,1,1,LINE_FOLLOW_MAX_MS,0,0,0)==E_PAR);
        assert(!energised);puts("PASS: default build rejects local line-trace requests");return 0;
    }
    admission_cases();
    history_cases();
    scenario=13;reset_fixture();
    if(!setjmp(done))motion_task(0,NULL);
    assert(!energised && !electrical_left && !electrical_right);
    assert(state.epoch!=idle_stop_epoch && state.result==idle_stop_result);
    assert(state.state==MOTION_IDLE && !owner_flags.active_valid);
    puts("PASS: actual idle STOP advances epoch but retains prior result (USB BOOTSEL regression)");
    scenario=14;reset_fixture();
    if(!setjmp(done))motion_task(0,NULL);
    assert(sent_request && energised && !electrical_left && !electrical_right);
    assert(!state.faults && state.result==MOTION_COMPLETED);
    assert(completed_at>=LINE_FOLLOW_MAX_MS+40u && completed_at<=LINE_FOLLOW_MAX_MS+61u);
    puts("PASS: encoder acquisition crossing a millisecond does not cause a false stale-sample timeout");
    scenario=15;reset_fixture();
    if(!setjmp(done))motion_task(0,NULL);
    assert(stop_injected && !electrical_left && !electrical_right);
    assert(state.faults&MOTION_FAULT_IR);
    assert(diagnostics.line_failure_flags==7 && diagnostics.line_failure_state==LINE_LOST);
    assert(diagnostics.line_failure_elapsed_ms==260 && diagnostics.line_failure_age_ms==0);
    motion_line_trace_entry_t last;
    assert(motion_get_line_trace(line_history.count-1,&last)==E_OK);
    assert(last.sample.state==LINE_LOST && last.sample.step==LINE_STEP_IR && last.sample.elapsed_ms==260);
    motion_request_t reset;
    assert(motion_request_init(&reset,MOTION_RESET,MOTION_LOCAL,state.epoch,3,2000,0,0,0)==E_OK);
    admit(&reset,clock_ms,0,NULL,0,0,false);
    assert(state.result==MOTION_COMPLETED && !state.faults && !diagnostics.line_failure_flags);
    publish_observer_status();assert(motion_get_line_trace(0,&last)==E_NOEXS);
    puts("PASS: failing input survives subsequent STOP and clears on accepted reset/new run");
    for(scenario=16;scenario<=23;++scenario) {
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(energised && !electrical_left && !electrical_right && !owner_flags.active_valid);
        if(scenario<=17)assert(state.result==MOTION_COMPLETED && !state.faults);
        if(scenario==18)assert(state.faults==MOTION_FAULT_TIMEOUT && diagnostics.line_step==LINE_STEP_TIGHT_TIMEOUT);
        if(scenario==19)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_TIGHT_UNSEEN);
        if(scenario==20)assert(state.result==MOTION_CANCELLED && !state.faults);
        if(scenario==21)assert(state.faults==MOTION_FAULT_IR);
        if(scenario==22)assert(state.faults==MOTION_FAULT_EXTERNAL);
        if(scenario==23)assert(state.faults==MOTION_FAULT_STALL);
        if(scenario>=18) {
            bool paused=false;
            for(unsigned i=0;i<line_history.count;++i) {
                motion_line_trace_entry_t entry;
                assert(motion_get_line_trace(i,&entry)==E_OK);
                if(entry.sample.tight_direction==1 && entry.sample.duty_left==0 && entry.sample.duty_right>0)paused=true;
            }
            assert(paused);
        }
        printf("tight-turn owner scenario %u PASS\n",scenario);
    }
    for(scenario=24;scenario<=27;++scenario) {
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(stop_injected && state.faults==MOTION_FAULT_ENCODER && !electrical_left && !electrical_right);
        motion_encoder_failure_t *e=&diagnostics.encoder_failure;
        unsigned expected=scenario==24 ? LINE_ENCODER_LEFT_RECEDING:scenario==25 ? LINE_ENCODER_RIGHT_RECEDING:
                          scenario==26 ? LINE_ENCODER_INVALID_EDGES:LINE_ENCODER_SPEED;
        assert(e->reason==expected && e->raw_valid && e->dt_ms==20);
        assert(diagnostics.line_failure_elapsed_ms==(scenario<=25 ? 180u:160u));
        if(scenario==24)assert(motion_count_delta(e->raw_left,e->previous_left)==1 && e->target_left==0);
        if(scenario==25)assert(motion_count_delta(e->raw_right,e->previous_right)==-1 && e->target_right==0);
        if(scenario==26)assert(e->invalid==e->initial_invalid+1);
        if(scenario==27)assert(e->speed_reason==SPEED_REJECT_RIGHT_RATE);
        motion_diagnostics_t snapshot;
        assert(motion_get_diagnostics(&snapshot)==E_OK && snapshot.encoder_failure.reason==expected);
        motion_request_t request;
        assert(motion_request_init(&request,MOTION_RESET,MOTION_LOCAL,state.epoch,3,2000,0,0,0)==E_OK);
        admit(&request,clock_ms,0,NULL,0,0,false);
        assert(state.result==MOTION_COMPLETED && !diagnostics.encoder_failure.reason);
        printf("encoder snapshot scenario %u PASS\n",scenario);
    }
    for(scenario=28;scenario<=29;++scenario) {
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(stop_injected && !state.faults && state.result==MOTION_CANCELLED);
        assert(!electrical_left && !electrical_right && !diagnostics.encoder_failure.reason);
        printf("one paused-wheel settling count then manual STOP scenario %u PASS\n",scenario);
    }
    for(scenario=0;scenario<13;++scenario){
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(sent_request && energised && !electrical_left && !electrical_right);
        assert(!owner_flags.active_valid && !state.valid && !state.calibrated);
        assert(state.raw_valid && !diagnostics.line_active && !line_trial.speed.valid);
        if(scenario==0 || scenario==8 || scenario==9 || (scenario==3 && MOTION_LINE_CIRCUIT)){assert(completed_at==LINE_FOLLOW_MAX_MS+40u);assert(state.result==MOTION_COMPLETED);}
        if(scenario==1 || scenario==10)assert(state.faults&MOTION_FAULT_EXTERNAL);
        if(scenario==2 || scenario==7 || scenario==11 || scenario==12)assert(state.faults&MOTION_FAULT_IR);
        if((scenario==3 && !MOTION_LINE_CIRCUIT) || scenario==6)assert(state.result==MOTION_CANCELLED);
        if(scenario==2 || scenario==7 || scenario==11 || scenario==12) {
            assert(diagnostics.line_failure_flags==7);
            assert(diagnostics.line_failure_elapsed_ms>0);
            if(scenario==7)assert(diagnostics.line_failure_age_ms>LINE_FOLLOW_INPUT_AGE_MS);
            else {
                assert(diagnostics.line_failure_age_ms==0);
                assert(diagnostics.line_failure_state==(scenario==12 ? LINE_BOTH_BLACK:LINE_LOST));
            }
        }
        if(scenario==0 || scenario==6)assert(!diagnostics.line_failure_flags);
        if(scenario==4)assert(state.faults&MOTION_FAULT_STALL);
        if(scenario==5)assert(state.faults&MOTION_FAULT_ENCODER);
        printf("integrated motion scenario %u PASS\n",scenario);
    }
    puts("PASS: local-only admission, uncalibrated metric guard, actual owner loop, steering, expiry, no renewal/queue, fault/stop/disconnect and reset");
    return 0;
}

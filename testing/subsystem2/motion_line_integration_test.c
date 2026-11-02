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
static bool sent_request,stop_injected;
static uint32_t stopped_at;
UW in_w(UW reg){uint64_t t=(uint64_t)clock_ms*1000;return reg==TIMER_TIMERAWH?(UW)(t>>32):(UW)t;}
UW tm_usb_state(void){return scenario==3 && clock_ms>=300 ? 2:3;}
static unsigned lock_busy_polls,lock_calls,lock_waits;
static ER lock_wait_result,lock_forced_error;
ER tk_loc_mtx(ID a,int b){
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
void encoder_hw_snapshot(encoder_raw_t *r){*r=fake_raw;}
void motor_hw_stop(void){if(electrical_left||electrical_right)stopped_at=clock_ms;electrical_left=electrical_right=0;}
void motor_hw_write(int16_t l,int16_t r){
    assert(l>=-200 && l<=0 && r>=0 && r<=200);
    if(l||r)++energised;
    electrical_left=l;electrical_right=r;
}
ER tk_wai_flg(ID a,UINT b,UINT c,UINT*d,int e){
    (void)a;(void)b;(void)c;(void)e;
    if(clock_ms>=LINE_FOLLOW_MAX_MS+400u)longjmp(done,1);
    clock_ms+=20;
    if(electrical_left && scenario!=4)fake_raw.left-=24;
    if(electrical_right)fake_raw.right+=24;
    if(scenario==5 && clock_ms>=300)fake_raw.invalid=1;
    line_msg_t line={.state=LINE_ON};
    if(scenario==2 && clock_ms>=300)line.state=LINE_LOST;
    if(scenario==11 && clock_ms>=LINE_FOLLOW_MAX_MS-500u)line.state=LINE_LOST;
    if(scenario==12 && clock_ms>=LINE_FOLLOW_MAX_MS/2u)line.state=LINE_BOTH_BLACK;
    if(scenario==8)line.err=100; /* stopped inner wheel is not a stall */
    if(scenario==9)line.err=-100;
    if(!(scenario==7 && clock_ms>=200)) {
        shared.line=line;shared.have_line=true;shared.line_ms=clock_ms;shared.line_epoch=state.epoch;
    }
    if(clock_ms==40) {
        motion_request_t request;
        assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_LOCAL,state.epoch,1,LINE_FOLLOW_MAX_MS,0,0,0)==E_OK);
        assert(motion_submit(&request)==E_OK);sent_request=true;
    }
    if(clock_ms==100) {
        motion_request_t request;
        assert(motion_request_init(&request,MOTION_LINE_TRACE,MOTION_LOCAL,state.epoch,2,LINE_FOLLOW_MAX_MS,0,0,0)==E_OK);
        assert(motion_submit(&request)==E_OBJ); /* no queued/replayed motion */
        assert(motion_renew(state.epoch,1,MOTION_LOCAL,clock_ms,LINE_FOLLOW_MAX_MS)==E_OBJ);
    }
    if(scenario==1 && clock_ms>=300)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
    if(scenario==10 && clock_ms>=LINE_FOLLOW_MAX_MS/2u)flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
    if(scenario==6 && clock_ms>=300 && !stop_injected){assert(motion_stop()==E_OK);stop_injected=true;}
    *d=flag_bits;return E_OK;
}
static void reset_fixture(void){
    clock_ms=flag_bits=energised=stopped_at=0;electrical_left=electrical_right=0;
    sent_request=stop_injected=false;fake_raw=(encoder_raw_t){0};
    memset(&shared,0,sizeof(shared));memset(&owner_flags,0,sizeof(owner_flags));
    memset(&state,0,sizeof(state));memset(&diagnostics,0,sizeof(diagnostics));
    memset(&active,0,sizeof(active));memset(wheels,0,sizeof(wheels));
    previous_counts=(encoder_raw_t){0};output_rest_until=last_invalid=owner_rejections=0;
    stop_generation=fault_generation=handled_stop_generation=handled_fault_generation=0;
    reset_control();assert(motion_init()==E_OK);assert(motion_application_ready()==E_OK);
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
    admit(&request,0,0,&line,0,1,true);assert(owner_flags.active_valid && state.state==MOTION_TRACING);
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
    for(scenario=0;scenario<13;++scenario){
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(sent_request && energised && !electrical_left && !electrical_right);
        assert(!owner_flags.active_valid && !state.valid && !state.calibrated);
        assert(state.raw_valid && !diagnostics.line_active && !line_trial.speed.valid);
        if(scenario==0 || scenario==8 || scenario==9 || (scenario==3 && MOTION_LINE_CIRCUIT)){assert(stopped_at==LINE_FOLLOW_MAX_MS+40u);assert(state.result==MOTION_COMPLETED);}
        if(scenario==1 || scenario==10)assert(state.faults&MOTION_FAULT_EXTERNAL);
        if(scenario==2 || scenario==7 || scenario==11 || scenario==12)assert(state.faults&MOTION_FAULT_IR);
        if((scenario==3 && !MOTION_LINE_CIRCUIT) || scenario==6)assert(state.result==MOTION_CANCELLED);
        if(scenario==4)assert(state.faults&MOTION_FAULT_STALL);
        if(scenario==5)assert(state.faults&MOTION_FAULT_ENCODER);
        printf("integrated motion scenario %u PASS\n",scenario);
    }
    puts("PASS: local-only admission, uncalibrated metric guard, actual owner loop, steering, expiry, no renewal/queue, fault/stop/disconnect and reset");
}

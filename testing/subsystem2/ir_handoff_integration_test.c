/* Actual motion publisher + IR bridge/runtime with deterministic interleavings. */
#define main motion_owner_cases
#include "motion_line_integration_test.c"
#undef main
#include "../../app_program/subsystem2/motion_ir_bridge.c"
#include "../../app_program/subsystem3/ir_runtime.c"
#undef memset
#undef memcpy
static unsigned acquisition_stops,discards,race_lock_count,race_on_lock;
static bool race_reset;
static ir_acq_status_t acquisition_mock;
ER tk_dly_tsk(unsigned ms){clock_ms+=ms;return E_OK;}
ER ir_acquisition_start(ID event){(void)event;acquisition_mock.running=true;return E_OK;}
void ir_acquisition_stop(void){++acquisition_stops;acquisition_mock.running=false;}
bool ir_acquisition_pop(ir_raw_frame_t *f,ir_acq_status_t *s){(void)f;*s=acquisition_mock;return false;}
void ir_acquisition_discard(void){++discards;}
static void race_at_publish(ID id)
{
    if(id!=mailbox_mutex || ++race_lock_count!=race_on_lock)return;
    lock_hook=NULL;
    /* Publish a coherent new owner epoch at the exact admission lock boundary.
     * Real STOP/reset owner paths are separately exercised in the owner suite. */
    end_request(race_reset ? MOTION_COMPLETED : MOTION_CANCELLED);
    ++state.epoch;
    shared.status=observer_status=state;
    shared.have_line=shared.have_barcode=false;
}
static void frame(uint32_t at)
{
    clock_ms=at;
    ir_raw_frame_t f={.sampled_ms=at,.barcode=200,.line_a=3000,.line_b=200};
    process_frame(&f);
}
static void fixture(void)
{
    lock_hook=NULL;reset_fixture();
    mailbox_mutex=11;status_mutex=12;runtime_mutex=13;runtime_event=14;
    state.epoch=7;state.ready=state.raw_valid=1;
    shared.status=observer_status=state;
    memset(&owner,0,sizeof(owner));memset(&pending,0,sizeof(pending));
    memset(&runtime_flags,0,sizeof(runtime_flags));
    owner.state=IR_RUNTIME_READY;owner.healthy=true;owner.epoch=7;
    owner.acquisition.running=true;
    for(unsigned i=0;i<IR_NUM_CH;++i)owner.calibration[i]=(ir_cal_t){.white=200,.black=3000};
    runtime_flags.epoch_known=true;epoch_floor_ms=90;
    acquisition_mock=(ir_acq_status_t){.running=true};
    acquisition_stops=discards=race_lock_count=0;
    motion_ir_bridge_init(&bridge);frame(100);
    assert(owner.state==IR_RUNTIME_READY && shared.have_line);
    for(uint32_t t=101;t<110;++t)frame(t);
}
static void finishing_barcode(void)
{
    bridge.capture.active=true;bridge.capture.is_bar=false;
    bridge.capture.count=BC_MIN_ELEMS;bridge.capture.run=BC_QUIET_MS-1;
    for(unsigned i=0;i<BC_MIN_ELEMS;++i)bridge.capture.widths[i]=1;
}
static void epoch_race(bool barcode,bool wrap)
{
    fixture();
    if(wrap) {
        state.epoch=owner.epoch=bridge.epoch=shared.line_epoch=UINT32_MAX;
        shared.status=observer_status=state;
    }
    if(barcode)finishing_barcode();
    else {bridge.capture.active=true;bridge.capture.count=3;bridge.capture.run=4;}
    race_on_lock=barcode ? 2:1;lock_hook=race_at_publish;
    frame(110);
    assert(owner.state==IR_RUNTIME_READY && !acquisition_stops);
    assert(owner.epoch_resyncs==1 && !owner.handoff_errors);
    assert(!(flag_bits&SOURCE_BIT(MOTION_HEALTH_IR)));
    assert(!shared.have_line && !shared.have_barcode);
    assert(!runtime_flags.epoch_known && !bridge.primed && !bridge.capture.active);
    frame(111); /* observe new epoch, establish fence, discard queued frames */
    assert(owner.epoch==state.epoch && epoch_floor_ms==111 && discards==2);
    assert(!shared.have_line && !shared.have_barcode);
    clock_ms=112;
    ir_raw_frame_t old={.sampled_ms=110,.barcode=3000,.line_a=3000,.line_b=200};
    process_frame(&old);old.sampled_ms=111;process_frame(&old);
    assert(!shared.have_line && !bridge.primed); /* no old sample re-stamping */
    frame(112);
    assert(shared.have_line && shared.line_epoch==state.epoch && shared.line_ms==112);
    assert(!shared.line.fault && !shared.have_barcode && !bridge.capture.active);
    assert(owner.state==IR_RUNTIME_READY && owner.healthy && !acquisition_stops);
    assert(!energised && !electrical_left && !electrical_right);
}
static void assert_real_fault(unsigned stage)
{
    assert(owner.state==IR_RUNTIME_FAULT && acquisition_stops);
    assert(flag_bits&SOURCE_BIT(MOTION_HEALTH_IR));
    assert(!owner.epoch_resyncs);
    if(stage)assert(owner.handoff_errors==1 && owner.last_handoff_stage==stage);
}
static void fail_publish_lock(ID id)
{
    if(id==mailbox_mutex){lock_hook=NULL;lock_forced_error=E_TMOUT;}
}
static void failure_guards(void)
{
    fixture();shared.line_ms=110;frame(110); /* duplicate within same epoch */
    assert_real_fault(2);assert(owner.last_handoff_error==E_OBJ);
    fixture();shared.line_ms=111;frame(110); /* out of order in same epoch */
    assert_real_fault(2);
    fixture();finishing_barcode();shared.have_barcode=true;frame(110);
    assert_real_fault(2);assert(owner.last_handoff_error==E_OBJ);
    fixture();lock_hook=fail_publish_lock;frame(110);lock_forced_error=0;
    assert_real_fault(2);assert(owner.last_handoff_error==E_TMOUT);
    fixture();lock_forced_error=E_OBJ;frame(110);lock_forced_error=0;
    assert_real_fault(1);
    fixture();frame(111); /* sample gap is not an epoch change */
    assert_real_fault(2);
    fixture();race_on_lock=1;lock_hook=race_at_publish;frame(111);
    assert_real_fault(2); /* even a concurrent epoch change cannot hide a gap */
    fixture();clock_ms=200;
    ir_raw_frame_t stale={.sampled_ms=110};process_frame(&stale);
    assert_real_fault(0);assert(owner.acquisition.first_fault_reason==IR_REASON_QUEUE_AGE);
    fixture();const uint8_t invalid[3]={0,101,0};
    race_on_lock=1;lock_hook=race_at_publish;
    assert(motion_ir_bridge_sample(&bridge,invalid,true,110,owner.epoch)==E_IO);
    assert(flag_bits&SOURCE_BIT(MOTION_HEALTH_IR));
    fixture();owner.healthy=false;runtime_flags.health_asserted=true;
    flag_bits|=SOURCE_BIT(MOTION_HEALTH_IR);
    race_on_lock=1;lock_hook=race_at_publish;frame(110);
    assert(flag_bits&SOURCE_BIT(MOTION_HEALTH_IR)); /* resync never clears fault */
    assert(!owner.healthy && owner.epoch_resyncs==1);
}
static void publication_results(void)
{
    fixture();
    /* Actual bridge transports the calibrated pair in the same critical
     * section as error/time/epoch. Rejected updates cannot change that pair. */
    const uint8_t normalized[3]={50,80,10};
    assert(motion_ir_bridge_sample(&bridge,normalized,true,110,owner.epoch)==E_OK);
    assert(shared.line_levels==line_levels_pack(normalized[IR_CH_LINE_A],normalized[IR_CH_LINE_B]));
    uint16_t previous=shared.line_levels;
    line_msg_t next={.state=LINE_ON,.t_ms=120};
    assert(motion_publish_line_observed(&next,line_levels_pack(5,95),120,6)==MOTION_IR_SUPERSEDED);
    assert(shared.line_levels==previous && shared.line_ms==110);
    assert(motion_publish_line_observed(&next,0xffff,120,7)==E_PAR);
    assert(shared.line_levels==previous && shared.line_ms==110);
    assert(motion_publish_line(&next,120,7)==E_OK && shared.line_levels==0);
    fixture();
    line_msg_t line={.state=LINE_ON,.t_ms=110};bc_msg_t bc={0};
    assert(motion_publish_line(&line,110,6)==MOTION_IR_SUPERSEDED);
    assert(motion_publish_barcode(&bc,110,6)==MOTION_IR_SUPERSEDED);
    assert(shared.line_ms==100 && !shared.have_barcode);
    assert(motion_publish_line(&line,111,6)==E_PAR); /* malformed, not benign */
    assert(motion_publish_line(NULL,110,6)==E_PAR);
    assert(motion_publish_barcode(NULL,110,6)==E_PAR);
    assert(motion_publish_line(&line,110,7)==E_OK);
    assert(motion_publish_line(&line,110,7)==E_OBJ);
    assert(motion_publish_barcode(&bc,110,7)==E_OK);
    assert(motion_publish_barcode(&bc,111,7)==E_OBJ);
}
int main(void)
{
    publication_results();
    for(unsigned i=0;i<16;++i) {
        race_reset=(i&1u)!=0;
        epoch_race((i&2u)!=0,(i&4u)!=0);
    }
    failure_guards();
    puts("PASS: STOP/reset and barcode publication races, epoch wrap, fresh-data fence, no replay/motor start, real faults retained");
    return 0;
}

#define main legacy_owner_tests
#include "motion_line_integration_test.c"
#undef main
int main(void)
{
    for(scenario=50;scenario<=88;++scenario) {
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(sent_request && energised && !electrical_left && !electrical_right && !owner_flags.active_valid);
        assert(first_electrical_left>=-85 && first_electrical_right<=85);
        if(scenario==50 || scenario==51)assert(stop_injected && !state.faults && state.result==MOTION_CANCELLED && stopped_at==2200);
        if(scenario==52)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_CROSSING_EXHAUSTED && diagnostics.line_stop_reason==LINE_STOP_MARK_DISTANCE);
        if(scenario==53)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_RECOVERY_EXHAUSTED);
        if(scenario==54 || scenario==57 || scenario==58)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_IR);
        if(scenario==55)assert(state.faults==MOTION_FAULT_EXTERNAL);
        if(scenario==56)assert(state.faults==MOTION_FAULT_ENCODER);
        if(scenario==59 || scenario==60 || scenario==65 || scenario==66 || scenario==67 || scenario==70 || scenario==71)
            assert(stop_injected && !state.faults && state.result==MOTION_CANCELLED);
        if(scenario==60)assert(stopped_at==320);
        if(scenario==61)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_IR);
        if(scenario==62)assert(state.faults==MOTION_FAULT_EXTERNAL);
        if(scenario==63)assert(state.faults==MOTION_FAULT_ENCODER);
        if(scenario==64 || scenario==68 || scenario==69)assert(!state.faults && state.result==MOTION_CANCELLED &&
            diagnostics.line_step==LINE_STEP_ROUTE_REQUIRED && diagnostics.line_stop_reason==LINE_STOP_ROUTE_COMMAND && stopped_at==300);
        if(scenario==65 || scenario==70)assert(state.barcode==3);
        if(scenario==66 || scenario==67 || scenario==70 || scenario==71)assert(state.rejected_barcodes);
        if(scenario>=72 && scenario<=76) {
            /* These are the observer's frozen fields after reset_control and
             * many idle cycles, not values read directly from a live trial. */
            motion_diagnostics_t frozen;assert(motion_get_diagnostics(&frozen)==E_OK);
            motion_encoder_failure_t *e=&frozen.encoder_failure;
            bool right=scenario==73 || scenario==75 || scenario==76;
            assert(state.faults==MOTION_FAULT_ENCODER && diagnostics.line_step==LINE_STEP_ENCODER);
            assert(e->reason==(right ? LINE_ENCODER_RIGHT_RECEDING:LINE_ENCODER_LEFT_RECEDING));
            assert(e->pause_budget==2 && e->raw_valid && !e->speed_reason);
            assert((right ? e->delta_right:e->delta_left)==(scenario==74 || scenario==75 ? -2:-1));
            assert(e->pause_eligible==(scenario==73 || scenario==75 ? 2:1));
            if(scenario==76) {
                assert(e->prior_target_right>0 && e->prior_duty_right>0);
                assert(!e->pause_used_right && e->pause_age_right_ms==UINT32_MAX && e->settle_age_right_ms==UINT32_MAX);
            } else {
                assert((right ? e->prior_target_right:e->prior_target_left)==0);
                assert((right ? e->prior_duty_right:e->prior_duty_left)==0);
                assert((right ? e->pause_used_right:e->pause_used_left)==0);
                assert((right ? e->settle_age_right_ms:e->settle_age_left_ms)==UINT32_MAX);
                assert((right ? e->pause_age_right_ms:e->pause_age_left_ms)>20);
            }
            assert(stopped_at==500u);
            assert(e->pause_window_limit_ms==120);
        }
        if(scenario==77 || scenario==78 || scenario==87)
            assert(stop_injected && !state.faults && state.result==MOTION_CANCELLED);
        if(scenario==88)assert(state.faults==MOTION_FAULT_EXTERNAL);
        if(scenario>=79 && scenario<=86) {
            motion_diagnostics_t frozen;assert(motion_get_diagnostics(&frozen)==E_OK);
            motion_encoder_failure_t *e=&frozen.encoder_failure;
            bool right=scenario%2==0;
            assert(state.faults==MOTION_FAULT_ENCODER && diagnostics.line_step==LINE_STEP_ENCODER);
            assert(e->pause_budget==2 && e->pause_window_limit_ms==120);
            if(scenario==85)assert(e->reason==LINE_ENCODER_INVALID_EDGES);
            else if(scenario==86)assert(e->reason==LINE_ENCODER_LEFT_RECEDING && e->prior_duty_left>0);
            else {
                assert(e->reason==(right ? LINE_ENCODER_RIGHT_RECEDING:LINE_ENCODER_LEFT_RECEDING));
                assert((right ? e->pause_used_right:e->pause_used_left)==(scenario<=80 ? 2:0));
                assert((right ? e->pause_window_right_ms:e->pause_window_left_ms)==
                    (scenario<=80 ? 80u:scenario<=82 ? 40u:120u));
                assert((right ? e->delta_right:e->delta_left)==(scenario==81 || scenario==82 ? -3:-1));
            }
        }
        bool crossed=false,reacquired=false;
        bool observed=false;
        for(unsigned i=0;i<line_history.count;++i) {
            motion_line_trace_entry_t entry;assert(motion_get_line_trace(i,&entry)==E_OK);
            if(line_levels_valid(entry.sample.levels))observed=true;
            if(entry.sample.adaptive_mode==LINE_ADAPT_CROSSING)crossed=true;
            if(crossed && entry.sample.adaptive_mode==LINE_ADAPT_TRACK)reacquired=true;
        }
        assert(observed);
        if(scenario>=59 && scenario<=63)assert(crossed);
        if(scenario==59)assert(reacquired);
        printf("PASS adaptive owner scenario %u: step %u, zero final motor outputs\n",scenario,diagnostics.line_step);
    }
}

#define main legacy_owner_tests
#include "motion_line_integration_test.c"
#undef main
int main(void)
{
    for(scenario=24;scenario<=29;++scenario) {
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(stop_injected && !electrical_left && !electrical_right);
        assert(first_electrical_left>=-85 && first_electrical_left<0 && first_electrical_right>0 && first_electrical_right<=85);
        if(scenario>=28)assert(!state.faults && state.result==MOTION_CANCELLED);
        else {
            assert(state.faults==MOTION_FAULT_ENCODER);
            unsigned reason=scenario==24 ? LINE_ENCODER_LEFT_RECEDING:
                            scenario==25 ? LINE_ENCODER_RIGHT_RECEDING:
                            scenario==26 ? LINE_ENCODER_INVALID_EDGES:LINE_ENCODER_SPEED;
            assert(diagnostics.encoder_failure.reason==reason);
            assert(diagnostics.line_failure_elapsed_ms==(scenario<=25 ? 180u:160u));
        }
        printf("PASS: search-profile paused encoder owner scenario %u\n",scenario);
    }
    for(scenario=30;scenario<=38;++scenario) {
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(sent_request && energised && !electrical_left && !electrical_right && !owner_flags.active_valid);
        assert(first_electrical_left>=-85 && first_electrical_left<0 && first_electrical_right>0 && first_electrical_right<=85);
        if(scenario==30) {
            /* Fake wheel speed can hit the travel cap before timeout. */
            assert(state.faults==MOTION_FAULT_RANGE || state.faults==MOTION_FAULT_IR);
            assert(diagnostics.line_step==LINE_STEP_SEARCH_RANGE || diagnostics.line_step==LINE_STEP_SEARCH_TIMEOUT);
        }
        if(scenario==31 || scenario==32 || scenario==36)assert(!state.faults && state.result==MOTION_CANCELLED);
        if(scenario==33 || scenario==35)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_IR);
        if(scenario==34)assert(state.faults==MOTION_FAULT_ENCODER);
        if(scenario==37)assert(state.faults==MOTION_FAULT_EXTERNAL);
        if(scenario==38)assert(!state.faults && state.result==MOTION_COMPLETED && completed_at==LINE_FOLLOW_MAX_MS+40u);
        bool sweep=false,reacquired=false,second=false;
        for(unsigned i=0;i<line_history.count;++i) {
            motion_line_trace_entry_t entry;assert(motion_get_line_trace(i,&entry)==E_OK);
            if(entry.sample.search_phase){if(reacquired)second=true;sweep=true;}
            else if(sweep)reacquired=true;
        }
        if(scenario==30 || scenario==31 || scenario==32)assert(sweep);
        if(scenario>=33 && scenario<=37)assert(!sweep); /* early guards/STOP during grace */
        if(scenario==38)assert(sweep && reacquired);
        if(scenario==31 || scenario==32)assert(reacquired);
        if(scenario==32)assert(second);
        printf("PASS: actual search owner scenario %u, final motor outputs zero\n",scenario);
    }
    return 0;
}

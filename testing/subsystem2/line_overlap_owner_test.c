#define main legacy_owner_tests
#include "motion_line_integration_test.c"
#undef main
int main(void)
{
    for(scenario=39;scenario<=45;++scenario) {
        reset_fixture();
        if(!setjmp(done))motion_task(0,NULL);
        assert(sent_request && energised && !electrical_left && !electrical_right && !owner_flags.active_valid);
        assert(first_electrical_left>=-85 && first_electrical_left<0 && first_electrical_right>0 && first_electrical_right<=85);
        if(scenario==39 || scenario==42)assert(stop_injected && !state.faults && state.result==MOTION_CANCELLED);
        if(scenario==40)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_OVERLAP_TIMEOUT && stopped_at==420);
        if(scenario==41)assert(state.faults==MOTION_FAULT_RANGE && diagnostics.line_step==LINE_STEP_OVERLAP_RANGE && stopped_at==320);
        if(scenario==42)assert(stopped_at==320);
        if(scenario==43 || scenario==45)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_IR);
        if(scenario==44)assert(state.faults==MOTION_FAULT_EXTERNAL);
        bool held=false,cleared=false;
        for(unsigned i=0;i<line_history.count;++i) {
            motion_line_trace_entry_t e;assert(motion_get_line_trace(i,&e)==E_OK);
            if(e.sample.state==LINE_BOTH_BLACK && e.sample.step==LINE_STEP_OK) {
                held=true;assert(e.sample.duty_left==0 && e.sample.tight_direction==1);
            } else if(held && e.sample.state!=LINE_BOTH_BLACK)cleared=true;
        }
        assert(held);
        if(scenario==39)assert(cleared); /* entry/exit recorded despite <100ms gap */
        printf("PASS: overlap owner scenario %u, final electrical outputs zero\n",scenario);
    }
}

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

        if(scenario==59 || scenario==60 || scenario==65 || scenario==66 || scenario==67 || scenario==70 || scenario==71)
            assert(stop_injected && !state.faults && state.result==MOTION_CANCELLED);
        if(scenario==60)assert(stopped_at==320);
        if(scenario==61)assert(state.faults==MOTION_FAULT_IR && diagnostics.line_step==LINE_STEP_IR);
        if(scenario==62)assert(state.faults==MOTION_FAULT_EXTERNAL);

        if(scenario==64 || scenario==68 || scenario==69)assert(!state.faults && state.result==MOTION_CANCELLED &&
            diagnostics.line_step==LINE_STEP_ROUTE_REQUIRED && diagnostics.line_stop_reason==LINE_STOP_ROUTE_COMMAND && stopped_at==300);
        if(scenario==65 || scenario==70)assert(state.barcode==3);
        if(scenario==66 || scenario==67 || scenario==70 || scenario==71)assert(state.rejected_barcodes);
        if((scenario>=72 && scenario<=84) || scenario==86 || scenario==87)
            assert(stop_injected && !state.faults && state.result==MOTION_CANCELLED);
        if(scenario==56 || scenario==63 || scenario==85) {
            assert(state.invalid_edges>0 && !state.faults);
            assert(stop_injected && state.result==MOTION_CANCELLED);
        }
        if(scenario==88)assert(state.faults==MOTION_FAULT_EXTERNAL);
        bool candidate=false,crossed=false,reacquired=false;
        bool observed=false;
        for(unsigned i=0;i<line_history.count;++i) {
            motion_line_trace_entry_t entry;assert(motion_get_line_trace(i,&entry)==E_OK);
            if(line_levels_valid(entry.sample.levels))observed=true;
            if(entry.sample.adaptive_mode==LINE_ADAPT_JUNCTION)candidate=true;
            if(entry.sample.adaptive_mode==LINE_ADAPT_CROSSING)crossed=true;
            if(crossed && entry.sample.adaptive_mode==LINE_ADAPT_TRACK)reacquired=true;
        }
        assert(observed);
        if(scenario>=60 && scenario<=62)assert(candidate && !crossed);
        if(scenario==63)assert(candidate && crossed);
        if(scenario==59)assert(crossed && reacquired);
        printf("PASS adaptive owner scenario %u: step %u, zero final motor outputs\n",scenario,diagnostics.line_step);
    }
}

/* Reuse the actual-console host fixture, with a separate autostart entry. */
#define main manual_profile_main
#include "line_console_test.c"
#undef main
static void fresh_at(uint32_t now) {
    test_clock=now;sensor.updated_ms=sensor.raw.sampled_ms=now;
    motion_line_console_key(-1);
}
static void reset_auto(void) {
    autostart=(line_auto_t){0};auto_reset_id=auto_reset_epoch=0;
    submitted=stops=0;fake_state=MOTION_ERROR;fake_faults=MOTION_FAULT_EXTERNAL;
    fake_epoch=1;fake_id=0;fake_result=MOTION_NONE;pin_pressed=false;
    sensor=(ir_runtime_status_t){.state=IR_RUNTIME_READY,.healthy=true};
}
static void acknowledge_reset(void) {
    fake_epoch=2;fake_id=1;fake_state=MOTION_IDLE;
    fake_faults=0;fake_result=MOTION_COMPLETED;
}
int main(void) {
    reset_auto();fresh_at(3000);fresh_at(3999);assert(!submitted);
    fresh_at(4000);assert(submitted==1 && last_command==MOTION_RESET);
    acknowledge_reset();fresh_at(4999);assert(submitted==1);
    fresh_at(5000);assert(submitted==2 && last_command==MOTION_LINE_TRACE);
    assert(autostart.phase==LINE_AUTO_CONSUMED);
    fresh_at(35000);fresh_at(100000);assert(submitted==2); /* no repeat */
    pin_pressed=true;fresh_at(100100);assert(stops==1);
    reset_auto();fresh_at(3500);motion_line_console_key('x');fresh_at(5000);assert(!submitted);
    reset_auto();fresh_at(4000);acknowledge_reset();sensor.healthy=false;
    fresh_at(5000);assert(submitted==1 && autostart.phase==LINE_AUTO_CANCELLED);
    sensor.healthy=true;fresh_at(5100);assert(submitted==1);
    reset_auto();fresh_at(4000); /* RESET not completed */
    fresh_at(5000);assert(submitted==1 && autostart.phase==LINE_AUTO_CANCELLED);
    reset_auto();fake_faults=MOTION_FAULT_ENCODER;fresh_at(4000);assert(!submitted);
    reset_auto();fresh_at(6000);assert(!submitted); /* no late startup */
    reset_auto();pin_pressed=true;fresh_at(3000);pin_pressed=false;fresh_at(5000);assert(!submitted);
    reset_auto();fresh_at(4000);acknowledge_reset();fresh_at(5600);
    assert(submitted==1 && autostart.phase==LINE_AUTO_CANCELLED);
    reset_auto();fresh_at(3000);motion_line_console_key('c');fresh_at(5000);assert(!submitted);
    reset_auto();fresh_at(3000);motion_line_console_key('g');fresh_at(5000);assert(!submitted);
    reset_auto();fresh_at(4000);acknowledge_reset();fake_epoch=3;
    fresh_at(5000);assert(submitted==1 && autostart.phase==LINE_AUTO_CANCELLED);
    puts("PASS: 5s one-shot boot start, actual reset acknowledgement, no early/late/repeated run, fault/STOP/calibration/epoch cancellation, GP20 stop only");
}

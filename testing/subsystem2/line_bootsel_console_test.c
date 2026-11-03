/* Actual console path; ROM entry is a counted stub, never host hardware. */
#define main original_console_main
#include "line_console_test.c"
#undef main
static void setup(void) {
    reset_button_fixture();
    boot_pending=false;bootsel_calls=0;fake_age=0;fake_snapshot_error=0;
    fake_left_duty=fake_right_duty=0;
    line_button_arm(&button,test_clock,false);
    fake_state=MOTION_IDLE;fake_faults=0;
}
static void stop_ack(void) {
    ++fake_epoch;fake_result=MOTION_CANCELLED;fake_state=MOTION_IDLE;
}
int main(void) {
    setup();motion_line_console_key('b');
    assert(stops==1 && boot_pending && !button.armed && !button_boot_pending);
    motion_line_console_key('r');assert(!submitted && !bootsel_calls);
    stop_ack();poll_button(true);assert(bootsel_calls==1 && !boot_pending);
    press_release();assert(!submitted); /* ROM failure cannot rearm */

    setup();motion_line_console_key('b');stop_ack();fake_left_duty=-100;
    poll_button(false);assert(!bootsel_calls);
    fake_left_duty=0;fake_right_duty=100;
    poll_button(false);assert(!bootsel_calls);
    fake_right_duty=0;poll_button(false);assert(bootsel_calls==1);

    setup();motion_line_console_key('b');poll_button(false);assert(!bootsel_calls);
    stop_ack();fake_age=101;poll_button(false);assert(!bootsel_calls);
    fake_age=0;fake_state=MOTION_TRACING;poll_button(false);assert(!bootsel_calls);
    fake_state=MOTION_ERROR;poll_button(false);assert(bootsel_calls==1);

    setup();motion_line_console_key('b');test_clock+=1000;stop_ack();
    poll_button(false);assert(!bootsel_calls && !boot_pending);
    setup();motion_line_console_key('b');motion_line_console_key('x');
    stop_ack();poll_button(false);assert(!bootsel_calls && !boot_pending);
    setup();fake_snapshot_error=E_TMOUT;motion_line_console_key('b');
    assert(stops==1 && !boot_pending && !bootsel_calls && !button.armed);
    setup();motion_line_console_key('b');stop_ack();fake_epoch++;
    fake_state=MOTION_ERROR;fake_result=MOTION_COMPLETED;
    poll_button(false);assert(bootsel_calls==1); /* idle STOP retains result; fault may also advance epoch */
    setup();fake_result=MOTION_COMPLETED;motion_line_console_key('b');
    ++fake_epoch;poll_button(false);assert(bootsel_calls==1);
    setup();fake_result=MOTION_NONE;motion_line_console_key('b');
    ++fake_epoch;poll_button(false);assert(bootsel_calls==1);
    puts("PASS: BOOTSEL requires fresh post-STOP epoch, zero outputs, inactive state; idle STOP retains result, concurrent fault advances epoch, disarming/timeout/snapshot/ROM failure/cancellation");
    return 0;
}

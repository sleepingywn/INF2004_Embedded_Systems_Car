#define main existing_console_main
#include "line_console_test.c"
#undef main
static unsigned led_level,led_writes;
void cyw43_utk_set_led(uint32_t on){led_level=on;++led_writes;}
int main(void)
{
    reset_button_fixture();poll_button(false);button_ack_reset();poll_button(false);
    assert(indicator.mode==LINE_LED_ARMED);
    press_release();assert(last_command==MOTION_LINE_TRACE && indicator.mode==LINE_LED_PENDING);
    assert(indicator.id==fake_id+1u && indicator.epoch==fake_epoch);
    ++fake_id;fake_state=MOTION_TRACING;fake_result=MOTION_ACCEPTED;
    poll_button(false);assert(indicator.mode==LINE_LED_RUNNING && led_level==1 && led_writes);
    fake_state=MOTION_IDLE;fake_result=MOTION_COMPLETED;
    poll_button(false);assert(indicator.mode==LINE_LED_OFF && led_level==0);
    reset_button_fixture();poll_button(false);button_ack_reset();poll_button(false);press_release();
    ++fake_id;fake_result=MOTION_REJECTED;poll_button(false);
    assert(indicator.mode==LINE_LED_ERROR);
    motion_line_console_key('x');assert(indicator.mode==LINE_LED_OFF && led_level==0);
    reset_button_fixture();poll_button(false);button_ack_reset();poll_button(false);
    sensor.healthy=false;press_release();assert(indicator.mode==LINE_LED_ERROR);
    puts("PASS: actual GP20 console publishes queued/running/rejected/completed/STOP LED levels without changing motion commands");
    return 0;
}

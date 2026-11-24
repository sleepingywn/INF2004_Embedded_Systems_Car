/* Buddy 3 sensor outputs and the Buddy 2 time-width decoder boundary. */
#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem3/sensor_line.h"
#include "../../app_program/subsystem3/barcode_level.h"
#include "../../app_program/subsystem3/barcode_sense.h"
#include "../../app_program/subsystem2/motion_route.h"

static void line_boundaries(void)
{
    ir_cal_acc_t acc;
    ir_cal_t cal[IR_NUM_CH];
    uint16_t low[IR_NUM_CH] = {200, 200, 200};
    uint16_t high[IR_NUM_CH] = {800, 800, 799};
    ir_cal_begin(&acc);
    ir_cal_add(&acc, low);
    ir_cal_add(&acc, high);
    assert(ir_cal_finish(&acc, cal) == (1u << IR_CH_LINE_B));
    assert(ir_normalise(&cal[IR_CH_LINE_A], cal[IR_CH_LINE_A].black) == 100);
    assert(ir_normalise(&cal[IR_CH_LINE_A], 500) == 50);
    assert(ir_normalise(&cal[IR_CH_LINE_B], 799) == 0);

    line_tracker_t tracker;
    line_state_t state;
    line_init(&tracker);
    assert(line_update(&tracker, 75, 15, 10, &state) == 60 && state == LINE_EDGE_ONLY);
    assert(line_update(&tracker, 15, 75, 10, &state) == -60 && state == LINE_EDGE_ONLY);
    assert(line_update(&tracker, 75, 75, 10, &state) == 0 && state == LINE_BOTH_BLACK);
    assert(line_update(&tracker, 15, 15, LINE_UNSEEN_MS - 1, &state) == 0 && state == LINE_ON);
    assert(line_update(&tracker, 15, 15, 1, &state) == 0 && state == LINE_LOST);
    assert(line_update(&tracker, 70, 20, 10, &state) == 50 && state == LINE_ON);
    assert(line_update(&tracker, 15, 15, 10, &state) == 0 && state == LINE_ON);
}

/* Two consecutive captures. Both contain 29 widths; only one encodes *A*. */
static void barcode_handoff(void)
{
    const char pattern[] = "nwnnwnwnn" "n" "wnnnnwnnw" "n" "nwnnwnwnn";
    bc_capture_t capture;
    bc_level_t level;
    bc_msg_t msg;
    bc_level_init(&level);
    bc_init(&capture);
    for (unsigned scan = 0; scan < 2; scan++) {
        unsigned expected[29];
        for (unsigned i = 0; i < 29; i++) {
            bool wide = scan == 0 && pattern[i] == 'w';
            expected[i] = wide ? 90 : 30;
            unsigned raw = i % 2 ? 177 : (wide ? 1309 : 307);
            for (unsigned k = 0; k < expected[i]; k++) {
                bool black = bc_level_black(&level, capture.is_bar, raw);
                assert(!bc_feed(&capture, black ? 100 : 0, &msg));
            }
        }
        bool black = bc_level_black(&level, capture.is_bar, 177);
        assert(!black && bc_feed(&capture, 0, &msg));
        assert(!bc_active(&capture) && msg.hdr.seq == scan);
        assert(msg.hdr.count == 29 && !msg.hdr.overflow);
        for (unsigned i = 0; i < 29; i++) assert(msg.widths[i] == expected[i]);
        assert(route_code39_decode(&msg) == (scan == 0 ? 'A' : 0));
    }
}

int main(void)
{
    line_boundaries();
    barcode_handoff();
    puts("PASS Buddy 3 calibration, line loss/reacquisition and analogue barcode handoff");
}

/* End-to-end synthetic stripe capture -> production Code39 decoder. */
#include <assert.h>
#include <stdio.h>
#include "barcode_sense.h"
#include "motion_logic.h"

static void scan(unsigned mask, char expected, int reverse, unsigned scale)
{
    bc_width_t widths[29];
    const unsigned patterns[3] = {0x094u, mask, 0x094u};
    bc_capture_t capture;
    bc_msg_t message;
    unsigned emitted = 0;
    for (unsigned c = 0; c < 3; ++c) {
        for (unsigned i = 0; i < 9; ++i)
            widths[c * 10 + i] = (patterns[c] & (1u << (8 - i))) ? 30 * scale : 10 * scale;
        if (c < 2) widths[c * 10 + 9] = 10 * scale;
    }
    bc_init(&capture);
    for (unsigned i = 0; i < 29; ++i) {
        unsigned duration = widths[reverse ? 28 - i : i];
        for (unsigned t = 0; t < duration; ++t)
            emitted += bc_feed(&capture, (i % 2 == 0) ? 127 : 0, &message);
    }
    emitted += bc_feed(&capture, 0, &message);
    assert(emitted == 1);
    assert(motion_code39_decode(&message) == expected);
    assert(motion_barcode_decode(&message) == (expected == 'Z' ? 0 : expected));
    message.hdr.overflow = 1;
    assert(motion_code39_decode(&message) == 0);
    assert(motion_barcode_decode(&message) == 0);
}
int main(void)
{
    /* Boundary hysteresis: grey does not start a bar, or split an active bar. */
    bc_capture_t capture;
    bc_msg_t message;
    bc_init(&capture);
    assert(!bc_feed(&capture, BC_BAR_ON - 1u, &message));
    assert(!bc_active(&capture));
    assert(!bc_feed(&capture, BC_BAR_ON, &message));
    assert(bc_active(&capture) && capture.is_bar);
    assert(!bc_feed(&capture, BC_BAR_OFF + 1u, &message));
    assert(capture.is_bar);
    assert(!bc_feed(&capture, BC_BAR_OFF, &message));
    assert(!capture.is_bar && capture.count == 1u);
    /* Quiet timeout must end exactly at the configured number of samples. */
    bc_init(&capture);
    assert(!bc_feed(&capture, 100, &message));
    for (unsigned t = 1; t < BC_QUIET_MS; ++t) {
        assert(!bc_feed(&capture, 0, &message));
        assert(bc_active(&capture));
    }
    assert(!bc_feed(&capture, 0, &message)); /* one-bar noise is discarded */
    assert(!bc_active(&capture));
    const unsigned masks[] = {0x109, 0x049, 0x148, 0x019, 0x0D0, 0};
    const char expected[] = {'A', 'B', 'C', 'D', 'Z', 0};
    for (unsigned i = 0; i < 6; ++i)
        for (int reverse = 0; reverse < 2; ++reverse)
            {
            scan(masks[i], expected[i], reverse, 1);
#ifdef BARCODE_SLOW_SCAN
            scan(masks[i], expected[i], reverse, 20);
#endif
        }
#ifdef BARCODE_SLOW_SCAN
    /* Actual user's reversed *A* scan: strict ratios reject it; rank-only
     * recognises the start/data/stop arrangement in either direction. */
    const bc_width_t recorded[29]={71,145,328,123,331,115,112,371,129,136,
        335,130,131,328,160,87,151,86,394,84,148,94,395,96,394,90,157,311,220};
    for(unsigned reverse=0;reverse<2;++reverse) {
        message=(bc_msg_t){0};message.hdr.count=29;
        for(unsigned i=0;i<29;++i) message.widths[i]=recorded[reverse ? 28-i:i];
#ifdef BARCODE_RELAXED_WIDTHS
        assert(motion_code39_decode(&message)=='A');
#else
        assert(motion_code39_decode(&message)==0);
#endif
        /* No start marker, missing elements, and tied widths remain invalid. */
        for(unsigned i=0;i<9;++i) message.widths[i]=100;
        assert(motion_code39_decode(&message)==0);
        message.hdr.count=27;assert(motion_code39_decode(&message)==0);
    }
#endif
    puts("PASS: capture/decode A-D, Z and invalid stripes, both directions; Z excluded from routes; overflow rejected");
    return 0;
}

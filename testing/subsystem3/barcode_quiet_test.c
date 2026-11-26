/* barcode_quiet_test.c - laptop check of the end-of-capture (quiet) time in
 * the route profile.
 *   gcc -std=c11 -Wall -Wextra -DBARCODE_SLOW_SCAN -DBARCODE_WIDE \
 *       -I../../app_program/subsystem3 barcode_quiet_test.c ../../app_program/subsystem2/motion_route_decode.c \
 *       ../../app_program/subsystem3/barcode_sense.c -o barcode_quiet_test
 *
 * Car 25 Nov 2026: a bigger printed barcode driven at V5 speed had wide white
 * gaps up to 1483 ms; the fixed 1.5 s quiet time ended both captures early
 * (21 and 19 of 29 elements).  The quiet time now also scales with the
 * barcode's widest element (4x, at most 5 s).
 */
#include <assert.h>
#include <stdio.h>
#include "barcode_sense.h"
#include "../../app_program/subsystem2/motion_route_decode.h"

/* Real Z scan (ms widths, KR4 car log 18 Nov), as in barcode_level_test.c */
static const unsigned scan_z[29] = {66,117,368,113,255,117,104,298,88,125,83,126,95,103,273,
                                    128,270,325,82,138,90,195,320,46,305,111,74,344,76};

/* Feed one element of n samples (1 ms each); returns true if a capture ended. */
static bool feed(bc_capture_t *c, bool bar, unsigned n, bc_msg_t *out)
{
    bool done = false;
    for (unsigned i = 0; i < n && !done; i++) done = bc_feed(c, bar ? 100u : 0u, out);
    return done;
}

/* Barcode at `scale` x the 18 Nov element widths, then white. */
static char run(unsigned scale, unsigned *count)
{
    bc_capture_t c; bc_msg_t m = {0};
    bc_init(&c);
    feed(&c, false, 500, &m);
    bool done = false;
    for (int e = 0; e < 29 && !done; e++) done = feed(&c, e % 2 == 0, scan_z[e] * scale, &m);
    if (!done) done = feed(&c, false, 6000, &m);
    assert(done);
    *count = m.hdr.count;
    return route_code39_decode(&m);
}

static void circle_exit_before_d(void)
{
    /* Observed false prefix, followed by the complete clean D widths from
       the same run (the failed trace ended two elements early). */
    const unsigned d[29] = {140,593,115,220,392,217,432,190,93,195,
                            132,270,93,190,426,521,103,199,407,193,
                            93,590,93,309,358,245,393,196,108};
    bc_capture_t c;
    bc_msg_t m = {0};
    bc_init(&c);
    assert(!feed(&c, true, 18375, &m));
    assert(!feed(&c, false, 3921, &m));
    assert(!bc_active(&c));
    for (unsigned i = 0; i < 29; i++)
        assert(!feed(&c, i % 2 == 0, d[i], &m));
    assert(feed(&c, false, 1, &m));
    assert(m.hdr.seq == 0 && m.hdr.count == 29 && route_code39_decode(&m) == 'D');

    /* A plausible first bar on a large label still extends its first gap. */
    bc_init(&c);
    assert(!feed(&c, true, 900, &m));
    assert(!feed(&c, false, 1800, &m));
    assert(bc_active(&c));
}

int main(void)
{
    circle_exit_before_d();
    unsigned n;
    /* Normal size: unchanged, Z decodes. */
    assert(run(1, &n) == 'Z' && n == 29);
    /* 5x bigger/slower: wide elements up to 1840 ms > the old 1.5 s.  The
       old fixed quiet time ended this capture after 2 elements. */
    assert(run(5, &n) == 'Z' && n == 29);
    printf("PASS normal and 5x barcode: 29 elements, Z\n");

    /* Partial barcode (car stops on it): capture still ends, at most 5 s
       of white after the last bar. */
    {
        bc_capture_t c; bc_msg_t m = {0};
        bc_init(&c);
        feed(&c, true, 900, &m); feed(&c, false, 300, &m);
        feed(&c, true, 300, &m); feed(&c, false, 300, &m);
        feed(&c, true, 300, &m);
        unsigned waited = 0; bool done = false;
        while (!done && waited < 10000) { done = bc_feed(&c, 0u, &m); waited++; }
        assert(done && m.hdr.count == 5);
        assert(waited >= 3600 && waited <= BC_QUIET_MAX_MS);   /* 4 x 900 ms */
        printf("PASS partial barcode ends after %u ms of white (4 x widest element)\n", waited);
    }
    /* Small elements: the 1.5 s floor still applies. */
    {
        bc_capture_t c; bc_msg_t m = {0};
        bc_init(&c);
        for (int k = 0; k < 3; k++) { feed(&c, true, 50, &m); feed(&c, false, 50, &m); }
        unsigned waited = 50; bool done = false;
        while (!done && waited < 10000) { done = bc_feed(&c, 0u, &m); waited++; }
        assert(done && waited >= BC_QUIET_MS && waited <= BC_QUIET_MS + 1);
        printf("PASS small elements still end after %u ms\n", waited);
    }
    return 0;
}

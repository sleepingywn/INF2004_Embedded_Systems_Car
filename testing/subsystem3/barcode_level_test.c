/* barcode_level_test.c - laptop check of the LINE_ROUTE barcode level rule.
 *   gcc -O2 -Wall -DBARCODE_SLOW_SCAN -DBARCODE_WIDE -I../../app_program/subsystem3 \
 *       -o barcode_level_test barcode_level_test.c -lm && ./barcode_level_test
 *
 * Sensor model fitted to the car (18 Nov 2026, raw ADC): white 177, wide bar
 * 1309, narrow bar 307, thin gap 177.  The spot is wider than a narrow element,
 * and the response is strongly non-linear: raw = 177 + 1323 * f^4.5, where f
 * is the black fraction under a 1.6-narrow-element spot.  That reproduces the
 * measured levels (narrow bar ~307, wide bar ~1300+) and the failure seen on
 * the track: thin gaps between wide bars only dip to ~190-260 at speed.
 * The pattern is the real Z scan (ms widths) from the KR4 car log (18 Nov).
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "barcode_level.h"
#include "../../app_program/subsystem2/motion_route_decode.h"

static const unsigned scan_z[29] = {66,117,368,113,255,117,104,298,88,125,83,126,95,103,273,
                                    128,270,325,82,138,90,195,320,46,305,111,74,344,76};

/* Build a 1 ms sensor trace: 300 ms white, the pattern, 300 ms white. */
static int trace(const unsigned *w, double spot_scale, double noise, unsigned *raw, int max)
{
    static unsigned char ideal[20000];
    int n = 0;
    for (int i = 0; i < 300; i++) ideal[n++] = 0;
    for (int e = 0; e < 29; e++)
        for (unsigned k = 0; k < w[e]; k++) ideal[n++] = (e % 2 == 0);
    for (int i = 0; i < 300; i++) ideal[n++] = 0;
    int spot = (int)(spot_scale * 1.6 * 90.0);  /* narrow element ~90 ms */
    for (int t = 0; t < n && t < max; t++) {
        int a = t - spot / 2, b = t + spot / 2, c = 0, m = 0;
        for (int k = a; k <= b; k++) if (k >= 0 && k < n) { m++; c += ideal[k]; }
        double f = (double)c / m;
        double r = 177.0 + 1323.0 * pow(f, 4.5) + noise * ((double)rand() / RAND_MAX - 0.5) * 2.0;
        raw[t] = r < 0 ? 0 : (unsigned)r;
    }
    return n;
}

/* Capture: elements from black/white changes; decode like the car. */
static char run(const unsigned *raw, int n, int relative, unsigned *count_out)
{
    bc_level_t s = {BC_LVL_WHITE_MAX, 0u, BC_LVL_WHITE_MAX, 0u};
    bc_msg_t m = {0};
    bool is_bar = false, started = false;
    unsigned len = 0, count = 0;
    for (int t = 0; t < n; t++) {
        bool black = relative ? bc_level_black(&s, is_bar, (uint16_t)raw[t])
                              : (raw[t] >= BC_LVL_BLACK_MIN || (is_bar && raw[t] > BC_LVL_WHITE_MAX));
        if (!started) {
            if (black) { started = true; is_bar = true; len = 1; }
            continue;
        }
        if (black == is_bar) { len++; continue; }
        if (count < BC_MAX_ELEMS) m.widths[count] = (bc_width_t)len;
        count++;
        is_bar = black; len = 1;
    }
    if (started && is_bar) { if (count < BC_MAX_ELEMS) m.widths[count] = (bc_width_t)len; count++; }
    m.hdr.count = count > 63 ? 63 : count;
    m.hdr.overflow = count > BC_MAX_ELEMS;
    *count_out = count;
    return route_code39_decode(&m);
}

int main(void)
{
    static unsigned raw[20000];
    /* spot x0.6-0.9: realistic; x1.0-1.2: spot much wider than the 46 ms gap,
       which then never dips at all - no threshold can recover that. */
    int fixed_ok = 0, rel_ok = 0, rel_wrong = 0, trials = 0, real_ok = 0, real_n = 0;
    for (int si = 6; si <= 12; si++) {
        double spot = si / 10.0;
        for (int seed = 0; seed < 20; seed++) {
            srand(seed + 1);
            int n = trace(scan_z, spot, 25.0, raw, 20000);
            unsigned cf, cr;
            char f = run(raw, n, 0, &cf), r = run(raw, n, 1, &cr);
            trials++;
            if (f == 'Z') fixed_ok++;
            if (r == 'Z') rel_ok++;
            if (r && r != 'Z') rel_wrong++;
            if (si <= 9) { real_n++; if (r == 'Z') real_ok++; }
            if (seed == 0) printf("spot x%.1f: fixed %2u elements -> %c   relative %2u elements -> %c\n",
                                  spot, cf, f ? f : '?', cr, r ? r : '?');
        }
    }
    printf("Z over %d noisy traces: fixed cutoffs decoded %d, relative decoded %d (%d/%d for spot <= x0.9), wrong letter %d\n",
           trials, fixed_ok, rel_ok, real_ok, real_n, rel_wrong);
    assert(rel_wrong == 0);
    assert(rel_ok > fixed_ok);
    assert(real_ok * 100 >= real_n * 95);

    /* Plain white with noise: never a bar. */
    {
        bc_level_t s = {BC_LVL_WHITE_MAX, 0u, BC_LVL_WHITE_MAX, 0u};
        srand(3);
        for (int t = 0; t < 5000; t++) {
            unsigned r = 177 + (unsigned)(rand() % 40);
            assert(!bc_level_black(&s, false, (uint16_t)r));
        }
    }
    /* Wide bar with +-100 texture noise around 1300: stays one bar. */
    {
        bc_level_t s = {BC_LVL_WHITE_MAX, 0u, BC_LVL_WHITE_MAX, 0u};
        bool bar = bc_level_black(&s, false, 1300);
        assert(bar);
        srand(4);
        for (int t = 0; t < 400; t++) {
            unsigned r = 1200 + (unsigned)(rand() % 200);
            bar = bc_level_black(&s, bar, (uint16_t)r);
            assert(bar);
        }
    }
    /* Narrow bar 307 over white 177: detected and ends cleanly. */
    {
        bc_level_t s = {BC_LVL_WHITE_MAX, 0u, BC_LVL_WHITE_MAX, 0u};
        bool bar = false;
        bar = bc_level_black(&s, bar, 177); assert(!bar);
        bar = bc_level_black(&s, bar, 307); assert(bar);
        bar = bc_level_black(&s, bar, 250); assert(bar);   /* above half way (242) */
        for (unsigned k = 1; k < BC_LVL_HOLD; k++) { bar = bc_level_black(&s, bar, 235); assert(bar); }
        bar = bc_level_black(&s, bar, 235); assert(!bar);  /* below half way for HOLD ms: gap */
        bar = bc_level_black(&s, bar, 260); assert(!bar);  /* needs +60 over the gap */
        bar = bc_level_black(&s, bar, 300); assert(bar);
    }
    /* Blurred gap between wide bars: 1300 -> 600 -> 1300 gives two bars. */
    {
        bc_level_t s = {BC_LVL_WHITE_MAX, 0u, BC_LVL_WHITE_MAX, 0u};
        bool bar = bc_level_black(&s, false, 1300);
        for (unsigned k = 0; k < BC_LVL_HOLD; k++) bar = bc_level_black(&s, bar, 600);
        assert(!bar);
        bar = bc_level_black(&s, bar, 1300); assert(bar);
    }
    /* Track A scan, 18 Nov: a narrow bar split by a 6 ms flicker
       ("B24 S6 B59").  A 6 ms dip half way down must not end the bar. */
    {
        bc_level_t s = {BC_LVL_WHITE_MAX, 0u, BC_LVL_WHITE_MAX, 0u};
        bool bar = bc_level_black(&s, false, 177);
        bar = bc_level_black(&s, bar, 520); assert(bar);
        for (int k = 0; k < 24; k++) { bar = bc_level_black(&s, bar, 520); assert(bar); }
        for (int k = 0; k < 6; k++) { bar = bc_level_black(&s, bar, 330); assert(bar); }
        for (int k = 0; k < 59; k++) { bar = bc_level_black(&s, bar, 520); assert(bar); }
    }
    puts("PASS barcode_level: white noise, textured wide bar, narrow bar, blurred gap, noisy Z traces, 6 ms flicker");
    return 0;
}

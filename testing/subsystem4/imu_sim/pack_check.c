/*
 * pack_check.c - laptop test for terrain_pack.c (S4-U15)
 * Checks the minimum-bit snapshot: size, round trip error within half a
 * step, saturation at the range limits, the "not available" turn code,
 * and the 15-bit time wrap.
 *   gcc -O2 -I../../../app_program/subsystem4 -o pack_check pack_check.c \
 *       ../../../app_program/subsystem4/terrain_pack.c
 */
#include "terrain_pack.h"
#include <stdio.h>
#include <math.h>

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

int main(void)
{
    terrain_values_t v = {0};
    S4_TERRAIN t;
    double worst_angle = 0, worst_turn = 0, worst_peak = 0;

    CHECK(sizeof(S4_TERRAIN) == 8, "snapshot is 8 bytes");

    /* round trip inside the range: error <= half a step */
    for (float a = -63.5f; a <= 63.5f; a += 0.01f) {
        v.pitch_deg = a; v.roll_deg = -a; v.turn_valid = true; v.turn_dps = a * 4.0f;
        t = terrain_pack(&v);
        double ep = fabs(S4_PITCH_DDEG(t) / 10.0 - a), er = fabs(S4_ROLL_DDEG(t) / 10.0 + a);
        double et = fabs(S4_TURN_DPS(t) - a * 4.0);
        if (ep > worst_angle) worst_angle = ep;
        if (er > worst_angle) worst_angle = er;
        if (et > worst_turn) worst_turn = et;
    }
    for (float m = 0; m <= 127.0f; m += 0.01f) {
        v.cur_peak_mm = m; v.high_peak_mm = m;
        t = terrain_pack(&v);
        double e = fabs((double)t.cur_peak_mm - m);
        if (e > worst_peak) worst_peak = e;
    }
    CHECK(worst_angle <= 0.25 + 1e-6, "pitch/roll within 0.25 deg");
    CHECK(worst_turn  <= 1.0 + 1e-6,  "turn rate within 1 deg/s");
    CHECK(worst_peak  <= 0.5 + 1e-6,  "peaks within 0.5 mm");

    /* saturation, not wrap-around */
    v.pitch_deg = 500; v.roll_deg = -500; v.turn_dps = 9999;
    v.cur_peak_mm = 1000; v.high_peak_mm = -5;
    t = terrain_pack(&v);
    CHECK(S4_PITCH_DDEG(t) == 635 && S4_ROLL_DDEG(t) == -640, "angles saturate");
    CHECK(S4_TURN_DPS(t) == 254 && S4_TURN_OK(t), "turn saturates, stays valid");
    CHECK(t.cur_peak_mm == 127 && t.high_peak_mm == 0, "peaks saturate");

    /* turn rate not available */
    v.turn_valid = false; t = terrain_pack(&v);
    CHECK(!S4_TURN_OK(t), "turn n/a code");

    /* flags, state, count wrap, 15-bit time and age across the wrap */
    v.hump_state = 2; v.motion_class = 5; v.hump_count = 17;
    v.calibrated = true; v.imu_fault = false; v.time_ms = 32760;
    t = terrain_pack(&v);
    CHECK(t.hump_state == 2 && t.motion_class == 5 && t.hump_count == 1, "state/class/count");
    CHECK(S4_VALID(t), "valid when calibrated and no fault");
    CHECK(S4_AGE_MS(t, 32780) == 20, "age across the 15-bit wrap");
    v.imu_fault = true; t = terrain_pack(&v);
    CHECK(!S4_VALID(t), "invalid on IMU fault");

    printf("size %zu bytes (was 38)\n", sizeof(S4_TERRAIN));
    printf("worst round-trip error: angle %.3f deg, turn %.2f deg/s, peak %.2f mm\n",
           worst_angle, worst_turn, worst_peak);
    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}

/* lift_check.c - laptop check of the lift / overturn detector (imu_logic.c).
 *   gcc -O2 -Wall -I../../../app_program/subsystem4 -o lift_check lift_check.c \
 *       ../../../app_program/subsystem4/imu_logic.c -lm && ./lift_check
 * Synthetic 50 Hz data in car axes (g).  Lift = 0.15 g check OR slow-lift sum. The real hump pass is checked with
 *   ./imu_sim --g ../logs/2026-10-31_A8_hump_pass.csv   (no LIFTED expected)
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "imu_logic.h"

/* +-0.01 g per axis: the recorded hump pass had |a| sd 0.006 g; idle car
   logs show |a| 995-1005 mg.  Motor vibration is added in check 8. */
static float noise(void) { return ((float)rand() / RAND_MAX - 0.5f) * 0.02f; }
static imu_logic_t s;
static unsigned lifted_events, by_tilt_events;
static float t;

static void start(void)
{
    imu_config_t cfg;
    imu_logic_default_config(&cfg);
    imu_logic_init(&s, &cfg);
    lifted_events = by_tilt_events = 0;
    t = 0.0f;
    srand(7);
    for (int i = 0; i < 110; i++) {            /* 2.2 s still and level: calibrate */
        imu_accel_t a = {noise(), noise(), 1.0f + noise()};
        imu_logic_update(&s, a, 0.02f, 0.0f, 0.0f);
        t += 0.02f;
    }
    assert(s.calibrated);
}
/* One sample: tilt (pitch about the side axis), extra upward accel, distance. */
static void step(float pitch_deg, float up_extra_g, float dist_mm)
{
    float p = pitch_deg * 0.0174533f;
    imu_accel_t a = {sinf(p) + noise(), noise(), cosf(p) + up_extra_g + noise()};
    uint32_t ev = imu_logic_update(&s, a, 0.02f, dist_mm, 0.0f);
    if (ev & IMU_EVT_LIFTED) {
        lifted_events++;
        if (s.lift_by_tilt) by_tilt_events++;
        printf("  [%6.2f s] LIFTED %s up %+.2f g tilt %.1f deg\n", t,
               s.lift_by_tilt ? "(overturned)" : "(lifted)", s.lift_peak_up_g, s.tilt_total_deg);
    }
    t += 0.02f;
}
static void hold(float seconds, float pitch_deg, float up_extra_g, float dist_mm)
{
    for (int i = 0; i < (int)(seconds / 0.02f + 0.5f); i++) step(pitch_deg, up_extra_g, dist_mm);
}

int main(void)
{
    /* 1. Driving 10 s with noise and single-sample bumps (+0.6 g, every 0.5 s). */
    start();
    for (int i = 0; i < 500; i++) step(0.0f, (i % 25 == 0) ? 0.6f : 0.0f, 0.66f);
    assert(lifted_events == 0 && !s.lifted);
    puts("PASS 1: driving with single-sample +0.6 g jolts -> no LIFTED");

    /* 2. Two-sample jolt (40 ms at +0.5 g): too short for the median + hold. */
    start();
    hold(1.0f, 0, 0, 0.66f); hold(0.04f, 0, 0.5f, 0.66f); hold(1.0f, 0, 0, 0.66f);
    assert(lifted_events == 0);
    puts("PASS 2: 40 ms +0.5 g jolt -> no LIFTED");

    /* 3. Hand lift: +0.35 g for 0.15 s, then slowing (-0.3 g) and held. */
    start();
    hold(1.0f, 0, 0, 0.66f); hold(0.15f, 0, 0.35f, 0); hold(0.15f, 0, -0.3f, 0);
    assert(lifted_events == 1 && by_tilt_events == 0 && s.lifted);
    assert(imu_classify_motion(&s, 0, 0, true, IMU_MOTION_STOPPED) == IMU_MOTION_LIFTED);
    /* Held still and level for 1.2 s: latch clears; a second lift triggers again. */
    hold(1.2f, 0, 0, 0);
    assert(!s.lifted);
    hold(0.15f, 0, 0.35f, 0);
    assert(lifted_events == 2);
    puts("PASS 3: hand lift (+0.35 g, 0.15 s) -> one LIFTED; clears after 1 s still; re-triggers");

    /* 4. Overturned: rolled over slowly (no upward push) to upside down. */
    start();
    for (int i = 0; i <= 90; i++) step(i * 2.0f, 0, 0);   /* 0 -> 180 deg over 1.8 s */
    hold(0.5f, 180.0f, 0, 0);
    assert(lifted_events == 1 && s.lifted);   /* picked-up check may fire first (wheels still) */
    puts("PASS 4: slowly overturned -> LIFTED (overturned or picked up)");

    /* 5. Hump tilt (7 deg) and a 30 deg parked slope: no LIFTED (TILT only). */
    start();
    hold(2.0f, 7.0f, 0, 0.66f); hold(2.0f, 30.0f, 0, 0);
    assert(lifted_events == 0);
    puts("PASS 5: hump tilt and 30 deg slope -> no LIFTED");

    /* 6. Calm hand lift (18 Nov 2026 car test: missed by the 0.15 g check):
     *    +0.08 g for 0.3 s, then -0.08 g for 0.3 s = rises ~0.24 m/s, ~7 cm.
     *    Caught by the slow-lift sum, not by the 0.15 g check. */
    start();
    hold(1.0f, 0, 0, 0); hold(0.3f, 0, 0.08f, 0); hold(0.3f, 0, -0.08f, 0); hold(0.5f, 0, 0, 0);
    assert(lifted_events == 1 && by_tilt_events == 0);
    puts("PASS 6: calm hand lift (+0.08 g, 0.3 s) -> LIFTED");

    /* 7. Slow lift with the wheels still (car test 19 Nov: a slow lift ended
     *    the run, then the car was held): +0.03 g for 0.3 s (~0.09 m/s) is
     *    caught by the still-wheels trigger.  Slower still (+0.012 g, ~0.04
     *    m/s, no tilt) is the documented limit. */
    start();
    hold(0.3f, 0, 0.03f, 0); hold(0.3f, 0, -0.03f, 0); hold(0.5f, 0, 0, 0);
    assert(lifted_events == 1);
    start();
    hold(0.3f, 0, 0.012f, 0); hold(0.3f, 0, -0.012f, 0); hold(0.5f, 0, 0, 0);
    assert(lifted_events == 0);
    puts("PASS 7: slow lift +0.03 g with wheels still -> LIFTED; +0.012 g -> not (limit)");

    /* 8. 30 s of rough driving: +-0.10 g random on every sample (sd 0.058 g,
     *    10x the recorded hump pass), plus a +0.6 g jolt every 0.5 s. */
    start();
    for (int i = 0; i < 1500; i++) {
        float r = ((float)rand() / RAND_MAX - 0.5f) * 0.20f;
        step(0.0f, r + ((i % 25 == 0) ? 0.6f : 0.0f), 0.66f);
    }
    assert(lifted_events == 0);
    puts("PASS 8: 30 s rough driving (+-0.10 g noise, jolts) -> no LIFTED");

    /* 9. Tilting 15 deg without rising while the wheels turn (e.g. a spin
     *    turn on uneven ground): |a| stays 1 g -> no slow-lift trigger. */
    start();
    s.wheels_moving = true;          /* e.g. a spin turn: the picked-up check is off, */
    for (int i = 0; i <= 15; i++) step((float)i, 0, 0);   /* only the rise sum is tested */
    hold(1.0f, 15.0f, 0, 0);
    s.wheels_moving = false;
    assert(lifted_events == 0);
    puts("PASS 9: tilted 15 deg in place -> no LIFTED");
    /* 10. Lifted in the middle of a hump climb, tilted in the hand, put
     *     down: no hump is counted (car test 19 Nov 2026). */
    start();
    hold(1.0f, 0, 0, 0.66f);
    for (int i = 0; i <= 10; i++) step(i * 1.0f, 0, 0.66f);   /* climbing to 10 deg */
    hold(0.3f, 10.0f, 0, 0.66f);
    hold(0.15f, 10.0f, 0.35f, 0);                             /* lifted */
    assert(s.lifted);
    hold(1.0f, 15.0f, 0, 0); hold(1.0f, -15.0f, 0, 0);        /* tilted in the hand */
    hold(2.0f, 0, 0, 0);                                      /* put down, still */
    assert(!s.lifted && s.hump_count == 0);
    puts("PASS 10: lifted mid-hump and tilted in the hand -> no hump counted");

    /* 11. Picked up while stopped (car test 19 Nov): wheels still, the car
     *     tilts 1 -> 12 deg in the hand with almost no upward push. */
    start();
    hold(1.0f, 0, 0, 0);                                      /* stopped, resting */
    for (int i = 0; i <= 24; i++) step(i * 0.5f, 0.02f, 0);   /* slow pick-up, 0 -> 12 deg */
    hold(0.5f, 12.0f, 0, 0);
    assert(lifted_events == 1 && s.lift_by_handle);
    puts("PASS 11: picked up while stopped (tilt moved 12 deg, wheels still) -> LIFTED");

    /* 12. Stopped ON the hump (tilt 7 deg, constant) for 5 s: not lifted. */
    start();
    for (int i = 0; i <= 14; i++) step(i * 0.5f, 0, 0.66f);  /* drive up to 7 deg */
    hold(5.0f, 7.0f, 0, 0);                                   /* stalled on the hump */
    assert(lifted_events == 0);
    puts("PASS 12: stopped on the hump (constant 7 deg) -> no LIFTED");
    return 0;
}

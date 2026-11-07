#ifndef MOTION_CONFIG_H
#define MOTION_CONFIG_H

/* User-confirmed motor mapping. GPIO numbers, not header pin numbers. */
#define MOTION_LEFT_A 8u
#define MOTION_LEFT_B 9u
#define MOTION_RIGHT_A 10u
#define MOTION_RIGHT_B 11u
/* Proposed encoder mapping: verify voltage, A/B order and count convention. */
#define MOTION_ENCODER_LEFT_A 4u
#define MOTION_ENCODER_LEFT_B 5u
#define MOTION_ENCODER_RIGHT_A 2u
#define MOTION_ENCODER_RIGHT_B 3u

#define MOTION_PERIOD_MS 20u
#define MOTION_PWM_HZ 10000u
#define MOTION_PWM_TOP 999u
#define MOTION_PRIORITY 14
#define MOTION_SCHEMA 1u

/* ZERO means unknown, NOT a measured zero. Keep disabled until calibrated.
 * x4 counts per OUTPUT WHEEL revolution, not pulses per motor revolution.
 * Signs must be +1 or -1; positive logical motion means vehicle forward.
 * No physical test or tuning result is implied by this file. */
/* Explicit integrated local line trial (2 s, or opt-in 30 s button profile).
 * Does not grant calibrated
 * metric odometry or permission for distance/turn/network commands. */
#ifndef MOTION_LINE_DEMO
#define MOTION_LINE_DEMO 0
#endif
#ifndef MOTION_LINE_CIRCUIT
#define MOTION_LINE_CIRCUIT 0
#endif
#ifndef MOTION_LINE_AUTOSTART
#define MOTION_LINE_AUTOSTART 0
#endif
#ifndef MOTION_LINE_SEARCH
#define MOTION_LINE_SEARCH 0
#endif
#ifndef MOTION_LINE_ADAPTIVE
#define MOTION_LINE_ADAPTIVE 0
#endif
#if MOTION_LINE_ADAPTIVE && (!MOTION_LINE_CIRCUIT || MOTION_LINE_AUTOSTART || MOTION_LINE_SEARCH)
#error "Adaptive control requires GP20 circuit mode with legacy search disabled"
#endif
#if MOTION_LINE_SEARCH && (!MOTION_LINE_CIRCUIT || MOTION_LINE_AUTOSTART)
#error "Bounded search requires the explicit GP20 circuit profile"
#endif
#if MOTION_LINE_AUTOSTART && !MOTION_LINE_CIRCUIT
#error "Autostart requires the bounded battery circuit profile"
#endif
#if MOTION_LINE_CIRCUIT && !MOTION_LINE_DEMO
#error "Circuit profile requires the local line profile"
#endif
#define MOTION_LINE_BUTTON_PIN 20u /* Robo Pico active-low programmable button */
#define MOTION_CALIBRATED 0
#define MOTION_ENCODERS_VERIFIED 1 /* verified 2026-10-31: both wheels count both ways, 0 invalid edges */
#define MOTION_LEFT_MOTOR_SIGN -1   /* measured 2026-10-31: +duty spins left wheel backward */
#define MOTION_RIGHT_MOTOR_SIGN 1   /* measured 2026-10-31: +duty spins right wheel forward */
#define MOTION_LEFT_ENCODER_SIGN -1 /* measured: left count goes DOWN when driving forward */
#define MOTION_RIGHT_ENCODER_SIGN 1 /* measured: right count goes UP when driving forward */
#define MOTION_LEFT_COUNTS_REV 2774u  /* ROUGH: one hand turn gave 2679 L / 2869 R; refine with a 1 m push */
#define MOTION_RIGHT_COUNTS_REV 2774u
#define MOTION_LEFT_CIRCUMFERENCE_UM 204200u /* 65 mm wheel (measured 64-66 mm) */
#define MOTION_RIGHT_CIRCUMFERENCE_UM 204200u
#define MOTION_TRACK_WIDTH_MM 125u /* measured, wheel centre to centre */
#define MOTION_MAX_SPEED_MM_S 0u
#define MOTION_CRUISE_MM_S 0u
#define MOTION_TURN_MM_S 0u
#define MOTION_DUTY_LIMIT 0u /* permille, 1..1000 */
#define MOTION_POSITION_TOLERANCE_MM 0u
#define MOTION_STALL_MS 0u
#define MOTION_INPUT_MAX_AGE_MS 0u
#define MOTION_MANOEUVRE_TIMEOUT_MS 0u
#define MOTION_REVERSE_REST_MS 0u
#define MOTION_LINE_KP_Q10 0u     /* mm/s per error unit, x1024 */
#define MOTION_STRAIGHT_KP_Q10 0u /* mm/s per mm mismatch, x1024 */
#define MOTION_PID_KP_Q10 0u      /* permille per mm/s, x1024 */
#define MOTION_PID_KI_Q10 0u      /* permille per mm/s per second */
#define MOTION_PID_KD_Q10 0u      /* permille per mm/s/second */

/* Explicit development profile; production keeps conservative pulse limits. */
#ifndef MOTION_DEVELOPMENT
#define MOTION_DEVELOPMENT 0
#endif
#if MOTION_DEVELOPMENT
#define MOTION_BENCH_DUTY_MAX 600
#define MOTION_BENCH_TIME_MAX_MS 4000u
#else
#define MOTION_BENCH_DUTY_MAX 200
#define MOTION_BENCH_TIME_MAX_MS 500u
#endif

#endif

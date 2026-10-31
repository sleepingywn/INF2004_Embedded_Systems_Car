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
#define MOTION_CALIBRATED 0
#define MOTION_ENCODERS_VERIFIED 0
#define MOTION_LEFT_MOTOR_SIGN 0
#define MOTION_RIGHT_MOTOR_SIGN 0
#define MOTION_LEFT_ENCODER_SIGN 0
#define MOTION_RIGHT_ENCODER_SIGN 0
#define MOTION_LEFT_COUNTS_REV 0u
#define MOTION_RIGHT_COUNTS_REV 0u
#define MOTION_LEFT_CIRCUMFERENCE_UM 0u
#define MOTION_RIGHT_CIRCUMFERENCE_UM 0u
#define MOTION_TRACK_WIDTH_MM 0u
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

/* Bench pulses are explicit API requests, never automatic startup tests.
 * Match the standalone test; limited to 20% and 500 ms. */
#define MOTION_BENCH_DUTY_MAX 200
#define MOTION_BENCH_TIME_MAX_MS 500u

#endif

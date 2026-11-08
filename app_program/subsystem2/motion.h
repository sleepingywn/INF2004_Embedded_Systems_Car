#ifndef MOTION_H
#define MOTION_H
#include "../app.h"
#include "motion_config.h"
#include "motion_bits.h"
#include "../subsystem3/ir_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MOTION_VELOCITY,
    MOTION_DISTANCE,
    MOTION_TURN,
    MOTION_FOLLOW,
    MOTION_BENCH,
    MOTION_RESET,
    MOTION_LINE_TRACE /* local bounded count-domain line trial, no metric claims */
} motion_command_t;
typedef enum
{
    MOTION_IDLE,
    MOTION_DRIVING,
    MOTION_TRAVELLING,
    MOTION_TURNING,
    MOTION_TRACING,
    MOTION_REACQUIRE,
    MOTION_ERROR,
    MOTION_PULSING
} motion_state_t;
typedef enum
{
    MOTION_NONE,
    MOTION_ACCEPTED,
    MOTION_COMPLETED,
    MOTION_CANCELLED,
    MOTION_EXPIRED,
    MOTION_FAILED,
    MOTION_REJECTED
} motion_result_t;
typedef enum
{
    MOTION_LOCAL,
    MOTION_CONTROLLER,
    MOTION_NETWORK,
    MOTION_TERRAIN
} motion_source_t;
typedef enum
{
    MOTION_HEALTH_LOCAL,
    MOTION_HEALTH_IR,
    MOTION_HEALTH_NETWORK,
    MOTION_HEALTH_TERRAIN
} motion_fault_source_t;
typedef enum
{
    MOTION_FAULT_ENCODER = 1u,
    MOTION_FAULT_STALL = 2u,
    MOTION_FAULT_TIMEOUT = 4u,
    MOTION_FAULT_IR = 8u,
    MOTION_FAULT_HANDOFF = 16u,
    MOTION_FAULT_RANGE = 32u,
    MOTION_FAULT_EXTERNAL = 64u,
    MOTION_FAULT_HARDWARE = 128u
} motion_fault_t;

/* One admitted request at a time, copied into B2-owned storage. A successful
 * submit means queued, not completed. Inspect status for its id/epoch/result.
 * Fresh id must increase within an epoch. No implicit queueing/replacement.
 * STOP is a separate persistent path and cannot be blocked by a full slot. */
typedef struct
{
    uint32_t epoch;
    uint32_t id;
    uint32_t issued_ms;
    uint16_t ttl_ms : 15; /* 2..30000; finite operations must finish in it */
    uint8_t version : 2;
    uint8_t source : 2;
    uint8_t command : 3;
    uint8_t reserved : 1;
    union
    {
        uint32_t distance_bits : 20; /* encoded +/-524287 */
        uint16_t angle_bits : 9;     /* encoded +/-180, MOTION_TURN only */
    } value;
    uint16_t left_bits : 12; /* encoded velocity mm/s or electrical permille */
    uint16_t right_bits : 12;
} motion_request_t;

typedef struct
{
    uint32_t time_ms;    /* encoder acquisition time, modulo 2^32 */
    uint32_t updated_ms; /* state publication time; not encoder time */
    uint32_t sequence;
    uint32_t epoch; /* changes on stop/fault/reset; no epoch mixing */
    uint32_t request_id;
    uint32_t request_epoch;
    uint32_t left_count; /* modulo 2^32 raw x4 edge counts */
    uint32_t right_count;
    uint32_t left_distance_bits : 24;
    uint32_t faults : 8;
    uint32_t right_distance_bits : 24;
    uint32_t state : 3;
    uint32_t result : 3;
    bool valid : 1;
    bool calibrated : 1;
    uint16_t left_speed_bits : 12;
    uint16_t right_speed_bits : 12;
    uint32_t barcode : 3; /* 0 none, 1 A/left, 2 B/right, 3 C, 4 D/U-turn */
    uint32_t source : 2;
    bool raw_valid : 1; /* coherent encoder acquisition, independent of calibration */
    bool ready : 1; /* application startup gate released; faults still apply */
    uint32_t reserved : 1;
    uint16_t left_duty_bits : 11;
    uint16_t right_duty_bits : 11;
    uint32_t invalid_edges;
    uint32_t rejected_requests;
    uint32_t rejected_barcodes;
} motion_status_t;

/* All signed physical values are accessed through these helpers. The *_bits
 * members are compact unsigned storage, not already-signed measurements. */
static inline int32_t motion_request_left(const motion_request_t *request)
{
    return (int32_t)motion_bits_decode(request->left_bits, 12u);
}
static inline int32_t motion_request_right(const motion_request_t *request)
{
    return (int32_t)motion_bits_decode(request->right_bits, 12u);
}
static inline int32_t motion_request_value(const motion_request_t *request)
{
    return (request->command == MOTION_TURN)
               ? (int32_t)motion_bits_decode(request->value.angle_bits, 9u)
               : (int32_t)motion_bits_decode(request->value.distance_bits, 20u);
}
static inline int32_t motion_left_speed(const motion_status_t *status)
{
    return (int32_t)motion_bits_decode(status->left_speed_bits, 12u);
}
static inline int32_t motion_right_speed(const motion_status_t *status)
{
    return (int32_t)motion_bits_decode(status->right_speed_bits, 12u);
}
static inline int32_t motion_left_distance(const motion_status_t *status)
{
    return (int32_t)motion_bits_decode(status->left_distance_bits, 24u);
}
static inline int32_t motion_right_distance(const motion_status_t *status)
{
    return (int32_t)motion_bits_decode(status->right_distance_bits, 24u);
}
static inline int32_t motion_left_duty(const motion_status_t *status)
{
    return (int32_t)motion_bits_decode(status->left_duty_bits, 11u);
}
static inline int32_t motion_right_duty(const motion_status_t *status)
{
    return (int32_t)motion_bits_decode(status->right_duty_bits, 11u);
}

uint32_t motion_now_ms(void);
/* Use this constructor to range-check wide caller values BEFORE bit-field
 * narrowing. Direct assignment of an out-of-range bit-field loses information. */
ER motion_request_init(motion_request_t *request, motion_command_t command, motion_source_t source,
                       uint32_t epoch, uint32_t id, uint32_t ttl_ms, int32_t value, int32_t left,
                       int32_t right);
ER motion_submit(const motion_request_t *request);
/* Renew an existing velocity/follow request; does not queue another movement.
 * Caller must preserve id/source/epoch and supply a newer issue timestamp. */
ER motion_renew(uint32_t epoch, uint32_t id, motion_source_t source, uint32_t issued_ms,
                uint16_t ttl_ms);
ER motion_stop(void);
ER motion_set_fault(motion_fault_source_t source, bool asserted);
typedef struct {
    uint32_t raw_left,raw_right,previous_left,previous_right,origin_left,origin_right;
    uint32_t initial_invalid,invalid,dt_ms;
    int32_t speed_previous_left,speed_previous_right,target_left,target_right;
    uint8_t reason,speed_reason,raw_valid;
    /* Frozen previous commanded interval, before owner reset. Deltas are
     * forward-positive logical counts; original raw counters remain above. */
    int32_t delta_left,delta_right,prior_target_left,prior_target_right;
    uint16_t prior_duty_left,prior_duty_right;
    uint8_t pause_eligible,pause_used_left,pause_used_right,pause_budget;
    uint32_t pause_age_left_ms,pause_age_right_ms;
    uint32_t settle_age_left_ms,settle_age_right_ms; /* UINT32_MAX if unused */
    uint32_t pause_window_left_ms,pause_window_right_ms,pause_window_limit_ms;
} motion_encoder_failure_t;
typedef struct {
    uint32_t first_timeout_ms, first_timeout_interval_ms, max_loop_ms;
    uint32_t app_ready_ms, radio_init_ms, released_ms;
    uint8_t timeout_reason; /* 1 startup, 2 loop, 3 encoder, 4 request, 5 manoeuvre, 6 reacquire */
    uint8_t fault_sources; /* bit per motion_fault_source_t */
    uint8_t first_timeout_state;
    uint16_t line_speed_left_cps,line_speed_right_cps;
    uint16_t line_target_left_cps,line_target_right_cps;
    int8_t line_error;
    int8_t line_steering_error; /* Applied error, including bounded curve memory. */
    int8_t line_tight_direction; /* +1 left pause, -1 right pause, 0 rolling */
    uint8_t line_search_phase; /* 0 tracking, 1 initial sweep, 2 return sweep */
    uint8_t line_step,line_active;
    uint16_t line_levels;
    uint8_t line_adaptive_mode,line_stop_reason;
    /* Frozen failing input, separate from last-successful-control diagnostics.
     * Flags: 1 captured, 2 had line, 4 epoch matched, 8 line.fault asserted. */
    uint32_t line_failure_age_ms,line_failure_elapsed_ms;
    int8_t line_failure_error;
    uint8_t line_failure_state,line_failure_flags;
    motion_encoder_failure_t encoder_failure;
} motion_diagnostics_t;
ER motion_get_diagnostics(motion_diagnostics_t *out);
/* Read-only stopped-run history. Index 0 is oldest; E_OBJ during motion,
 * E_NOEXS for an absent index. Callers verify epoch/id across multiple reads. */
#define MOTION_LINE_TRACE_SAMPLES 64u
typedef struct {
    uint32_t elapsed_ms,age_ms;
    int32_t speed_left_cps,speed_right_cps;
    uint16_t target_left_cps,target_right_cps;
    int16_t duty_left,duty_right;
    int8_t error,steering;
    int8_t tight_direction;
    uint8_t search_phase;
    uint16_t levels; /* Packed calibrated left/right + validity, same sample. */
    uint8_t adaptive_mode,stop_reason;
    uint8_t state,step,flags; /* flags: input present=2, epoch match=4, fault=8 */
} motion_line_trace_sample_t;
typedef struct {
    motion_line_trace_sample_t sample;
    uint32_t epoch,id;
    uint16_t count;
} motion_line_trace_entry_t;
ER motion_get_line_trace(unsigned index,motion_line_trace_entry_t *out);
ER motion_get_status(motion_status_t *status);
/* IR task only: tolerate brief snapshot contention. The 2 ms wait request is
 * rounded by the kernel tick; the motion publisher itself never waits. */
ER motion_get_ir_status(motion_status_t *status);
/* S3 task context only. Caller supplies FULL acquisition time and current
 * B2 epoch, not merely reception time. Never call these from an ADC ISR.
 * E_OK means accepted. MOTION_IR_SUPERSEDED means rejected because the epoch
 * changed: discard the sample/capture and resynchronise, never retry it under
 * a new epoch. Other failures remain negative kernel errors. */
enum { MOTION_IR_SUPERSEDED = 1 };
ER motion_publish_line(const line_msg_t *line, uint32_t sampled_ms, uint32_t epoch);
ER motion_publish_line_observed(const line_msg_t *line, uint16_t levels,
                                uint32_t sampled_ms, uint32_t epoch);
ER motion_publish_barcode(const bc_msg_t *barcode, uint32_t sampled_ms, uint32_t epoch);
/* INTEGRATION TEST: latest line sample / barcode capture received from S3,
 * whatever the motion state.  motion_debug_line returns E_NOEXS before the
 * first sample. */
ER motion_debug_line(line_msg_t *line, uint32_t *sampled_ms);
ER motion_debug_barcode(bc_msg_t *barcode, uint32_t *sampled_ms, uint32_t *count);
#if MOTION_LINE_ROUTE
/* Barcode routing log, drained by the console task (single reader).
 * kind: 1 barcode, 2 armed, 3 not armed, 4 junction, 5 exit, 6 leg done,
 *       7 V5 resumed, 8 route failure, 9 30 s segment limit reached. */
enum { ROUTE_EV_BARCODE = 1, ROUTE_EV_ARMED, ROUTE_EV_NOT_ARMED, ROUTE_EV_JUNCTION,
       ROUTE_EV_EXIT, ROUTE_EV_LEG_DONE, ROUTE_EV_RESUMED, ROUTE_EV_FAILED,
       ROUTE_EV_TIME_LIMIT };
typedef struct {
    uint32_t ms;
    uint8_t kind;
    char code;        /* letter concerned, 0 = none */
    int16_t v[4];     /* kind-specific values, see line_console.c */
} motion_route_event_t;
/* Full widths of the latest 29-element-or-longer capture, for the log. */
typedef struct {
    bc_msg_t by_time, by_distance;
    bool distance_valid;
    char time_code, distance_code, old_code, chosen;
    uint32_t sequence;
} motion_route_barcode_t;
bool motion_route_next_event(motion_route_event_t *event);
bool motion_route_barcode(motion_route_barcode_t *out, uint32_t *last_sequence);
#endif
ER motion_init(void);
ER motion_start(void);
/* Called by app_main only after all subsystem start functions succeed. */
ER motion_application_ready(void);
void motion_task(INT stacd, void *exinf);
#endif

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
    MOTION_RESET
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
    uint32_t reserved : 3;
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
ER motion_get_status(motion_status_t *status);
/* S3 task context only. Caller supplies FULL acquisition time and current
 * B2 epoch, not merely reception time. Never call these from an ADC ISR. */
ER motion_publish_line(const line_msg_t *line, uint32_t sampled_ms, uint32_t epoch);
ER motion_publish_barcode(const bc_msg_t *barcode, uint32_t sampled_ms, uint32_t epoch);
ER motion_init(void);
ER motion_start(void);
void motion_task(INT stacd, void *exinf);
#endif

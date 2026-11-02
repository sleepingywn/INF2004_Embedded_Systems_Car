#ifndef TELEMETRY_WIRE_H
#define TELEMETRY_WIRE_H
#include <stdint.h>
/* Version 1: explicit little-endian fields; never transmit C struct layout.
 * Validity bits: 0 motion snapshot, 1 physical calibration, 2 IR, 3 terrain.
 * Bits 4/5: fresh motion status / application ready.
 * Event types: 0 snapshot, 1 motor transition, 16..20 S4 event indices.
 * A cleared bit means unavailable, even when the associated bytes are zero.
 */
#define S1_FRAME_SIZE 80u
#define S1_WIRE_VERSION 1u
typedef struct {
    uint32_t boot_id, sequence, published_ms;
    uint32_t motion_ms, motion_epoch, motion_sequence;
    uint32_t left_count, right_count;
    uint32_t ir_ms, terrain_age_ms;
    int16_t left_speed, right_speed, line_error;
    int8_t pitch_half_deg, roll_half_deg;
    uint8_t valid, state, faults, result;
    uint8_t current_peak_mm, highest_peak_mm, hump_count, motion_class;
    uint32_t dropped_events;
    uint8_t event_type, event_source;
    uint32_t event_ms, event_count;
    int32_t event_value;
} s1_wire_values_t;
void s1_encode_frame(uint8_t out[S1_FRAME_SIZE], const s1_wire_values_t *v);
#endif

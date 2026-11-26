#include "telemetry_wire.h"
static void put16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}
static void put32(uint8_t *bytes, uint32_t value)
{
    put16(bytes, (uint16_t)value);
    put16(bytes + 2, (uint16_t)(value >> 16));
}
void s1_encode_frame(uint8_t out[S1_FRAME_SIZE], const s1_wire_values_t *value)
{
    unsigned index;
    for (index = 0u; index < S1_FRAME_SIZE; ++index)
    {
        out[index] = 0u;
    }

    out[0] = 'R';
    out[1] = 'C';
    out[2] = S1_WIRE_VERSION;
    out[3] = value->valid;
    put32(out + 4, value->boot_id);
    put32(out + 8, value->sequence);
    put32(out + 12, value->published_ms);
    put32(out + 16, value->motion_ms);
    put32(out + 20, value->motion_epoch);
    put32(out + 24, value->motion_sequence);
    put32(out + 28, value->left_count);
    put32(out + 32, value->right_count);
    put32(out + 36, value->ir_ms);
    put32(out + 40, value->terrain_age_ms);
    put16(out + 44, (uint16_t)value->left_speed);
    put16(out + 46, (uint16_t)value->right_speed);
    put16(out + 48, (uint16_t)value->line_error);
    out[50] = (uint8_t)value->pitch_half_deg;
    out[51] = (uint8_t)value->roll_half_deg;
    out[52] = value->state;
    out[53] = value->faults;
    out[54] = value->result;
    out[55] = value->current_peak_mm;
    out[56] = value->highest_peak_mm;
    out[57] = value->hump_count;
    out[58] = value->motion_class;
    /* byte 59 reserved, always zero */
    put32(out + 60, value->dropped_events);
    out[64] = value->event_type;
    out[65] = value->event_source;
    put32(out + 68, value->event_ms);
    put32(out + 72, value->event_count);
    put32(out + 76, (uint32_t)value->event_value);
    out[80] = value->barcode_code;
    out[81] = value->barcode_elements;
    out[82] = value->barcode_flags;
    put32(out + 84, value->barcode_sequence);
    put32(out + 88, value->barcode_ms);
    put32(out + 92, value->barcode_epoch);
}

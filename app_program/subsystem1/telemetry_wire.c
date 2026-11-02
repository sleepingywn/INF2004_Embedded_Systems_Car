#include "telemetry_wire.h"
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v)
{
    put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16));
}
void s1_encode_frame(uint8_t out[S1_FRAME_SIZE], const s1_wire_values_t *v)
{
    unsigned i;
    for (i = 0; i < S1_FRAME_SIZE; ++i) out[i] = 0;
    out[0] = 'R'; out[1] = 'C'; out[2] = S1_WIRE_VERSION;
    out[3] = v->valid;
    put32(out+4, v->boot_id); put32(out+8, v->sequence);
    put32(out+12, v->published_ms); put32(out+16, v->motion_ms);
    put32(out+20, v->motion_epoch); put32(out+24, v->motion_sequence);
    put32(out+28, v->left_count); put32(out+32, v->right_count);
    put32(out+36, v->ir_ms); put32(out+40, v->terrain_age_ms);
    put16(out+44, (uint16_t)v->left_speed);
    put16(out+46, (uint16_t)v->right_speed);
    put16(out+48, (uint16_t)v->line_error);
    out[50] = (uint8_t)v->pitch_half_deg;
    out[51] = (uint8_t)v->roll_half_deg;
    out[52] = v->state; out[53] = v->faults; out[54] = v->result;
    out[55] = v->current_peak_mm; out[56] = v->highest_peak_mm;
    out[57] = v->hump_count; out[58] = v->motion_class;
    /* byte 59 reserved, always zero */
    put32(out+60, v->dropped_events);
    out[64] = v->event_type; out[65] = v->event_source;
    put32(out+68, v->event_ms); put32(out+72, v->event_count);
    put32(out+76, (uint32_t)v->event_value);
}

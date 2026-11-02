#include "subsystem1.h"
#include "telemetry_transport.h"
#include "telemetry_time.h"
#include "../subsystem2/motion.h"
#include "../subsystem4/subsystem4.h"
/* Called by S1 only. Network APIs remain in the Wi-Fi service task. */
/* RP2040 ROSC RANDOMBIT, read-only. A boot discriminator, not a secret.
 * Sample with spacing and mix 256 bits; a 32-bit tag is probabilistic.
 * Receiver also checks uptime/sequence and never treats this as authentication.
 */
static uint32_t boot_tag(void)
{
    volatile const uint32_t *random_bit = (volatile const uint32_t *)0x4006001cu;
    uint32_t tag = 2166136261u;
    unsigned i, spacing;
    for (i = 0; i < 256u; ++i) {
        for (spacing = 0; spacing < 64u; ++spacing) __asm__ volatile("nop");
        tag = (tag ^ (*random_bit & 1u)) * 16777619u;
    }
    return tag ? tag : 1u;
}
void s1_collect(void)
{
    static uint32_t sequence, boot_id;
    static uint8_t previous_faults, previous_result;
    static uint32_t seen_events[S4_EV_NUM], motion_event_count, lost_events;
    static uint32_t terrain_changed_ms;
    static uint16_t terrain_stamp;
    static int terrain_seen;
    s1_wire_values_t v = {0};
    motion_status_t m;
    line_msg_t line;
    S4_TERRAIN t;
    uint8_t frame[S1_FRAME_SIZE];
    uint32_t now = motion_now_ms(), line_ms;
    int motion_available = 0;
    v.sequence = ++sequence;
    v.published_ms = now;
    if (!boot_id) boot_id = boot_tag();
    v.boot_id = boot_id;
    if (motion_get_status(&m) == E_OK) {
        motion_available = 1;
        if ((uint32_t)(now - m.updated_ms) <= 100u) v.valid |= 16u;
        if (m.ready) v.valid |= 32u;
        v.motion_ms = m.time_ms; v.motion_epoch = m.epoch;
        v.motion_sequence = m.sequence;
        v.left_count = m.left_count; v.right_count = m.right_count;
        v.state = m.state; v.faults = m.faults; v.result = m.result;
        if (m.raw_valid && (uint32_t)(now - m.time_ms) <= 100u) v.valid |= 1u;
        if (m.valid && m.calibrated) v.valid |= 2u;
        v.left_speed = motion_left_speed(&m);
        v.right_speed = motion_right_speed(&m);
    }
    if (motion_debug_line(&line, &line_ms) == E_OK) {
        v.ir_ms = line_ms; v.line_error = line.err;
        if (!line.fault && line.state != LINE_CALIBRATING && line.state != LINE_FAULT && (uint32_t)(now - line_ms) <= 100u) v.valid |= 4u;
    }
    if (s4_get_terrain(&t) == E_OK) {
        if (!terrain_seen || terrain_stamp != t.time_ms) {
            terrain_seen = 1; terrain_stamp = t.time_ms; terrain_changed_ms = now;
        }
        /* S4 publishes kernel-clock timestamps; keep its interface unchanged. */
        SYSTIM terrain_now;
        tk_get_tim(&terrain_now);
        v.terrain_age_ms = s1_terrain_age(terrain_now.lo, t.time_ms);
        /* Full-time change tracking prevents old 15-bit timestamps becoming
         * apparently fresh again after a wrap while the producer is stalled. */
        if (S4_VALID(t) && s1_terrain_fresh(terrain_now.lo, t.time_ms,
            now, terrain_changed_ms, S4_STALE_MS)) v.valid |= 8u;
        v.pitch_half_deg = t.pitch_hd; v.roll_half_deg = t.roll_hd;
        v.current_peak_mm = t.cur_peak_mm; v.highest_peak_mm = t.high_peak_mm;
        v.hump_count = t.hump_count; v.motion_class = t.motion_class;
    }
    v.dropped_events = lost_events;
    s1_encode_frame(frame, &v);
    (void)s1_transport_offer(frame, 0);
    if (motion_available && (v.faults != previous_faults || v.result != previous_result)) {
        v.sequence = ++sequence;
        v.event_type = 1; v.event_source = 2;
        v.event_ms = m.updated_ms; v.event_count = ++motion_event_count;
        v.event_value = v.faults;
        s1_encode_frame(frame, &v);
        if (!s1_transport_offer(frame, 1) && lost_events != UINT32_MAX) ++lost_events;
        previous_faults = v.faults; previous_result = v.result;
    }
    S4_EVENTS records;
    if (s4_get_events(&records) == E_OK) {
        for (unsigned i = 0; i < S4_EV_NUM; ++i) {
            if (records.ev[i].count == seen_events[i]) continue;
            v.sequence = ++sequence;
            v.event_type = (uint8_t)(16u + i); v.event_source = records.source;
            v.event_ms = records.ev[i].time_ms;
            v.event_count = records.ev[i].count; v.event_value = records.ev[i].value;
            /* Count records coalesced by the source before S1 could copy them. */
            uint32_t gap = records.ev[i].count - seen_events[i] - 1u;
            if (gap > UINT32_MAX - lost_events) lost_events = UINT32_MAX;
            else lost_events += gap;
            v.dropped_events = lost_events;
            s1_encode_frame(frame, &v);
            if (!s1_transport_offer(frame, 1) && lost_events != UINT32_MAX) ++lost_events;
            seen_events[i] = records.ev[i].count;
        }
    }
}

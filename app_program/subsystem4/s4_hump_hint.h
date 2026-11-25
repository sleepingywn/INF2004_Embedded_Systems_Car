/* s4_hump_hint.h - "the IMU sees the car on a hump" for the line follower.
 *
 * Car test 25 Nov 2026: on the one-sided hump the car tilts ~7 deg, the
 * line sensors lift off the floor and read black on both sides.  V5 then
 * treats it as a wide black mark and ends the run after 400 counts
 * (LINE_STOP_MARK_DISTANCE, trace reason=5), on 1 in 3 hump passes.
 *
 * Single writer: terrain_task (S4) stores motion_now_ms() every 20 ms cycle
 * while it is calibrated, not lifted, has no IMU fault and its hump state is
 * CLIMBING or DESCENDING.  One aligned 32-bit word, so the write is atomic.
 * Reader: motion_task (S2).  The hint expires S4_HUMP_HINT_MS after the
 * last write, so a stopped/faulted terrain task cannot keep it on.
 */
#ifndef S4_HUMP_HINT_H
#define S4_HUMP_HINT_H
#include <stdbool.h>
#include <stdint.h>

#define S4_HUMP_HINT_MS 100u

extern volatile uint32_t s4_hump_seen_ms;   /* 0 = never */

static inline bool s4_hump_recent(uint32_t now_ms)
{
    uint32_t t = s4_hump_seen_ms;
    return t != 0u && (uint32_t)(now_ms - t) <= S4_HUMP_HINT_MS;
}
#endif

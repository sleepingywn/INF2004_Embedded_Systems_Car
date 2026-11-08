/* Barcode routing on top of V5 line following (MOTION_LINE_ROUTE).
 * Ported from the standalone bench (testing/subsystem3/ir_line_bench,
 * scan_drive.h, branch local/v5-barcode-turning W3) which was proven on the
 * track on 8 Nov 2026. Header-only, no kernel calls: motion.c owns timing,
 * encoders, motors and logging; host tests include this directly.
 *
 * A decoded barcode arms ONE manoeuvre for the next junction:
 *   A = right arc, B or Z = left arc, C = straight 100 mm, D = 180 deg pivot.
 * A junction is both line sensors LINE_BOTH_BLACK for >= 20 ms. With nothing
 * armed, V5 handles the mark itself (straight across). */
#ifndef MOTION_ROUTE_H
#define MOTION_ROUTE_H
#include <stdbool.h>
#include <stdint.h>
#include "../subsystem3/ir_types.h"
#include "motion_route_decode.h"

/* Values from the bench (Ken's trial + W1 handover fix). Do not retune here. */
#define ROUTE_JUNCTION_CONFIRM_MS 20u
#define ROUTE_ARC_OUTER_CPS 750
#define ROUTE_ARC_INNER_CPS 200
#define ROUTE_LEG_CPS 450
#define ROUTE_ARC_MIN_LEAD 150       /* counts of wheel difference before the turn-side sensor counts */
#define ROUTE_ARC_EITHER_SIDE_LEAD 300
#define ROUTE_EXIT_MIN_DARKNESS 35u
#define ROUTE_EXIT_MIN_CONTRAST 10
#define ROUTE_EXIT_CONFIRM_MS 10u    /* bench: 10 ms; kernel sees a new line sample every 20 ms loop */
#define ROUTE_TURN_TIMEOUT_MS 6000u
#define ROUTE_SETTLE_MS 300u
#define ROUTE_STALL_MS 400u
#define ROUTE_LINE_FRESH_MS 100u
#define ROUTE_STRAIGHT_COUNTS 1358u  /* 100 mm: 100000 um * 2774 / 204200 */
#define ROUTE_U_TURN_COUNTS 2667u    /* 180 deg pivot on 125 mm track */

typedef enum {
    ROUTE_IDLE = 0,       /* V5 is driving */
    ROUTE_ARC_RIGHT,
    ROUTE_ARC_LEFT,
    ROUTE_SETTLE_BEFORE,  /* stopped, before C/D leg */
    ROUTE_STRAIGHT,
    ROUTE_PIVOT,
    ROUTE_SETTLE_AFTER    /* stopped, before V5 restarts */
} route_phase_t;

static inline bool route_letter_armable(char c)
{
    return c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'Z';
}
static inline route_phase_t route_first_phase(char c)
{
    return c == 'A' ? ROUTE_ARC_RIGHT : (c == 'B' || c == 'Z') ? ROUTE_ARC_LEFT : ROUTE_SETTLE_BEFORE;
}
static inline const char *route_letter_name(char c)
{
    return c == 'A' ? "right" : (c == 'B' || c == 'Z') ? "left" : c == 'C' ? "straight" :
           c == 'D' ? "U-turn" : "none";
}
/* Distance first, time as fallback; disagreement arms nothing. */
static inline char route_choose(char by_distance, char by_time)
{
    if (by_distance && by_time && by_distance != by_time) return '\0';
    return by_distance ? by_distance : by_time;
}

/* ---- Odometry history for distance-based bar widths ------------------- */
/* 20 ms motion loop -> 512 entries = 10.2 s, longer than the big barcode. */
#define ROUTE_ODO_SIZE 512u
typedef struct {
    uint32_t ms[ROUTE_ODO_SIZE];
    uint32_t total[ROUTE_ODO_SIZE]; /* left+right forward counts, wrapping */
    uint16_t head;
    uint16_t count;
} route_odometry_t;

static inline void route_odo_reset(route_odometry_t *o) { o->head = 0; o->count = 0; }
static inline void route_odo_add(route_odometry_t *o, uint32_t ms, uint32_t total)
{
    if (o->count) {
        uint16_t newest = (uint16_t)((o->head + ROUTE_ODO_SIZE - 1u) % ROUTE_ODO_SIZE);
        if ((int32_t)(ms - o->ms[newest]) <= 0) return; /* time must advance */
    }
    o->ms[o->head] = ms;
    o->total[o->head] = total;
    o->head = (uint16_t)((o->head + 1u) % ROUTE_ODO_SIZE);
    if (o->count < ROUTE_ODO_SIZE) ++o->count;
}
/* Forward total at time t, linearly interpolated. A time up to 40 ms after
 * the newest entry uses the newest entry (the last edge is "now"). */
static inline bool route_odo_at(const route_odometry_t *o, uint32_t t, uint32_t *out)
{
    if (!o->count) return false;
    unsigned idx = (o->head + ROUTE_ODO_SIZE - 1u) % ROUTE_ODO_SIZE;
    int32_t after = (int32_t)(t - o->ms[idx]);
    if (after >= 0) {
        if (after > 40) return false;
        *out = o->total[idx];
        return true;
    }
    for (unsigned n = 1; n < o->count; ++n) {
        unsigned older = (idx + ROUTE_ODO_SIZE - 1u) % ROUTE_ODO_SIZE;
        int32_t from_older = (int32_t)(t - o->ms[older]);
        if (from_older >= 0) {
            uint32_t span_ms = o->ms[idx] - o->ms[older];
            int32_t span_counts = (int32_t)(o->total[idx] - o->total[older]);
            *out = o->total[older] + (uint32_t)(int32_t)(((int64_t)span_counts * from_older) / (int64_t)span_ms);
            return true;
        }
        idx = older;
    }
    return false;
}
/* Time widths are 1 ms samples (1 kHz IR acquisition). end_ms is the sample
 * time at which the capture completed: the end of the last element. */
static inline bool route_distance_widths(const route_odometry_t *o, const bc_msg_t *time_msg,
                                         uint32_t end_ms, bc_msg_t *dist)
{
    unsigned n = time_msg->hdr.count;
    if (n == 0u || n > BC_MAX_ELEMS) return false;
    *dist = *time_msg;
    uint32_t edge_ms = end_ms, edge_total;
    if (!route_odo_at(o, edge_ms, &edge_total)) return false;
    for (unsigned k = n; k-- > 0;) {
        uint32_t start_ms = edge_ms - time_msg->widths[k];
        uint32_t start_total;
        if (!route_odo_at(o, start_ms, &start_total)) return false;
        int32_t d = (int32_t)(edge_total - start_total);
        if (d < 1) d = 1;
        if ((uint32_t)d >= BC_WIDTH_SAT) d = (int32_t)BC_WIDTH_SAT - 1;
        dist->widths[k] = (bc_width_t)d;
        edge_ms = start_ms;
        edge_total = start_total;
    }
    return true;
}

/* ---- Branch exit (Ken's rule, unchanged) -------------------------------- */
/* left/right are forward counts since the arc began; a/b the calibrated
 * 0..100 line levels (a = left sensor). True when this sample counts as a
 * sighting of the outgoing branch. */
static inline bool route_exit_sighting(bool right_turn, int32_t left, int32_t right,
                                       unsigned a, unsigned b, bool both_black)
{
    int64_t lead = right_turn ? (int64_t)left - right : (int64_t)right - left;
    bool right_sighting = b >= ROUTE_EXIT_MIN_DARKNESS && (int)b - (int)a >= ROUTE_EXIT_MIN_CONTRAST;
    bool left_sighting = a >= ROUTE_EXIT_MIN_DARKNESS && (int)a - (int)b >= ROUTE_EXIT_MIN_CONTRAST;
    bool turn_side = right_turn ? right_sighting : left_sighting;
    bool either = lead >= ROUTE_ARC_EITHER_SIDE_LEAD && (left_sighting || right_sighting);
    return !both_black && lead >= ROUTE_ARC_MIN_LEAD && (turn_side || either);
}
#endif

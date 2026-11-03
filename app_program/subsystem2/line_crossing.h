#ifndef MOTION_LINE_CROSSING_H
#define MOTION_LINE_CROSSING_H
#include <stdint.h>
#include <stdbool.h>

/* Explicit GP20 trial policy: try straight, with finite travel and time.
 * Counts are provisional, not calibrated millimetres or branch geometry. */
typedef struct {
    uint32_t mark_ms, mark_counts, total_ms, total_counts, clear_ms;
    int32_t speed, heading_gain, heading_limit;
} line_crossing_config_t;
static const line_crossing_config_t line_crossing_config={
    .mark_ms=1800,.mark_counts=400,.total_ms=3500,.total_counts=900,
    .clear_ms=60,.speed=300,.heading_gain=2,.heading_limit=50
};
typedef enum {
    LINE_STOP_NONE, LINE_STOP_RECOVERY_TIME, LINE_STOP_RECOVERY_DISTANCE,
    LINE_STOP_SWEEP_COMPLETE, LINE_STOP_MARK_TIME, LINE_STOP_MARK_DISTANCE,
    LINE_STOP_CROSSING_TIME, LINE_STOP_CROSSING_DISTANCE,
    LINE_STOP_ROUTE_COMMAND, LINE_STOP_INVALID_INPUT
} line_stop_reason_t;
typedef enum { CROSS_NONE, CROSS_ADVANCE, CROSS_REACQUIRE, CROSS_FAILED } line_crossing_result_t;
typedef struct {
    bool active, clear_pending;
    uint32_t started_ms, clear_since;
    int32_t origin[2];
    line_stop_reason_t reason;
} line_crossing_t;
static inline line_crossing_result_t line_crossing_step(line_crossing_t *s,
        bool black,bool visible,uint32_t now,int32_t l,int32_t r,int32_t *lt,int32_t *rt)
{
    const line_crossing_config_t *c=&line_crossing_config;
    if(s->reason)return CROSS_FAILED;
    if(black && !s->active) {
        s->active=true;s->started_ms=now;s->origin[0]=l;s->origin[1]=r;
    }
    if(!s->active)return CROSS_NONE;
    int32_t dl=l-s->origin[0],dr=r-s->origin[1];
    int32_t travel=dl>dr ? dl:dr;
    uint32_t elapsed=now-s->started_ms;
    /* Bounds precede sightings; neither a flicker nor a late sample renews
     * the allowance. A whole new encounter requires confirmed clear line. */
    if(elapsed>=c->total_ms)s->reason=LINE_STOP_CROSSING_TIME;
    else if(travel>=(int32_t)c->total_counts)s->reason=LINE_STOP_CROSSING_DISTANCE;
    else if(black && elapsed>=c->mark_ms)s->reason=LINE_STOP_MARK_TIME;
    else if(black && travel>=(int32_t)c->mark_counts)s->reason=LINE_STOP_MARK_DISTANCE;
    if(s->reason)return CROSS_FAILED;
    if(!black && visible) {
        if(!s->clear_pending){s->clear_pending=true;s->clear_since=now;}
        if(now-s->clear_since>=c->clear_ms) {
            *s=(line_crossing_t){0};return CROSS_NONE;
        }
    } else s->clear_pending=false;
    if(!black)return CROSS_REACQUIRE;
    /* Match wheel travel to hold heading across the unreadable mark. This
     * preserves entry orientation; it cannot infer the intended track angle. */
    int32_t correction=(dl-dr)*c->heading_gain;
    if(correction>c->heading_limit)correction=c->heading_limit;
    if(correction<-c->heading_limit)correction=-c->heading_limit;
    *lt=c->speed-correction;*rt=c->speed+correction;
    return CROSS_ADVANCE;
}
#endif

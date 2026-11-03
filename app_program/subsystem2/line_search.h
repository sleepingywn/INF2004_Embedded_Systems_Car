#ifndef MOTION_LINE_SEARCH_H
#define MOTION_LINE_SEARCH_H
#include <stdint.h>
#include <stdbool.h>
#define LINE_SEARCH_ZERO_MS 600u
#define LINE_SEARCH_CURVE_ZERO_MS 300u
#define LINE_SEARCH_CURVE_RECENT_MS 200u
#define LINE_SEARCH_CURVE_ERROR 30
#define LINE_SEARCH_HOLD_OUTER_CPS 500
#define LINE_SEARCH_GRACE_CPS 350
#define LINE_SEARCH_PAUSE_RELEASE_MS 100u
#define LINE_SEARCH_FIRST_MS 500u
#define LINE_SEARCH_MAX_MS 1500u
#define LINE_SEARCH_MAX_COUNTS 1000
#define LINE_SEARCH_CONFIRM_MS 60u
#define LINE_SEARCH_SEEN_ERROR 20
#define LINE_SEARCH_INNER_CPS 250
#define LINE_SEARCH_OUTER_CPS 550
typedef enum { LINE_SEARCH_TRACK,LINE_SEARCH_SWEEP,LINE_SEARCH_TIMEOUT,LINE_SEARCH_RANGE } line_search_result_t;
typedef struct {
    uint32_t zero_ms,started_ms,confirm_ms,curve_ms;
    int32_t origin_left,origin_right;
    int8_t last_direction,first_direction,confirm_direction,curve_direction;
    bool zero_pending,active,confirming,curve_grace;
    uint8_t phase;
} line_search_t;
/* Validated observations only. Synthetic steering cannot renew the budget.
 * Confirmation is same-side real input; white or opposite noise restarts the
 * confirmation dwell, never the active search deadline or travel origins. */
static inline line_search_result_t line_search_step(line_search_t *s,int error,bool reported_lost,
        bool tight_active,uint32_t now,int32_t left,int32_t right,int32_t *lt,int32_t *rt)
{
    (void)tight_active; /* tight-turn release is handled by the motor owner */
    int magnitude=error<0 ? -error:error;
    int direction=error>0 ? 1:error<0 ? -1:0;
    bool visible=!reported_lost && magnitude>=LINE_SEARCH_SEEN_ERROR;
    /* Only actual evidence records a bend. Even weak opposite evidence
     * cancels the stored correction; zero/synthetic turns never renew it. */
    if(!reported_lost && direction) {
        if(direction!=s->curve_direction)s->curve_direction=0;
        if(magnitude>=LINE_SEARCH_CURVE_ERROR) {
            s->curve_direction=(int8_t)direction;s->curve_ms=now;
        }
    }
    if(!s->active) {
        if(visible){s->last_direction=(int8_t)direction;s->zero_pending=false;return LINE_SEARCH_TRACK;}
        if(!s->zero_pending){
            s->zero_pending=true;s->zero_ms=now;
            s->curve_grace=s->curve_direction && now-s->curve_ms<=LINE_SEARCH_CURVE_RECENT_MS;
        }
        /* Centered OUTSIDE placement can already report LOST at admission.
         * Give it the same bounded slow-forward grace, never an unlimited pass. */
        uint32_t grace=s->curve_grace ? LINE_SEARCH_CURVE_ZERO_MS:LINE_SEARCH_ZERO_MS;
        if(now-s->zero_ms<grace)return LINE_SEARCH_TRACK;
        s->active=true;s->started_ms=now;s->origin_left=left;s->origin_right=right;
        s->first_direction=s->last_direction ? s->last_direction:1;
        s->confirming=false;
    }
    uint32_t elapsed=now-s->started_ms;
    if(elapsed>=LINE_SEARCH_MAX_MS)return LINE_SEARCH_TIMEOUT;
    if((int64_t)left-s->origin_left>=LINE_SEARCH_MAX_COUNTS ||
       (int64_t)right-s->origin_right>=LINE_SEARCH_MAX_COUNTS)return LINE_SEARCH_RANGE;
    s->phase=elapsed<LINE_SEARCH_FIRST_MS ? 1u:2u;
    if(visible) {
        if(!s->confirming || s->confirm_direction!=direction) {
            s->confirming=true;s->confirm_direction=(int8_t)direction;s->confirm_ms=now;
        }
        if(now-s->confirm_ms>=LINE_SEARCH_CONFIRM_MS) {
            s->active=false;s->phase=0;s->zero_pending=false;s->confirming=false;
            s->last_direction=(int8_t)direction;return LINE_SEARCH_TRACK;
        }
    } else s->confirming=false;
    int steer=visible ? direction:s->phase==1 ? s->first_direction:-s->first_direction;
    *lt=steer>0 ? LINE_SEARCH_INNER_CPS:LINE_SEARCH_OUTER_CPS;
    *rt=steer<0 ? LINE_SEARCH_INNER_CPS:LINE_SEARCH_OUTER_CPS;
    return LINE_SEARCH_SWEEP;
}
#endif

#ifndef MOTION_LINE_TIGHT_TURN_H
#define MOTION_LINE_TIGHT_TURN_H
#include <stdbool.h>
#include <stdint.h>

#define LINE_TIGHT_ENTER_ERROR (MOTION_LINE_SEARCH ? 55:85)
#define LINE_TIGHT_PENDING_ERROR (MOTION_LINE_SEARCH ? 45:85)
#define LINE_TIGHT_EXIT_ERROR 45
#define LINE_TIGHT_ENTER_MS 120u
#define LINE_TIGHT_EXIT_MS 100u
#define LINE_TIGHT_MAX_MS 800u
#define LINE_TIGHT_UNSEEN_MS 200u
#define LINE_TIGHT_COOLDOWN_MS 300u
#define LINE_TIGHT_OUTER_CPS 500
typedef enum { LINE_TIGHT_OFF,LINE_TIGHT_ACTIVE,LINE_TIGHT_TIMEOUT,LINE_TIGHT_UNSEEN } line_tight_result_t;
typedef struct {
    uint32_t pending_ms,started_ms,settled_ms,unseen_ms,exited_ms;
    int8_t direction,pending_direction;
    bool settling,unseen,cooldown;
} line_tight_turn_t;
static inline void line_tight_exit(line_tight_turn_t *t,uint32_t now)
{
    t->direction=t->pending_direction=0;
    t->settling=t->unseen=false;t->cooldown=true;t->exited_ms=now;
}
/* Called only after freshness/epoch/state/encoder guards, with actual IR error.
 * Zero is ambiguous, not proof of reacquisition. Opposite evidence wins.
 * All intervals use subtraction so timer wrap does not change the limits. */
static inline line_tight_result_t line_tight_step(line_tight_turn_t *t,int error,uint32_t now)
{
    int magnitude=error<0 ? -error:error;
    int direction=error>0 ? 1:error<0 ? -1:0;
    if(t->direction) {
        if(now-t->started_ms>=LINE_TIGHT_MAX_MS)return LINE_TIGHT_TIMEOUT;
        if(direction && direction!=t->direction) {
            line_tight_exit(t,now);return LINE_TIGHT_OFF;
        }
        if(!direction) {
            if(!t->unseen){t->unseen=true;t->unseen_ms=now;}
            if(now-t->unseen_ms>=LINE_TIGHT_UNSEEN_MS)return LINE_TIGHT_UNSEEN;
        } else t->unseen=false;
        if(direction && magnitude<=LINE_TIGHT_EXIT_ERROR) {
            if(!t->settling){t->settling=true;t->settled_ms=now;}
            if(now-t->settled_ms>=LINE_TIGHT_EXIT_MS) {
                line_tight_exit(t,now);return LINE_TIGHT_OFF;
            }
        } else t->settling=false;
        return LINE_TIGHT_ACTIVE;
    }
    if(t->cooldown && now-t->exited_ms<LINE_TIGHT_COOLDOWN_MS)return LINE_TIGHT_OFF;
    t->cooldown=false;
    /* The loaded curve produced persistent errors 45..76 without reaching
     * the old 85 trigger. Arm at 55 in the search trial, then tolerate a
     * same-side dip to 45 during confirmation. Brief peaks, weaker readings
     * and opposite signs cannot accumulate confirmation across separate bends. */
    int threshold=t->pending_direction==direction ? LINE_TIGHT_PENDING_ERROR:LINE_TIGHT_ENTER_ERROR;
    if(magnitude<threshold){t->pending_direction=0;return LINE_TIGHT_OFF;}
    if(t->pending_direction!=direction) {
        t->pending_direction=(int8_t)direction;t->pending_ms=now;
    } else if(now-t->pending_ms>=LINE_TIGHT_ENTER_MS) {
        t->direction=(int8_t)direction;t->pending_direction=0;t->started_ms=now;
        t->settling=t->unseen=false;return LINE_TIGHT_ACTIVE;
    }
    return LINE_TIGHT_OFF;
}
#endif

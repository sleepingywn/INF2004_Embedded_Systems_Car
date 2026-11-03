#ifndef MOTION_LINE_OVERLAP_H
#define MOTION_LINE_OVERLAP_H
#include <stdbool.h>
#include <stdint.h>

/* Short confirmation of a both-black candidate during an established curve.
 * Count-domain travel bound, not calibrated distance. No junction bypass. */
#define LINE_OVERLAP_MAX_MS 120u
#define LINE_OVERLAP_MAX_COUNTS 80
#define LINE_OVERLAP_CLEAR_MS 60u
#define LINE_OVERLAP_RECENT_MS 200u
#define LINE_OVERLAP_MAX_CPS 350
typedef enum { LINE_OVERLAP_NONE,LINE_OVERLAP_HOLD,LINE_OVERLAP_UNSAFE,
               LINE_OVERLAP_TIMEOUT,LINE_OVERLAP_RANGE } line_overlap_result_t;
typedef struct {
    bool active,clearing;
    uint32_t started_ms,clear_ms;
    int32_t origin_left,origin_right,target_left,target_right;
} line_overlap_t;
static inline int32_t line_overlap_target(int32_t previous)
{
    if(!previous)return 0; /* preserve an intentional inside-wheel pause */
    return previous>LINE_OVERLAP_MAX_CPS ? LINE_OVERLAP_MAX_CPS:previous;
}
static inline line_overlap_result_t line_overlap_step(line_overlap_t *o,
        bool both_black,bool may_begin,bool directional,uint32_t now,
        int32_t left,int32_t right,int32_t previous_left,int32_t previous_right,
        int32_t *lt,int32_t *rt)
{
    if(!both_black) {
        /* Only sustained, actual directional evidence grants another budget.
         * Both-white and short flicker cannot disguise a persistent candidate. */
        if(o->active && directional) {
            if(!o->clearing){o->clearing=true;o->clear_ms=now;}
            if(now-o->clear_ms>=LINE_OVERLAP_CLEAR_MS)*o=(line_overlap_t){0};
        } else o->clearing=false;
        return LINE_OVERLAP_NONE;
    }
    if(!o->active) {
        if(!may_begin || (!previous_left && !previous_right))return LINE_OVERLAP_UNSAFE;
        *o=(line_overlap_t){.active=true,.started_ms=now,
            .origin_left=left,.origin_right=right,
            .target_left=line_overlap_target(previous_left),
            .target_right=line_overlap_target(previous_right)};
    }
    o->clearing=false;
    if(now-o->started_ms>=LINE_OVERLAP_MAX_MS)return LINE_OVERLAP_TIMEOUT;
    if((int64_t)left-o->origin_left>=LINE_OVERLAP_MAX_COUNTS ||
       (int64_t)right-o->origin_right>=LINE_OVERLAP_MAX_COUNTS)return LINE_OVERLAP_RANGE;
    *lt=o->target_left;*rt=o->target_right;
    return LINE_OVERLAP_HOLD;
}
#endif

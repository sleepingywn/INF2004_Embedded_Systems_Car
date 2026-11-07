#ifndef MOTION_LINE_INDICATOR_H
#define MOTION_LINE_INDICATOR_H
#include "motion.h"

typedef enum {
    LINE_LED_OFF, LINE_LED_ARMED, LINE_LED_PENDING, LINE_LED_RUNNING, LINE_LED_ERROR
} line_led_mode_t;
typedef struct {
    uint32_t epoch,id,queued_ms;
    bool tracking,failed;
    line_led_mode_t mode;
} line_indicator_t;
static inline void line_indicator_clear(line_indicator_t *s)
{
    *s=(line_indicator_t){0};
}
static inline void line_indicator_queue(line_indicator_t *s,uint32_t epoch,uint32_t id,uint32_t now)
{
    *s=(line_indicator_t){.epoch=epoch,.id=id,.queued_ms=now,.tracking=true,.mode=LINE_LED_PENDING};
}
static inline void line_indicator_fail(line_indicator_t *s)
{
    s->tracking=false;s->failed=true;s->mode=LINE_LED_ERROR;
}
static inline void line_indicator_observe(line_indicator_t *s,const motion_status_t *m,
                                          bool armed,uint32_t now)
{
    if(s->failed){s->mode=LINE_LED_ERROR;return;}
    if(!m || (uint32_t)(now-m->updated_ms)>200u) {
        s->mode=(armed || s->tracking) ? LINE_LED_ERROR : LINE_LED_OFF;
        return; /* transient busy/stale status never claims a running motor */
    }
    if(m->faults && (armed || s->tracking)){line_indicator_fail(s);return;}
    if(!s->tracking){s->mode=armed ? LINE_LED_ARMED : LINE_LED_OFF;return;}
    if(m->epoch!=s->epoch){line_indicator_fail(s);return;}
    if(m->request_epoch==s->epoch && m->request_id==s->id) {
        if(m->state==MOTION_TRACING && m->result==MOTION_ACCEPTED) {
            s->mode=LINE_LED_RUNNING;return;
        }
        if(m->state==MOTION_IDLE && m->result==MOTION_COMPLETED) {
            line_indicator_clear(s);return;
        }
        if(m->result==MOTION_REJECTED || m->result==MOTION_FAILED ||
           m->result==MOTION_CANCELLED || m->result==MOTION_EXPIRED) {
            line_indicator_fail(s);return;
        }
    }
    /* Enqueueing alone is not success. Keep observing even after an unusually
     * delayed acknowledgement; the indicator never submits or stops motion. */
    s->mode=(uint32_t)(now-s->queued_ms)>=1000u ? LINE_LED_ERROR : LINE_LED_PENDING;
}
static inline bool line_indicator_level(line_led_mode_t mode,uint32_t now)
{
    switch(mode) {
    case LINE_LED_ARMED: return now%1000u<500u;
    case LINE_LED_PENDING: return now%1000u<100u || (now%1000u>=200u && now%1000u<300u);
    case LINE_LED_RUNNING: return true;
    case LINE_LED_ERROR: return now%200u<100u;
    default: return false;
    }
}
#endif

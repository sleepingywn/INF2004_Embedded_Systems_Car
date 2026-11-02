#ifndef MOTION_LINE_FOLLOW_H
#define MOTION_LINE_FOLLOW_H
#include "forward_speed.h"
#include "motion_logic.h"
#include "motion_config.h"

/* Bounded, forward-only local line trial; no metric odometry or barcode turns. */
#define LINE_FOLLOW_MAX_MS (MOTION_LINE_CIRCUIT ? 30000u : 2000u)
#define LINE_FOLLOW_INPUT_AGE_MS 100u
#define LINE_FOLLOW_STALL_MS 200u
#define LINE_FOLLOW_COUNT_LIMIT (2800u * (LINE_FOLLOW_MAX_MS / 1000u))
_Static_assert(LINE_FOLLOW_MAX_MS <= 30000u, "Fits motion request TTL");
typedef enum {
    LINE_STEP_WAIT, LINE_STEP_OK, LINE_STEP_DONE, LINE_STEP_IR,
    LINE_STEP_ENCODER, LINE_STEP_TIME, LINE_STEP_STALL, LINE_STEP_LIMIT,
    LINE_STEP_DISCONNECTED
} line_step_t;
typedef struct {
    speed_control_t speed;
    uint32_t origin_left,origin_right,invalid,previous_ms,started_ms;
    uint32_t last_progress_ms[2];
    int32_t previous_total[2];
} line_follow_t;

static inline bool line_follow_usable(const line_msg_t *line,uint32_t now,
                                      uint32_t sampled_ms,uint32_t epoch,
                                      uint32_t line_epoch,bool have_line)
{
    return have_line && line && epoch==line_epoch &&
           (uint32_t)(now-sampled_ms)<=LINE_FOLLOW_INPUT_AGE_MS && !line->fault &&
           (line->state==LINE_ON || line->state==LINE_EDGE_ONLY) &&
           line->err>=-100 && line->err<=100;
}
static inline bool line_follow_targets(int error,int32_t *left,int32_t *right)
{
    if(error < -100 || error > 100 || !left || !right)return false;
    /* Buddy 3: positive error means line to LEFT. Slow the left wheel.
     * Never speed the outer wheel above the tested target or demand reverse.
     * A full-scale error stops the inner wheel. No straight-travel correction
     * runs here: unequal wheel travel is necessary to steer along the line. */
    *left=TARGET_CPS-(error>0 ? error*TARGET_CPS/100 : 0);
    *right=TARGET_CPS+(error<0 ? error*TARGET_CPS/100 : 0);
    return true;
}
static inline void line_follow_begin(line_follow_t *f,uint32_t left,uint32_t right,
                                    uint32_t invalid,uint32_t sampled_ms,uint32_t now)
{
    *f=(line_follow_t){0};
    speed_control_start(&f->speed);
    /* Steering may require less than 14% or a stopped inner wheel. */
    f->speed.left.minimum=f->speed.right.minimum=0;
    f->origin_left=left;f->origin_right=right;f->invalid=invalid;
    f->previous_ms=sampled_ms;f->started_ms=now;
    f->last_progress_ms[0]=f->last_progress_ms[1]=now;
}
static inline line_step_t line_follow_step(line_follow_t *f,uint32_t raw_left,
        uint32_t raw_right,uint32_t invalid,uint32_t sampled_ms,uint32_t now,
        const line_msg_t *line,uint32_t line_ms,uint32_t epoch,uint32_t line_epoch,
        bool have_line,bool connected)
{
    if(!connected)return LINE_STEP_DISCONNECTED;
    if(!line_follow_usable(line,now,line_ms,epoch,line_epoch,have_line))return LINE_STEP_IR;
    if((uint32_t)(now-f->started_ms)>=LINE_FOLLOW_MAX_MS)return LINE_STEP_DONE;
    if((uint32_t)(now-sampled_ms)>LINE_FOLLOW_INPUT_AGE_MS)return LINE_STEP_TIME;
    if(invalid!=f->invalid)return LINE_STEP_ENCODER;
    uint32_t dt=sampled_ms-f->previous_ms;
    if(dt>CONTROL_MAX_US/1000u)return LINE_STEP_TIME;
    if(dt<CONTROL_MIN_US/1000u)return LINE_STEP_WAIT;
    /* Verified forward signs: left raw negative, right raw positive. */
    int64_t l=-(int64_t)motion_count_delta(raw_left,f->origin_left);
    int64_t r=motion_count_delta(raw_right,f->origin_right);
    if(l<0 || r<0 || l<f->previous_total[0] || r<f->previous_total[1])return LINE_STEP_ENCODER;
    if(l>=LINE_FOLLOW_COUNT_LIMIT || r>=LINE_FOLLOW_COUNT_LIMIT)return LINE_STEP_LIMIT;
    int32_t lt,rt;
    if(!line_follow_targets(line->err,&lt,&rt))return LINE_STEP_IR;
    if(l>f->previous_total[0] || lt==0 || f->speed.left.target_cps==0)f->last_progress_ms[0]=now;
    if(r>f->previous_total[1] || rt==0 || f->speed.right.target_cps==0)f->last_progress_ms[1]=now;
    if((lt && (uint32_t)(now-f->last_progress_ms[0])>=LINE_FOLLOW_STALL_MS) ||
       (rt && (uint32_t)(now-f->last_progress_ms[1])>=LINE_FOLLOW_STALL_MS))return LINE_STEP_STALL;
    if(!speed_control_targets_step(&f->speed,(int32_t)l,(int32_t)r,dt*1000u,lt,rt))return LINE_STEP_ENCODER;
    f->previous_total[0]=(int32_t)l;f->previous_total[1]=(int32_t)r;
    f->previous_ms=sampled_ms;
    return LINE_STEP_OK;
}
#endif

#ifndef MOTION_FORWARD_SPEED_H
#define MOTION_FORWARD_SPEED_H
#include <stdbool.h>
#include <stdint.h>

/* Bench-only provisional video calibration. Speed units are counts/second
 * normalised to REFERENCE_COUNTS_REV, not calibrated mm/s. No reverse output.
 * Gains are conservative initial settings requiring physical validation.
 */
#define LEFT_COUNTS_REV 2550
#define RIGHT_COUNTS_REV 2550
#define REFERENCE_COUNTS_REV 2550
#define LEFT_FEEDFORWARD 155u /* provisional bias from two floor traces */
#define RIGHT_FEEDFORWARD 185u
#define START_DUTY 200u
#define MAX_DUTY 200u
#define MIN_DUTY 140u
#define TARGET_CPS 1200
#define POSITION_GAIN 2
#define POSITION_TRIM_MAX 120
#define KP_MILLI 30 /* milli-permille per count/second: 0.03 permille/(count/s) */
#define KI_MILLI 240 /* milli-permille per (count/second)*second */
#define INTEGRAL_LIMIT_MILLI 40000
#define FILTER_TAU_US 40000u
#define DUTY_SLEW_PER_SECOND 250u /* 5 permille per 20 ms */
#define CONTROL_MIN_US 20000u
#define CONTROL_MAX_US 60000u
#define MAX_MEASURED_CPS 10000

enum { SPEED_LIMIT_LOW=1u, SPEED_LIMIT_HIGH=2u, SPEED_LIMIT_SLEW=4u };
typedef struct {
    int32_t filtered_milli_cps;
    int32_t integral_milli;
    int32_t target_cps;
    unsigned duty,limits,feedforward,minimum;
} wheel_speed_t;
typedef struct {
    wheel_speed_t left,right;
    int32_t previous_left,previous_right;
    bool valid;
} speed_control_t;

static inline void speed_control_reset(speed_control_t *c)
{
    *c=(speed_control_t){0};
}
static inline void speed_control_start(speed_control_t *c)
{
    speed_control_reset(c);
    c->left.duty=c->right.duty=START_DUTY;
    c->left.minimum=c->right.minimum=MIN_DUTY;
    c->left.feedforward=LEFT_FEEDFORWARD;
    c->right.feedforward=RIGHT_FEEDFORWARD;
    c->left.target_cps=c->right.target_cps=TARGET_CPS;
}
static inline int32_t clamp_i64(int64_t x,int32_t lo,int32_t hi)
{
    if(x<lo)return lo;
    if(x>hi)return hi;
    return (int32_t)x;
}
static inline void speed_wheel_step(wheel_speed_t *w,int32_t measured_cps,
                             int32_t target_cps,uint32_t dt_us)
{
    /* Actual elapsed time, first-order low-pass; no assumed 20 ms interval. */
    int64_t change=(int64_t)measured_cps*1000-w->filtered_milli_cps;
    w->filtered_milli_cps+=(int32_t)(change*dt_us/(FILTER_TAU_US+dt_us));
    if(target_cps==0) {
        w->target_cps=0;w->integral_milli=0;w->duty=0;w->limits=0;
        return;
    }
    /* Restart a commanded-stopped wheel at its bounded feed-forward value;
     * a slow ramp from 0 can remain below breakaway torque until stall timeout. */
    if(w->target_cps==0 && w->duty==0)w->duty=w->feedforward;
    w->target_cps=target_cps;
    int32_t error=target_cps-w->filtered_milli_cps/1000;
    int32_t candidate=clamp_i64((int64_t)w->integral_milli+
                              (int64_t)KI_MILLI*error*dt_us/1000000,
                              -INTEGRAL_LIMIT_MILLI,INTEGRAL_LIMIT_MILLI);
    int64_t request=(int64_t)w->feedforward*1000+(int64_t)KP_MILLI*error+candidate;
    unsigned slew=DUTY_SLEW_PER_SECOND*dt_us/1000000u;
    int32_t lo=(int32_t)w->duty-(int32_t)slew;
    int32_t hi=(int32_t)w->duty+(int32_t)slew;
    if(lo<(int32_t)w->minimum)lo=w->minimum;
    if(hi>(int32_t)MAX_DUTY)hi=MAX_DUTY;
    /* Conditional integration includes both PWM saturation and slew limits.
     * Permit integration that unwinds saturation, freeze accumulation into it.
     */
    if(!((request>(int64_t)hi*1000 && error>0) ||
         (request<(int64_t)lo*1000 && error<0))) w->integral_milli=candidate;
    request=(int64_t)w->feedforward*1000+(int64_t)KP_MILLI*error+w->integral_milli;
    w->limits=0;
    if(request<(int64_t)w->minimum*1000)w->limits|=SPEED_LIMIT_LOW;
    if(request>(int64_t)MAX_DUTY*1000)w->limits|=SPEED_LIMIT_HIGH;
    if(request<(int64_t)lo*1000 || request>(int64_t)hi*1000)w->limits|=SPEED_LIMIT_SLEW;
    w->duty=(unsigned)clamp_i64(request/1000,lo,hi);
}
static inline bool speed_control_targets_step(speed_control_t *c,int32_t left,int32_t right,uint32_t dt_us,
                                       int32_t lt,int32_t rt)
{
    /* Validate both inputs before updating either controller. Fault => caller
     * stops both motors. Raw counter wrap is handled by motion_count_delta.
     */
    if(dt_us<CONTROL_MIN_US || dt_us>CONTROL_MAX_US || lt<0 || rt<0 ||
       lt>TARGET_CPS || rt>TARGET_CPS)return false;
    int64_t dl=(int64_t)left-c->previous_left,dr=(int64_t)right-c->previous_right;
    if(dl<0 || dr<0)return false;
    /* Reject implausible deltas before multiplying; keep conversion bounded
     * even when a corrupted caller supplies extreme signed totals. */
    int64_t lmax=(int64_t)MAX_MEASURED_CPS*dt_us*LEFT_COUNTS_REV/
                 ((int64_t)1000000*REFERENCE_COUNTS_REV);
    int64_t rmax=(int64_t)MAX_MEASURED_CPS*dt_us*RIGHT_COUNTS_REV/
                 ((int64_t)1000000*REFERENCE_COUNTS_REV);
    if(dl>lmax || dr>rmax)return false;
    int64_t ls=dl*1000000*REFERENCE_COUNTS_REV/((int64_t)dt_us*LEFT_COUNTS_REV);
    int64_t rs=dr*1000000*REFERENCE_COUNTS_REV/((int64_t)dt_us*RIGHT_COUNTS_REV);
    if(ls>MAX_MEASURED_CPS || rs>MAX_MEASURED_CPS)return false;
    speed_wheel_step(&c->left,(int32_t)ls,lt,dt_us);
    speed_wheel_step(&c->right,(int32_t)rs,rt,dt_us);
    c->previous_left=left;c->previous_right=right;c->valid=true;
    return true;
}
/* Standalone straight-bench adapter. Line following supplies different
 * targets directly and MUST NOT equalise wheel travel while steering. */
static inline bool speed_control_step(speed_control_t *c,int32_t left,int32_t right,uint32_t dt_us)
{
    int64_t lead=(int64_t)left*REFERENCE_COUNTS_REV/LEFT_COUNTS_REV-
                 (int64_t)right*REFERENCE_COUNTS_REV/RIGHT_COUNTS_REV;
    int32_t trim=clamp_i64(lead*POSITION_GAIN,-POSITION_TRIM_MAX,POSITION_TRIM_MAX);
    int32_t lt=TARGET_CPS-(trim>0?trim:0),rt=TARGET_CPS+(trim<0?trim:0);
    return speed_control_targets_step(c,left,right,dt_us,lt,rt);
}
#endif

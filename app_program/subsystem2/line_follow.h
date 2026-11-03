#ifndef MOTION_LINE_FOLLOW_H
#define MOTION_LINE_FOLLOW_H
#include "forward_speed.h"
#include "motion_logic.h"
#include "motion_config.h"
#include "line_tight_turn.h"
#include "line_search.h"
#include "line_overlap.h"
#include "line_observation.h"
#include "line_adaptive.h"
#define LINE_FEEDBACK_PROFILE (MOTION_LINE_SEARCH || MOTION_LINE_ADAPTIVE)
#if (MOTION_LINE_SEARCH || MOTION_LINE_ADAPTIVE) && LINE_MODE != LINE_MODE_OUTSIDE
#error "Bounded ambiguous-line search is only valid for OUTSIDE placement"
#endif

/* Bounded, forward-only local line trial; no metric odometry or barcode turns. */
#define LINE_FOLLOW_MAX_MS (MOTION_LINE_CIRCUIT ? 30000u : 2000u)
#define LINE_FOLLOW_INPUT_AGE_MS 100u
#define LINE_FOLLOW_STALL_MS 200u
#define LINE_FOLLOW_COUNT_LIMIT (2800u * (LINE_FOLLOW_MAX_MS / 1000u))
/* Provisional curve profiles after loaded track observations.
 * The 150-count/s inner target stalled; retain a minimum rolling target 250.
 * The search trial uses 450 cruise, 250/600 full curve by error 60;
 * older non-search profiles retain 700 cruise and 250/750 full curve.
 * The outer wheel may exceed cruise, but mean wheel speed decreases through
 * the turn. Count-domain settings, not a calibrated radius or guarantee. */
#define LINE_CRUISE_CPS (LINE_FEEDBACK_PROFILE ? 450:700)
#define LINE_CURVE_CPS (LINE_FEEDBACK_PROFILE ? 600:750)
#define LINE_INNER_MIN_CPS 250
#define LINE_FULL_TURN_ERROR 60
#define LINE_CURVE_ENTER_ERROR 30
#define LINE_CURVE_EXIT_HOLD_MS 1000u
#define LINE_CURVE_EXIT_CPS 400
#define LINE_STEERING_HOLD_MS 500u
#define LINE_STEERING_HOLD_ERROR 20
#define LINE_SPEED_KP_MILLI (LINE_FEEDBACK_PROFILE ? 120u:60u)
#define LINE_SPEED_KI_MILLI 480u
#define LINE_TARGET_SLEW_CPS_PER_SECOND (LINE_FEEDBACK_PROFILE ? 2000u:4000u)
#define LINE_TARGET_DECEL_CPS_PER_SECOND 4000u
#define LINE_DECEL_PER_SECOND (LINE_FEEDBACK_PROFILE ? 1000u:500u)
/* V5 adaptive trial: gross backward counts, only during initial settling.
 * Provisional count/time bounds, not a diagnosis of mechanical vs electrical
 * movement. Legacy profiles retain their previous one-count policy. */
#define LINE_PAUSE_REVERSE_BUDGET (MOTION_LINE_ADAPTIVE ? 2u:1u)
#define LINE_PAUSE_SETTLE_MS 120u
#define LINE_PAUSE_REARM_MS 60u
#define LINE_PAUSE_REARM_COUNTS 3u
_Static_assert(LINE_FOLLOW_MAX_MS <= 30000u, "Fits motion request TTL");
typedef enum {
    LINE_STEP_WAIT, LINE_STEP_OK, LINE_STEP_DONE, LINE_STEP_IR,
    LINE_STEP_ENCODER, LINE_STEP_TIME, LINE_STEP_STALL, LINE_STEP_LIMIT,
    LINE_STEP_DISCONNECTED,LINE_STEP_TIGHT_TIMEOUT,LINE_STEP_TIGHT_UNSEEN,
    LINE_STEP_SEARCH_TIMEOUT,LINE_STEP_SEARCH_RANGE,
    LINE_STEP_OVERLAP_TIMEOUT,LINE_STEP_OVERLAP_RANGE,
    LINE_STEP_ROUTE_REQUIRED,LINE_STEP_RECOVERY_EXHAUSTED,LINE_STEP_CROSSING_EXHAUSTED
} line_step_t;
/* First rejected layer; multiple bits may identify both wheels. Speed-layer
 * detail uses SPEED_REJECT_* when LINE_ENCODER_SPEED is set. */
enum { LINE_ENCODER_RAW_MISSING=1u,LINE_ENCODER_INVALID_EDGES=2u,
       LINE_ENCODER_LEFT_NEGATIVE=4u,LINE_ENCODER_RIGHT_NEGATIVE=8u,
       LINE_ENCODER_LEFT_RECEDING=16u,LINE_ENCODER_RIGHT_RECEDING=32u,
       LINE_ENCODER_SPEED=64u };
typedef struct {
    speed_control_t speed;
    uint32_t origin_left,origin_right,invalid,previous_ms,started_ms;
    uint32_t last_progress_ms[2];
    int32_t previous_total[2];
    uint32_t last_curve_ms;
    bool curve_seen;
    uint32_t last_steering_ms;
    int8_t curve_direction,steering_error;
    line_tight_turn_t tight;
    line_search_t search;
    line_overlap_t overlap;
    line_adaptive_t adaptive;
    uint16_t observed_levels;
    uint8_t encoder_reject,speed_reject;
    uint8_t pause_reverse_used[2];
    uint32_t output_pause_ms[2],last_settle_ms[2]; /* diagnostic only */
    uint32_t pause_window_ms[2];
    bool pause_window_active[2];
    uint32_t pause_forward_ms[2],pause_forward_counts[2];
    unsigned pre_pause_duty[2];
    int32_t rejected_target_left,rejected_target_right;
} line_follow_t;

/* Eligibility for the PREVIOUS output interval. Used by both the guard and
 * its frozen diagnostic; requested new targets do not grant tolerance. */
static inline unsigned line_pause_eligible(const line_follow_t *f)
{
    bool left=(MOTION_LINE_ADAPTIVE ? f->adaptive.paused[0]:f->tight.direction>0) &&
        f->speed.left.target_cps==0 && f->speed.left.duty==0;
    bool right=(MOTION_LINE_ADAPTIVE ? f->adaptive.paused[1]:f->tight.direction<0) &&
        f->speed.right.target_cps==0 && f->speed.right.duty==0;
    return (left ? 1u:0u)|(right ? 2u:0u);
}

static inline bool line_follow_sample_valid(const line_msg_t *line,uint32_t now,
                                      uint32_t sampled_ms,uint32_t epoch,
                                      uint32_t line_epoch,bool have_line)
{
    return have_line && line && epoch==line_epoch &&
           (uint32_t)(now-sampled_ms)<=LINE_FOLLOW_INPUT_AGE_MS && !line->fault &&
           line->err>=-100 && line->err<=100;
}
/* Admission remains strict: both-black can never start a run. */
static inline bool line_follow_usable(const line_msg_t *line,uint32_t now,
                                      uint32_t sampled_ms,uint32_t epoch,
                                      uint32_t line_epoch,bool have_line)
{
    return line_follow_sample_valid(line,now,sampled_ms,epoch,line_epoch,have_line) &&
           (line->state==LINE_ON || line->state==LINE_EDGE_ONLY ||
            (LINE_FEEDBACK_PROFILE && line->state==LINE_LOST && line->err==0));
}
static inline bool line_follow_targets(int error,int32_t *left,int32_t *right)
{
    if(error < -100 || error > 100 || !left || !right)return false;
    /* Buddy 3: positive error means line to LEFT. Slow the left wheel.
     * Keep a positive rolling target: normal steering must not repeatedly
     * enter the speed controller's immediate-stop/feed-forward-restart path.
     * Fault/STOP/expiry still stop both outputs immediately in the owner. */
    int magnitude=error<0 ? -error : error;
    if(magnitude>LINE_FULL_TURN_ERROR)magnitude=LINE_FULL_TURN_ERROR;
    int32_t outer=LINE_CRUISE_CPS-(LINE_CRUISE_CPS-LINE_CURVE_CPS)*magnitude/LINE_FULL_TURN_ERROR;
    int32_t inner=outer-(outer-LINE_INNER_MIN_CPS)*magnitude/LINE_FULL_TURN_ERROR;
    *left=error>0 ? inner : outer;
    *right=error<0 ? inner : outer;
    return true;
}
/* Slow briefly after a significant bend. Scale both targets equally so
 * gentle/opposite steering keeps its direction and wheel-speed ratio. No old
 * steering direction is latched: the current IR error always selects the turn. */
static inline void line_curve_exit_limit(line_follow_t *f,int error,uint32_t now,
                                         int32_t *left,int32_t *right)
{
    int magnitude=error<0 ? -error : error;
    if(magnitude>=LINE_CURVE_ENTER_ERROR) {
        f->curve_seen=true;f->last_curve_ms=now;
    } else if(f->curve_seen && (uint32_t)(now-f->last_curve_ms)<LINE_CURVE_EXIT_HOLD_MS) {
        int32_t maximum=*left>*right ? *left : *right;
        if(maximum>LINE_CURVE_EXIT_CPS) {
            *left=*left*LINE_CURVE_EXIT_CPS/maximum;
            *right=*right*LINE_CURVE_EXIT_CPS/maximum;
        }
    }
}
/* OUTSIDE-mode zero error is ambiguous: centred or both sensors off track.
 * Briefly retain a fading, gentle correction after a significant curve.
 * Only actual strong input renews the timer. Every nonzero observation wins;
 * even a weak opposite observation cancels the old direction. No recovery
 * overrides input guards, and no synthetic error refreshes exit/hold memory. */
static inline int line_steering_error(line_follow_t *f,const line_msg_t *line,uint32_t now)
{
    int error=line->err;
    if(error) {
        int direction=error>0 ? 1:-1;
        if((error<0 ? -error:error)>=LINE_CURVE_ENTER_ERROR) {
            f->curve_direction=(int8_t)direction;
            f->last_steering_ms=now;
        } else if(direction!=f->curve_direction) {
            f->curve_direction=0;
        }
        return error;
    }
    uint32_t elapsed=now-f->last_steering_ms;
    if(line->state!=LINE_ON || !f->curve_direction || elapsed>=LINE_STEERING_HOLD_MS)
        return 0;
    int magnitude=(int)(LINE_STEERING_HOLD_ERROR*(LINE_STEERING_HOLD_MS-elapsed)/LINE_STEERING_HOLD_MS);
    return f->curve_direction*magnitude;
}
static inline int32_t line_target_ramp(int32_t previous,int32_t target,uint32_t dt_ms)
{
    int32_t limit=(int32_t)(LINE_TARGET_SLEW_CPS_PER_SECOND*dt_ms/1000u);
    int32_t down=(int32_t)(LINE_TARGET_DECEL_CPS_PER_SECOND*dt_ms/1000u);
    if(target>previous+limit)return previous+limit;
    if(target<previous-down)return previous-down;
    return target;
}
/* Line-search trial only: seed restart near the last rolling output instead
 * of the full 155/185 feed-forward kick. The 80..110 permille seed is a
 * provisional breakaway compromise; existing slew, feedback and stall guard
 * apply immediately. No extra time exemption or change to the shared PI loop. */
static inline void line_soft_restart(wheel_speed_t *w,int32_t target,unsigned previous)
{
    if(!LINE_FEEDBACK_PROFILE || !target || w->target_cps || w->duty)return;
    unsigned seed=previous<80u ? 80u:previous>110u ? 110u:previous;
    int32_t error=target-w->filtered_milli_cps/1000;
    w->duty=seed; /* nonzero bypasses the shared full feed-forward restart */
    w->integral_milli=clamp_i64(((int64_t)seed-w->feedforward)*1000-
        (int64_t)w->kp_milli*error,w->integral_floor_milli,INTEGRAL_LIMIT_MILLI);
}
/* Before the first encoder count, changing a turn target must not undo the
 * matched low-power startup seed. Cancel only that target's proportional
 * jump; feedback, normal acceleration slew and the 200ms stall guard remain.
 * Once any forward travel occurs, full deceleration authority applies. */
static inline void line_startup_retarget(wheel_speed_t *w,int32_t total,int32_t target)
{
    if(!LINE_FEEDBACK_PROFILE || total || !target || !w->target_cps || !w->duty)return;
    w->integral_milli=clamp_i64((int64_t)w->integral_milli+
        (int64_t)w->kp_milli*(w->target_cps-target),
        w->integral_floor_milli,INTEGRAL_LIMIT_MILLI);
}
static inline void line_follow_begin(line_follow_t *f,uint32_t left,uint32_t right,
                                    uint32_t invalid,uint32_t sampled_ms,uint32_t now)
{
    *f=(line_follow_t){0};
    speed_control_start(&f->speed);
    /* Let the speed loop reduce PWM while retaining positive wheel targets. */
    f->speed.left.minimum=f->speed.right.minimum=0;
    /* Low curve targets need enough negative integral to cancel the fixed
     * feed-forward bias. The straight-bench +/-40-permille limit otherwise
     * leaves persistent overspeed. Retain its positive limit, anti-windup,
     * output cap and slew; never integrate below cancelling the whole bias. */
    f->speed.left.integral_floor_milli=-(int32_t)f->speed.left.feedforward*1000;
    f->speed.right.integral_floor_milli=-(int32_t)f->speed.right.feedforward*1000;
    /* Faster line-only feedback and bounded deceleration reduce curve-entry
     * delay. Acceleration and the 20% output cap stay unchanged. */
    f->speed.left.kp_milli=f->speed.right.kp_milli=LINE_SPEED_KP_MILLI;
    f->speed.left.ki_milli=f->speed.right.ki_milli=LINE_SPEED_KI_MILLI;
    f->speed.left.decel_per_second=f->speed.right.decel_per_second=LINE_DECEL_PER_SECOND;
    f->speed.left.duty=LEFT_FEEDFORWARD;
    f->speed.right.duty=RIGHT_FEEDFORWARD;
    f->speed.left.target_cps=f->speed.right.target_cps=LINE_CRUISE_CPS;
#if LINE_FEEDBACK_PROFILE
    /* Initial GP20 startup must use the same low-power seeding as a restart.
     * Merely lowering target speed leaves the old full feed-forward kick.
     * Seed BOTH output and integral so the first PI step cannot restore it.
     * Admission itself still writes no motor output; normal slew, encoder
     * checks and the original startup stall deadline govern every step. */
    f->speed.left.duty=f->speed.right.duty=0;
    f->speed.left.target_cps=f->speed.right.target_cps=0;
    line_soft_restart(&f->speed.left,LINE_CRUISE_CPS,80u);
    line_soft_restart(&f->speed.right,LINE_CRUISE_CPS,80u);
    f->speed.left.target_cps=f->speed.right.target_cps=LINE_CRUISE_CPS;
#endif
    f->origin_left=left;f->origin_right=right;f->invalid=invalid;
    f->previous_ms=sampled_ms;f->started_ms=now;
    f->last_progress_ms[0]=f->last_progress_ms[1]=now;
}
static inline line_step_t line_follow_step_observed(line_follow_t *f,uint32_t raw_left,
        uint32_t raw_right,uint32_t invalid,uint32_t sampled_ms,uint32_t now,
        const line_msg_t *line,uint32_t line_ms,uint32_t epoch,uint32_t line_epoch,
        bool have_line,bool connected,uint16_t levels)
{
    f->observed_levels=levels;
    f->encoder_reject=f->speed_reject=0;
    if(!connected)return LINE_STEP_DISCONNECTED;
    if(!line_follow_sample_valid(line,now,line_ms,epoch,line_epoch,have_line))return LINE_STEP_IR;
    bool both_black=LINE_FEEDBACK_PROFILE && line->state==LINE_BOTH_BLACK && line->err==0;
    if(!both_black && !line_follow_usable(line,now,line_ms,epoch,line_epoch,have_line))return LINE_STEP_IR;
    if((uint32_t)(now-f->started_ms)>=LINE_FOLLOW_MAX_MS)return LINE_STEP_DONE;
    if((uint32_t)(now-sampled_ms)>LINE_FOLLOW_INPUT_AGE_MS)return LINE_STEP_TIME;
    if(invalid!=f->invalid){f->encoder_reject=LINE_ENCODER_INVALID_EDGES;return LINE_STEP_ENCODER;}
    uint32_t dt=sampled_ms-f->previous_ms;
    if(dt>CONTROL_MAX_US/1000u)return LINE_STEP_TIME;
    if(dt<CONTROL_MIN_US/1000u)return LINE_STEP_WAIT;
    /* Verified forward signs: left raw negative, right raw positive. */
    int64_t l=-(int64_t)motion_count_delta(raw_left,f->origin_left);
    int64_t r=motion_count_delta(raw_right,f->origin_right);
    int64_t backwards[2]={f->previous_total[0]-l,f->previous_total[1]-r};
    /* Only the interval already commanded stopped can settle backward. Do
     * not grant tolerance on entry to a pause, to the powered outer wheel,
     * or renew it after forward motion/bounce within the same pause. */
    unsigned pause_mask=line_pause_eligible(f);
    bool paused[2]={(pause_mask&1u)!=0,(pause_mask&2u)!=0};
    bool previously_stopped[2]={!f->speed.left.target_cps && !f->speed.left.duty,
                               !f->speed.right.target_cps && !f->speed.right.duty};
    bool previously_rolling[2]={f->speed.left.target_cps>=LINE_INNER_MIN_CPS && f->speed.left.duty>0,
                               f->speed.right.target_cps>=LINE_INNER_MIN_CPS && f->speed.right.duty>0};
    bool settled[2]={false,false};
    for(unsigned i=0;i<2;++i)
        settled[i]=paused[i] && backwards[i]>0 &&
            (!MOTION_LINE_ADAPTIVE || (f->pause_window_active[i] &&
                (uint32_t)(now-f->pause_window_ms[i])<LINE_PAUSE_SETTLE_MS)) &&
            backwards[i]+f->pause_reverse_used[i]<=LINE_PAUSE_REVERSE_BUDGET;
    if(l<0)f->encoder_reject|=LINE_ENCODER_LEFT_NEGATIVE;
    if(r<0)f->encoder_reject|=LINE_ENCODER_RIGHT_NEGATIVE;
    if(backwards[0]>0 && !settled[0])f->encoder_reject|=LINE_ENCODER_LEFT_RECEDING;
    if(backwards[1]>0 && !settled[1])f->encoder_reject|=LINE_ENCODER_RIGHT_RECEDING;
    if(f->encoder_reject)return LINE_STEP_ENCODER;
    if(l>=LINE_FOLLOW_COUNT_LIMIT || r>=LINE_FOLLOW_COUNT_LIMIT)return LINE_STEP_LIMIT;
    int32_t lt,rt;
#if MOTION_LINE_ADAPTIVE
    line_adaptive_output_t output=line_adaptive_step(&f->adaptive,levels,now,dt,(int32_t)l,(int32_t)r);
    if(output.mode==LINE_ADAPT_INVALID)return LINE_STEP_IR;
    if(output.mode==LINE_ADAPT_ROUTE_REQUIRED)return LINE_STEP_ROUTE_REQUIRED;
    if(output.mode==LINE_ADAPT_LOST)return LINE_STEP_RECOVERY_EXHAUSTED;
    if(output.mode==LINE_ADAPT_CROSSING_FAILED)return LINE_STEP_CROSSING_EXHAUSTED;
    lt=output.left;rt=output.right;
    /* Applied target imbalance includes uncertain/search modes. The raw
     * calibrated pair is logged separately, not replaced by synthetic data. */
    f->steering_error=(int8_t)la_clip((rt-lt)/6,-100,100);
    f->tight.direction=lt==0 && rt>0 ? 1:rt==0 && lt>0 ? -1:0;
    f->search.phase=f->adaptive.phase;
#else
#if MOTION_LINE_SEARCH
    /* The only extra admission is an already-running, recently observed curve.
     * Both-black does not count as directional evidence or renew search/tight
     * timers. In particular it cannot rescue an already active lost-line search. */
    if(both_black && f->search.active)return LINE_STEP_IR;
    bool may_overlap=f->curve_seen && now-f->last_curve_ms<=LINE_OVERLAP_RECENT_MS && (l>0 || r>0);
    bool directional=(line->state==LINE_ON || line->state==LINE_EDGE_ONLY) &&
                     (line->err>=LINE_SEARCH_SEEN_ERROR || line->err<=-LINE_SEARCH_SEEN_ERROR);
    line_overlap_result_t overlap=line_overlap_step(&f->overlap,both_black,may_overlap,directional,
        now,(int32_t)l,(int32_t)r,f->speed.left.target_cps,f->speed.right.target_cps,&lt,&rt);
    if(overlap==LINE_OVERLAP_UNSAFE)return LINE_STEP_IR;
    if(overlap==LINE_OVERLAP_TIMEOUT)return LINE_STEP_OVERLAP_TIMEOUT;
    if(overlap==LINE_OVERLAP_RANGE)return LINE_STEP_OVERLAP_RANGE;
    if(overlap==LINE_OVERLAP_HOLD) {
        if(f->tight.direction && now-f->tight.started_ms>=LINE_TIGHT_MAX_MS)return LINE_STEP_TIGHT_TIMEOUT;
        f->tight.pending_direction=0;
        f->steering_error=(int8_t)(lt<rt ? 30:lt>rt ? -30:0);
    } else {
    line_search_result_t search=line_search_step(&f->search,line->err,line->state==LINE_LOST,
        f->tight.direction!=0,now,(int32_t)l,(int32_t)r,&lt,&rt);
    if(search==LINE_SEARCH_TIMEOUT)return LINE_STEP_SEARCH_TIMEOUT;
    if(search==LINE_SEARCH_RANGE)return LINE_STEP_SEARCH_RANGE;
    bool searching=search==LINE_SEARCH_SWEEP;
    if(searching) {
        /* Leave any prior intentional pause once. Search itself never pauses
         * or reverses a wheel, and does not feed synthetic error to tight mode. */
        if(f->tight.direction)line_tight_exit(&f->tight,now);
        f->tight.pending_direction=0;
        f->steering_error=(int8_t)(lt<rt ? 60:-60);
    } else
#endif
    {
#if MOTION_LINE_SEARCH
        /* Debounce a brief ambiguous observation while paused, then resume
         * slow forward motion for the remainder of the fixed grace interval. */
        if(f->tight.direction && f->search.zero_pending &&
           now-f->search.zero_ms>=LINE_SEARCH_PAUSE_RELEASE_MS)
            line_tight_exit(&f->tight,now);
#endif
        line_tight_result_t tight=line_tight_step(&f->tight,line->err,now);
        if(tight==LINE_TIGHT_TIMEOUT)return LINE_STEP_TIGHT_TIMEOUT;
        if(tight==LINE_TIGHT_UNSEEN)return LINE_STEP_TIGHT_UNSEEN;
        f->steering_error=(int8_t)line_steering_error(f,line,now);
        if(!line_follow_targets(f->steering_error,&lt,&rt))return LINE_STEP_IR;
        line_curve_exit_limit(f,line->err,now,&lt,&rt);
        if(tight==LINE_TIGHT_ACTIVE) {
            lt=f->tight.direction>0 ? 0:LINE_TIGHT_OUTER_CPS;
            rt=f->tight.direction<0 ? 0:LINE_TIGHT_OUTER_CPS;
        }
#if MOTION_LINE_SEARCH
        else if(f->search.zero_pending && line->err==0) {
            /* A recent bend gets a short continuation, not blind straight
             * travel. Weak real readings keep their own steering above.
             * The search timer is based only on real evidence. */
            if(f->search.curve_grace && f->search.curve_direction) {
                int direction=f->search.curve_direction;
                lt=direction>0 ? LINE_SEARCH_INNER_CPS:LINE_SEARCH_HOLD_OUTER_CPS;
                rt=direction<0 ? LINE_SEARCH_INNER_CPS:LINE_SEARCH_HOLD_OUTER_CPS;
                f->steering_error=(int8_t)(direction*LINE_SEARCH_CURVE_ERROR);
            } else {
                lt=rt=LINE_SEARCH_GRACE_CPS;f->steering_error=0;
            }
        }
#endif
    }
#if MOTION_LINE_SEARCH
    }
#endif
#endif /* MOTION_LINE_ADAPTIVE */
    /* An intentional inside-wheel pause cuts its output immediately. The
     * outside wheel and subsequent rolling targets retain normal ramps.
     * Search-trial resume uses line_soft_restart; other profiles retain their
     * breakaway feed-forward. Hysteresis/cooldown limit repeated restarts. */
    /* Restore v2's immediate commanded pause; positive targets retain ramps.
     * Encoder settling is guarded separately; fault/STOP remains immediate. */
    if(lt)lt=line_target_ramp(f->speed.left.target_cps,lt,dt);
    if(rt)rt=line_target_ramp(f->speed.right.target_cps,rt,dt);
    if(l>f->previous_total[0] || lt==0 || f->speed.left.target_cps==0)f->last_progress_ms[0]=now;
    if(r>f->previous_total[1] || rt==0 || f->speed.right.target_cps==0)f->last_progress_ms[1]=now;
    if((lt && (uint32_t)(now-f->last_progress_ms[0])>=LINE_FOLLOW_STALL_MS) ||
       (rt && (uint32_t)(now-f->last_progress_ms[1])>=LINE_FOLLOW_STALL_MS))return LINE_STEP_STALL;
    unsigned rejected;
    /* Treat the permitted settling sample as zero speed on that wheel only.
     * Preserve actual signed totals; no accumulating synthetic distance or
     * deferred negative delta on restart. Commit only after both wheels pass.
     * The shared speed controller retains its strict reverse/rate checks. */
    speed_control_t next_speed=f->speed;
    if(lt==0 && f->speed.left.target_cps)f->pre_pause_duty[0]=f->speed.left.duty;
    if(rt==0 && f->speed.right.target_cps)f->pre_pause_duty[1]=f->speed.right.duty;
    line_soft_restart(&next_speed.left,lt,f->pre_pause_duty[0]);
    line_soft_restart(&next_speed.right,rt,f->pre_pause_duty[1]);
    line_startup_retarget(&next_speed.left,(int32_t)l,lt);
    line_startup_retarget(&next_speed.right,(int32_t)r,rt);
    if(settled[0])next_speed.previous_left=(int32_t)l;
    if(settled[1])next_speed.previous_right=(int32_t)r;
    if(!speed_control_targets_step_report(&next_speed,(int32_t)l,(int32_t)r,dt*1000u,lt,rt,&rejected)) {
        f->encoder_reject=LINE_ENCODER_SPEED;f->speed_reject=(uint8_t)rejected;
        f->rejected_target_left=lt;f->rejected_target_right=rt;
        return LINE_STEP_ENCODER;
    }
    f->speed=next_speed;
    for(unsigned i=0;i<2;++i) {
        if(settled[i]) {
            f->pause_reverse_used[i]+=(uint8_t)backwards[i];
            f->last_settle_ms[i]=now;
        }
        bool output_paused=i==0 ? lt==0 && !f->speed.left.duty:rt==0 && !f->speed.right.duty;
        if(output_paused && !previously_stopped[i])f->output_pause_ms[i]=now;
        if(MOTION_LINE_ADAPTIVE) {
            /* A short command pulse or encoder bounce must not renew either
             * the time window or gross reverse allowance. Require observed
             * forward rolling in prior AND next powered intervals. */
            bool rolling=i==0 ? lt>=LINE_INNER_MIN_CPS && f->speed.left.duty>0:
                                rt>=LINE_INNER_MIN_CPS && f->speed.right.duty>0;
            if(previously_rolling[i] && rolling && backwards[i]<0) {
                f->pause_forward_ms[i]+=dt;
                if(f->pause_forward_ms[i]>LINE_PAUSE_REARM_MS)f->pause_forward_ms[i]=LINE_PAUSE_REARM_MS;
                f->pause_forward_counts[i]+=(uint32_t)(-backwards[i]);
                if(f->pause_forward_counts[i]>LINE_PAUSE_REARM_COUNTS)f->pause_forward_counts[i]=LINE_PAUSE_REARM_COUNTS;
                if(f->pause_forward_ms[i]>=LINE_PAUSE_REARM_MS && f->pause_forward_counts[i]>=LINE_PAUSE_REARM_COUNTS) {
                    f->pause_reverse_used[i]=0;f->pause_window_active[i]=false;
                }
            } else f->pause_forward_ms[i]=f->pause_forward_counts[i]=0;
            if(output_paused && !f->pause_window_active[i]) {
                f->pause_window_active[i]=true;f->pause_window_ms[i]=now;
            }
        } else if(i==0 ? lt!=0 : rt!=0)f->pause_reverse_used[i]=0;
    }
    f->previous_total[0]=(int32_t)l;f->previous_total[1]=(int32_t)r;
    f->previous_ms=sampled_ms;
    return LINE_STEP_OK;
}
/* Compatibility API for old profiles. Adaptive mode intentionally rejects a
 * publisher without levels; error==0 alone is not surface coverage. */
static inline line_step_t line_follow_step(line_follow_t *f,uint32_t raw_left,
        uint32_t raw_right,uint32_t invalid,uint32_t sampled_ms,uint32_t now,
        const line_msg_t *line,uint32_t line_ms,uint32_t epoch,uint32_t line_epoch,
        bool have_line,bool connected)
{
    return line_follow_step_observed(f,raw_left,raw_right,invalid,sampled_ms,now,
        line,line_ms,epoch,line_epoch,have_line,connected,0);
}
#endif

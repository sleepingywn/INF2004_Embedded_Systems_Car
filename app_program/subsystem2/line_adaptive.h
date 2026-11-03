#ifndef MOTION_LINE_ADAPTIVE_H
#define MOTION_LINE_ADAPTIVE_H
#include <stdint.h>
#include <stdbool.h>
#include "line_observation.h"
#include "line_crossing.h"

/* Count-domain calibration, independent of course order/curve duration.
 * Provisional until tested on multiple unseen layouts with unchanged settings.
 * No encoder count here is claimed to represent a calibrated distance/angle. */
typedef struct {
    int32_t cruise, minimum_rolling, maximum, search_speed;
    int32_t kp_milli, kd_milli, max_steering;
    uint32_t filter_ms, derivative_ms;
    unsigned seen_enter, seen_leave, black;
    uint32_t confirm_ms, pause_confirm_ms, uncertainty_counts;
    uint32_t sweep_counts, recovery_counts, recovery_ms;
} line_adaptive_config_t;
static const line_adaptive_config_t line_adaptive_config={
    .cruise=450,.minimum_rolling=250,.maximum=600,.search_speed=350,
    .kp_milli=3500,.kd_milli=40,.max_steering=400,
    .filter_ms=40,.derivative_ms=80,
    .seen_enter=20,.seen_leave=10,.black=75,
    .confirm_ms=60,.pause_confirm_ms=60,.uncertainty_counts=120,
    .sweep_counts=160,.recovery_counts=800,.recovery_ms=3000
};
typedef enum {
    LINE_ADAPT_TRACK, LINE_ADAPT_UNCERTAIN, LINE_ADAPT_SEARCH,
    LINE_ADAPT_JUNCTION, LINE_ADAPT_ROUTE_REQUIRED,
    LINE_ADAPT_LOST, LINE_ADAPT_INVALID, LINE_ADAPT_CROSSING, LINE_ADAPT_CROSSING_FAILED
} line_adaptive_mode_t;
typedef struct {
    bool initialized, visible, uncertain, confirming, route_blocked;
    line_crossing_t crossing;
    line_stop_reason_t stop_reason;
    bool paused[2];
    bool pause_pending[2];
    uint32_t pause_since[2];
    uint32_t uncertain_ms, confirmation_ms;
    int32_t origin[2], sweep_origin[2];
    int32_t error_milli, derivative_milli, steering;
    int8_t last_direction, search_direction;
    uint8_t phase;
    line_adaptive_mode_t mode;
} line_adaptive_t;
typedef struct {
    int32_t left,right;
    line_adaptive_mode_t mode;
} line_adaptive_output_t;
static inline line_adaptive_output_t la_stop(line_adaptive_t *s,line_adaptive_mode_t mode,line_stop_reason_t reason)
{s->initialized=true;s->mode=mode;s->stop_reason=reason;return (line_adaptive_output_t){0,0,mode};}
static inline int32_t la_abs(int32_t x){return x<0 ? -x:x;}
static inline int32_t la_clip(int32_t x,int32_t lo,int32_t hi)
{return x<lo ? lo:x>hi ? hi:x;}
static inline int32_t la_travel(int32_t l,int32_t r,const int32_t origin[2])
{int32_t a=la_abs(l-origin[0]),b=la_abs(r-origin[1]);return a>b ? a:b;}
static inline line_adaptive_output_t la_output(line_adaptive_t *s,
        line_adaptive_mode_t mode,int32_t l,int32_t r)
{s->mode=mode;return (line_adaptive_output_t){l,r,mode};}

/* No motor writes or clock reads: deterministic observation -> wheel targets.
 * The owner retains STOP, encoder, sample-age, epoch, stall and duration guards.
 * Both-white is explicitly uncertain for the present OUTSIDE placement.
 * Both-black cannot identify a junction's branches.
 * The GP20 policy tries straight across within travel/time bounds; it does
 * not infer a branch direction from zero error. */
static inline line_adaptive_output_t line_adaptive_step(line_adaptive_t *s,
        uint16_t levels,uint32_t now,uint32_t dt,int32_t l,int32_t r)
{
    const line_adaptive_config_t *c=&line_adaptive_config;
    if(!line_levels_valid(levels) || dt<20 || dt>60 || l<0 || r<0)
        return la_stop(s,LINE_ADAPT_INVALID,LINE_STOP_INVALID_INPUT);
    if(s->initialized && (s->mode==LINE_ADAPT_ROUTE_REQUIRED ||
        s->mode==LINE_ADAPT_LOST || s->mode==LINE_ADAPT_INVALID || s->mode==LINE_ADAPT_CROSSING_FAILED))
        return la_output(s,s->mode,0,0);
    if(s->route_blocked)return la_stop(s,LINE_ADAPT_ROUTE_REQUIRED,LINE_STOP_ROUTE_COMMAND);
    unsigned left=line_level_left(levels),right=line_level_right(levels);
    int32_t error=(int32_t)left-(int32_t)right;
    bool black=left>=c->black && right>=c->black;
    unsigned maximum=left>right ? left:right;
    bool visible=!black && maximum>=(s->visible ? c->seen_leave:c->seen_enter);
    bool entering=!s->initialized;
    s->initialized=true;
    bool was_visible=s->visible;
    int32_t crossing_left=0,crossing_right=0;
    /* An unresolved line-loss budget cannot be escaped by entering a mark. */
    if(s->uncertain && now-s->uncertain_ms>=c->recovery_ms)
        return la_stop(s,LINE_ADAPT_LOST,LINE_STOP_RECOVERY_TIME);
    if(s->uncertain && la_travel(l,r,s->origin)>=(int32_t)c->recovery_counts)
        return la_stop(s,LINE_ADAPT_LOST,LINE_STOP_RECOVERY_DISTANCE);
    /* Partial darkness on both eyes can still be the edge of a wide mark.
     * Replenish the encounter budget only after one eye is clearly white. */
    unsigned minimum=left<right ? left:right;
    bool clear_line=visible && maximum>=c->seen_enter && minimum<=c->seen_leave;
    line_crossing_result_t crossing=line_crossing_step(&s->crossing,black,clear_line,now,l,r,
        &crossing_left,&crossing_right);
    if(crossing==CROSS_FAILED)
        return la_stop(s,LINE_ADAPT_CROSSING_FAILED,s->crossing.reason);
    if(crossing==CROSS_ADVANCE) {
        s->confirming=false;s->visible=false;
        s->paused[0]=s->paused[1]=false;
        s->pause_pending[0]=s->pause_pending[1]=false;
        s->error_milli=s->derivative_milli=s->steering=0;
        return la_output(s,LINE_ADAPT_CROSSING,crossing_left,crossing_right);
    }
    bool reacquired=!was_visible;
    s->visible=visible;
    if(!visible && !s->uncertain) {
        s->uncertain=true;s->uncertain_ms=now;
        s->origin[0]=l;s->origin[1]=r;s->phase=0;
        s->search_direction=s->last_direction ? s->last_direction:1;
    }
    if(s->uncertain) {
        /* Brief/noisy sightings may steer immediately but cannot replenish
         * the total recovery budget. Require stable evidence to clear it. */
        if(visible) {
            if(!s->confirming){s->confirming=true;s->confirmation_ms=now;}
            if(now-s->confirmation_ms>=c->confirm_ms) {
                s->uncertain=false;s->confirming=false;s->phase=0;
            }
        } else s->confirming=false;
    }
    if(!visible) {
        s->pause_pending[0]=s->pause_pending[1]=false;
        s->derivative_milli=0;
        /* Preserve filter continuity across intermittent edge sightings.
         * Fade toward neutral while the line position is unobservable. */
        s->error_milli-=(int32_t)((int64_t)s->error_milli*dt/(c->filter_ms+dt));
        if(s->phase==0 && la_travel(l,r,s->origin)<(int32_t)c->uncertainty_counts) {
            /* A small proportional continuation fades with travelled counts.
             * Startup with no direction proceeds slowly within the same bound. */
            int32_t remain=(int32_t)c->uncertainty_counts-la_travel(l,r,s->origin);
            int32_t steer=s->steering*remain/(int32_t)c->uncertainty_counts;
            steer=la_clip(steer,-100,100);
            s->paused[0]=s->paused[1]=false;
            return la_output(s,LINE_ADAPT_UNCERTAIN,c->search_speed-steer,c->search_speed+steer);
        }
        if(!s->phase){s->phase=1;s->sweep_origin[0]=l;s->sweep_origin[1]=r;}
        /* Differential wheel travel bounds a pivot without guessing degrees.
         * One sweep toward last evidence, then one wider opposite sweep. */
        int32_t differential=la_abs((l-s->sweep_origin[0])-(r-s->sweep_origin[1]));
        uint32_t span=c->sweep_counts*(s->phase==1 ? 1u:2u);
        if(differential>=(int32_t)span) {
            if(s->phase==2)return la_stop(s,LINE_ADAPT_LOST,LINE_STOP_SWEEP_COMPLETE);
            s->phase=2;s->search_direction=-s->search_direction;
            s->sweep_origin[0]=l;s->sweep_origin[1]=r;
        }
        s->paused[0]=s->search_direction>0;s->paused[1]=s->search_direction<0;
        return la_output(s,LINE_ADAPT_SEARCH,s->paused[0] ? 0:c->search_speed,
                         s->paused[1] ? 0:c->search_speed);
    }
    if(error>=10 || error<=-10)s->last_direction=error>0 ? 1:-1;
    if(entering){s->error_milli=0;s->derivative_milli=0;}
    if(reacquired)s->derivative_milli=0;
    /* Opposite evidence cancels old filter direction without snapping to a
     * full correction. One brief sighting must not cause stop/restart cycling. */
    if((error>=20 && s->error_milli<0) || (error<=-20 && s->error_milli>0)) {
        s->error_milli=0;s->derivative_milli=0;
    }
    int32_t previous=s->error_milli;
    s->error_milli+=(int32_t)((int64_t)(error*1000-previous)*dt/(c->filter_ms+dt));
    int32_t derivative=(s->error_milli-previous)*1000/(int32_t)dt;
    s->derivative_milli+=(int32_t)((int64_t)(derivative-s->derivative_milli)*dt/(c->derivative_ms+dt));
    int32_t p=(int32_t)((int64_t)c->kp_milli*s->error_milli/1000000);
    int32_t d=(int32_t)((int64_t)c->kd_milli*s->derivative_milli/1000000);
    int32_t steering=la_clip(p+la_clip(d,-100,100),-c->max_steering,c->max_steering);
    /* Strong opposite evidence wins immediately over filter/motor memory. */
    if((error>=20 && steering<0) || (error<=-20 && steering>0))steering=0;
    s->steering=steering;
    int32_t base=c->cruise-la_abs(steering)/3;
    int32_t targets[2]={base-steering,base+steering};
    for(unsigned i=0;i<2;++i) {
        targets[i]=la_clip(targets[i],0,c->maximum);
        /* Derivative spikes cannot initiate a wheel pause. Require the raw
         * direction and filtered proportional demand to agree continuously.
         * This debounce confirms evidence, never limits curve duration. */
        int32_t proportional=c->cruise-la_abs(p)/3+(i==0 ? -p:p);
        bool same_side=i==0 ? error>=20:error<=-20;
        bool candidate=same_side && proportional<c->minimum_rolling-70;
        if(!candidate)s->pause_pending[i]=false;
        else if(!s->pause_pending[i]){s->pause_pending[i]=true;s->pause_since[i]=now;}
        if(s->paused[i])s->paused[i]=same_side && proportional<c->minimum_rolling+30;
        else s->paused[i]=candidate && now-s->pause_since[i]>=c->pause_confirm_ms;
        targets[i]=s->paused[i] ? 0:la_clip(targets[i],c->minimum_rolling,c->maximum);
    }
    /* v4 restores the v2 tracking baseline: filtered-demand hysteresis and
     * the ordinary 600-count/s outer cap, without v3's raw-error gates. */
    return la_output(s,LINE_ADAPT_TRACK,targets[0],targets[1]);
}
#endif

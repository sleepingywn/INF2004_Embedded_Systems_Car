/*
 * imu_logic.c  -  Buddy 4 (IMU / Terrain) processing logic
 * Pure C + math.h only: no printf, no Pico SDK, no hardware.
 */
#include "imu_logic.h"
#include "imu_math.h" /* own sqrt/atan2/sin: the template has no libm */
#include <string.h>

#define RAD2DEG (57.29578f)
#define DEG2RAD (0.01745329f)
#define SLOW_LIFT_PHASE_G (0.017f)
#define SLOW_LIFT_RISE_S (0.20f)
#define SLOW_LIFT_MAX_SAMPLE_S (0.04f)

void imu_logic_default_config(imu_config_t *config)
{
    config->calib_time_s = 2.0f;
    config->filter_tau_s = 0.15f;
    config->accel_gate_g =
        0.10f; /* 0.15 let spin-on-the-spot samples (0.85-0.88 g) through */
    config->hump_mode =
        IMU_HUMP_ONE_SIDED; /* our track: hump on the right only */
    config->hump_enter_deg =
        3.0f; /* a 10-13 mm one-sided hump tilts only ~4-5 deg */
    config->hump_exit_deg = 1.5f;
    config->hump_hold_s = 0.10f;
    config->hump_timeout_s =
        10.0f; /* a slow hand push kept the wheel on the hump ~5 s */
    config->hump_lever_mm =
        78.0f; /* calibrated 2026-11-25 (S4-U18): right wheel on the 10 mm hump,
                  car still -> total tilt 7.35 deg; 10 / sin(7.35) = 78 (was 98
                  from an assumed 11.5 mm hump at 6.75 deg) */
    config->tilt_deg = 25.0f;
    /* Lift / overturn (12 Nov 2026).  First car test: a quick hand lift
     * peaked at +0.203 g (just over the first 0.20 g value) and a gentler
     * lift was missed, so 0.15 g.  Driving logged |a| 0.88-1.12 g in single
     * samples; the median of 3 + 60 ms hold removes those.  Humps tilt
     * ~7 deg, tilt event is 25. */
    config->lift_up_g = 0.15f;
    config->lift_hold_s = 0.06f; /* 3 samples of the 3-sample median */
    config->overturn_deg = 45.0f;
    config->overturn_hold_s = 0.10f;
    config->lift_clear_s = 1.0f;
    /* Slow lift: 0.015 g*s = 0.15 m/s upward.  A calm hand lift reaches
     * 0.2-0.5 m/s.  Recorded driving (hump pass, 50 Hz): |a| sd 0.006 g, the
     * sum peaked at 0.002 g*s; KR4 motor runs showed |a| 0.88-1.07 g. */
    config->lift_vel_g_s = 0.010f;
    /* 0.10 m/s base after the missed 19 Nov pickup; vibration raises it. */
    config->lift_vel_noise_k = 0.3f;
    config->lift_vel_tau_s = 0.6f;
    config->lift_vel_clip_g = 0.3f;
    config->handle_deg =
        5.0f; /* stopped: tilt within ~1 deg; slow pick-up on 19 Nov: 4-5 deg */
    config->lift_vel_still_g_s = 0.006f;
    /* Slow lift while driving (19 Nov 2026, twice): rise 105-117 mm/s, then
       V5 lost the line and stopped the wheels.  Off the hump, driving stays
       below ~90 mm/s; the hump itself reached 104, so only off the hump. */
    config->rise_then_stop_g_s = 0.0075f; /* 75 mm/s */
    config->rise_memory_s = 1.5f;
    config->free_spin_mm_s = 38.0f;
    /* lifted: 44 mm/s (twice); V5 straight: 20-33 */ /* 60 mm/s; still car
                                                         in the logs: rise
                                                         <= 31 mm/s */
    config->handle_hold_s = 0.2f;
    config->rest_settle_s = 0.5f;
}

void imu_logic_init(imu_logic_t *state, const imu_config_t *cfg)
{
    memset(state, 0, sizeof(*state));
    state->cfg = *cfg;
    state->hump_state = HUMP_FLAT;
    state->scale = 1.0f;
    state->rise_age = state->spin_age = 1e3f; /* nothing seen yet */
}

/* After the sensor chip is re-initialised (frozen readings), the mounting
 * is unchanged, so the boot calibration still holds.  Recalibrating here
 * would happen while the car may be driving or on the hump and would bake
 * that tilt into the reference.  Keep the calibration and this run's hump
 * results (FR5: highest completed hump in the current run); drop everything
 * built on the bad data: the filter, a hump in progress, the lift detector and
 * the LIFTED latch.  The filter restarts level; a new hump must start from
 * flat. */
void imu_logic_sensor_restart(imu_logic_t *state)
{
    if (!state->calibrated)
    { /* fault during calibration: start over */
        imu_config_t cfg = state->cfg;
        imu_logic_init(state, &cfg);
        return;
    }
    imu_logic_t keep = *state;
    imu_logic_init(state, &keep.cfg);
    state->calibrated = true;
    state->t = keep.t;
    state->pitch0 = keep.pitch0;
    state->roll0 = keep.roll0;
    memcpy(state->rot, keep.rot, sizeof(state->rot));
    state->rot_ok = keep.rot_ok;
    state->scale = keep.scale;
    state->filt.fwd = 0.0f;
    state->filt.side = 0.0f;
    state->filt.up = 1.0f;
    state->highest_hump_peak_mm = keep.highest_hump_peak_mm;
    state->current_hump_peak_mm = keep.current_hump_peak_mm;
    state->hump_count = keep.hump_count;
    state->rearm_needed = true; /* next hump only after a flat reading */
}

void imu_logic_reset_run(imu_logic_t *state)
{
    state->rearm_needed = state->rearm_needed || state->hump_state != HUMP_FLAT;
    state->hump_state = HUMP_FLAT;
    state->enter_timer = state->desc_timer = state->exit_timer =
        state->state_timer = 0.0f;
    state->pre_h_mm = state->hump_h_mm = state->pitch_peak_mm =
        state->hump_peak_angle = 0.0f;
    state->highest_hump_peak_mm = 0.0f;
    state->current_hump_peak_mm = 0.0f;
    state->current_hump_peak_pitch_deg = 0.0f;
    state->current_hump_peak_tilt_deg = 0.0f;
    state->current_hump_peak_roll_deg = 0.0f;
    state->current_hump_one_sided = false;
    state->hump_count = 0;
}

const char *imu_hump_state_name(hump_state_t hump_state)
{
    switch (hump_state)
    {
        case HUMP_FLAT:
            return "FLAT";
        case HUMP_CLIMBING:
            return "CLIMBING";
        case HUMP_DESCENDING:
            return "DESCENDING";
        default:
            return "?";
    }
}

imu_motion_t imu_classify_motion(const imu_logic_t *state, float speed_mm_s,
                                 float turn_dps, bool wheels_valid,
                                 imu_motion_t prev)
{
    float v = imu_fabsf(speed_mm_s), w = imu_fabsf(turn_dps);
    float turn_on = (prev == IMU_MOTION_TURNING) ? IMU_MOTION_TURN_EXIT_DPS
                                                 : IMU_MOTION_TURN_DPS;

    if (!state->calibrated)
    {
        return IMU_MOTION_UNKNOWN;
    }
    /* IMU-based classes first: they do not need the wheels */
    if (state->lifted)
    {
        return IMU_MOTION_LIFTED;
    }
    if (state->tilt_active)
    {
        return IMU_MOTION_TILTED;
    }
    if (state->hump_state != HUMP_FLAT)
    {
        return IMU_MOTION_ON_HUMP;
    }
    if (!wheels_valid)
    {
        return IMU_MOTION_UNKNOWN;
    }
    if (w >= IMU_MOTION_SPIN_DPS && v < IMU_MOTION_SPIN_MM_S)
    {
        return IMU_MOTION_SPINNING;
    }
    if (v < IMU_MOTION_STOP_MM_S && w < turn_on)
    {
        return IMU_MOTION_STOPPED;
    }
    if (w >= turn_on)
    {
        return IMU_MOTION_TURNING;
    }
    return IMU_MOTION_STRAIGHT;
}

const char *imu_motion_name(imu_motion_t m)
{
    static const char *const name[8] = {"UNKNOWN", "STOPPED",  "STRAIGHT",
                                        "TURNING", "SPINNING", "ON_HUMP",
                                        "TILTED",  "LIFTED"};
    return ((unsigned)m < 8u) ? name[m] : "?";
}

/* Pitch from gravity: how much of gravity points along the forward axis. */
static float pitch_from(imu_accel_t accel)
{
    return imu_atan2f(accel.fwd, imu_sqrtf(accel.side * accel.side +
                                           accel.up * accel.up)) *
           RAD2DEG;
}

static float roll_from(imu_accel_t accel)
{
    return imu_atan2f(accel.side, accel.up) * RAD2DEG;
}

/* Keep an angle difference in -180..+180, e.g. +358 deg becomes -2 deg.
 * Needed because atan2 jumps from +180 to -180. */
static float wrap180(float d)
{
    while (d > 180.0f)
    {
        d -= 360.0f;
    }
    while (d < -180.0f)
    {
        d += 360.0f;
    }
    return d;
}

/* Build the sensor -> car rotation from the average reading taken while the
 * car stands still on level ground (gravity points straight down).
 *   up    = direction of the measured gravity reaction
 *   fwd   = the sensor's forward axis, tipped onto the horizontal plane
 *   side  = up x fwd  (+ = left)
 * This removes ANY mounting angle exactly (tilted, upside down...), so
 * rolling the car no longer leaks into pitch and vice versa. */
static void build_rotation(imu_logic_t *state, imu_accel_t avg)
{
    float n =
        imu_sqrtf(avg.fwd * avg.fwd + avg.side * avg.side + avg.up * avg.up);
    state->scale = (n > 0.5f && n < 1.5f) ? 1.0f / n : 1.0f;
    state->rot_ok = false;
    if (n < 0.5f)
    {
        return;
    }

    float u[3] = {avg.fwd / n, avg.side / n, avg.up / n};
    float f[3] = {1.0f - u[0] * u[0], -u[0] * u[1],
                  -u[0] * u[2]}; /* x minus its vertical part */
    float fn = imu_sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (fn < 0.2f)
    {
        return; /* forward axis nearly vertical: mapping is wrong */
    }
    f[0] /= fn;
    f[1] /= fn;
    f[2] /= fn;
    float sd[3] = {u[1] * f[2] - u[2] * f[1], u[2] * f[0] - u[0] * f[2],
                   u[0] * f[1] - u[1] * f[0]};
    for (int i = 0; i < 3; i++)
    {
        state->rot[0][i] = f[i];
        state->rot[1][i] = sd[i];
        state->rot[2][i] = u[i];
    }
    state->rot_ok = true;
}

/* Sensor reading -> car frame, scaled so that rest = 1 g. */
static imu_accel_t to_car(const imu_logic_t *state, imu_accel_t accel)
{
    imu_accel_t r = accel;
    if (state->rot_ok)
    {
        float v[3] = {accel.fwd, accel.side, accel.up};
        r.fwd = state->rot[0][0] * v[0] + state->rot[0][1] * v[1] +
                state->rot[0][2] * v[2];
        r.side = state->rot[1][0] * v[0] + state->rot[1][1] * v[1] +
                 state->rot[1][2] * v[2];
        r.up = state->rot[2][0] * v[0] + state->rot[2][1] * v[1] +
               state->rot[2][2] * v[2];
    }
    r.fwd *= state->scale;
    r.side *= state->scale;
    r.up *= state->scale;
    return r;
}

static void abort_hump(imu_logic_t *state)
{
    /* Lasted too long: probably a static slope or a disturbance, not a hump. */
    state->hump_state = HUMP_FLAT;
    state->rearm_needed = true;
    state->enter_timer = state->desc_timer = state->exit_timer =
        state->state_timer = 0.0f;
}

void imu_logic_wheel_gap(imu_logic_t *state)
{
    if (state->hump_state != HUMP_FLAT || state->enter_timer > 0.0f ||
        state->pre_h_mm > 0.0f)
    {
        if (state->hump_state != HUMP_FLAT)
        {
            state->current_hump_peak_mm = 0.0f;
        }
        abort_hump(state);
        state->pre_h_mm = 0.0f;
    }
}

static void update_attitude(imu_logic_t *state, imu_accel_t accel, float dt_s,
                            float motion_accel_g)
{
    const imu_config_t *config = &state->cfg;
    imu_accel_t g = accel;
    g.fwd -= motion_accel_g;

    /* ---------- 4. Low-pass filter, skipping obviously disturbed samples ---
     */
    float g_mag = imu_sqrtf(g.fwd * g.fwd + g.side * g.side + g.up * g.up);
    if (imu_fabsf(g_mag - 1.0f) <= config->accel_gate_g)
    {
        float alpha = dt_s / (config->filter_tau_s + dt_s);
        state->filt.fwd += alpha * (g.fwd - state->filt.fwd);
        state->filt.side += alpha * (g.side - state->filt.side);
        state->filt.up += alpha * (g.up - state->filt.up);
    }

    /* ---------- 5. Angles ---------- */
    if (state->rot_ok)
    { /* mounting already removed by the rotation */
        state->pitch_deg = pitch_from(state->filt);
        state->roll_deg = roll_from(state->filt);
    }
    else
    { /* fallback: subtract the mounting angles */
        state->pitch_deg = wrap180(pitch_from(state->filt) - state->pitch0);
        state->roll_deg = wrap180(roll_from(state->filt) - state->roll0);
    }
    /* total tilt: angle between the car's up axis and vertical (pitch and
       roll combined).  A full-width hump shows up as pitch, a one-sided hump
       (one wheel climbs) as roll; total tilt sees both. */
    float tilt = imu_atan2f(imu_sqrtf(state->filt.fwd * state->filt.fwd +
                                      state->filt.side * state->filt.side),
                            state->filt.up) *
                 RAD2DEG;
    state->tilt_total_deg = tilt;
}

static float update_rise(imu_logic_t *state, imu_accel_t accel, float dt_s,
                         float motion_accel_g, float mag, float *up_sample)
{
    const imu_config_t *config = &state->cfg;
    /* median of the last 3 up samples: a one-sample jolt never counts */
    state->up_hist[state->up_n % 3u] = accel.up;
    { /* slow-lift input: |a| without the car's own forward acceleration
         (from the wheels), so speeding up / braking does not look like a
         lift.  Clamped: wheels spinning free in the air must not matter. */
        float forward_accel = motion_accel_g;
        if (forward_accel > 0.5f)
        {
            forward_accel = 0.5f;
        }
        if (forward_accel < -0.5f)
        {
            forward_accel = -0.5f;
        }
        float f = accel.fwd - forward_accel;
        state->mag_hist[state->up_n % 3u] =
            imu_sqrtf(f * f + accel.side * accel.side + accel.up * accel.up);
    }
    state->up_n++;
    float median_up = 1.0f;
    if (state->up_n >= 3u)
    {
        float x = state->up_hist[0], y = state->up_hist[1],
              z = state->up_hist[2];
        median_up = (x > y) ? ((y > z)   ? y
                               : (x > z) ? z
                                         : x)
                            : ((x > z)   ? x
                               : (y > z) ? z
                                         : y);
    }
    bool lifting = median_up - 1.0f > config->lift_up_g;
    state->lift_timer = lifting ? state->lift_timer + dt_s : 0.0f;

    float median_extra_g = 0.0f;
    /* slow lift: leaky sum of median-of-3 |a| - 1 (orientation-free:
       tilting the car in the hand does not change |a|; the median drops
       one-sample jolts) */
    {
        float d = 0.0f;
        if (state->up_n >= 3u)
        {
            float x = state->mag_hist[0], y = state->mag_hist[1],
                  z = state->mag_hist[2];
            d = ((x > y) ? ((y > z)   ? y
                            : (x > z) ? z
                                      : x)
                         : ((x > z)   ? x
                            : (y > z) ? z
                                      : y)) -
                1.0f;
        }
        median_extra_g = d;
        if (d > config->lift_vel_clip_g)
        {
            d = config->lift_vel_clip_g;
        }
        if (d < -config->lift_vel_clip_g)
        {
            d = -config->lift_vel_clip_g;
        }
        float k = (config->lift_vel_tau_s > dt_s)
                      ? dt_s / config->lift_vel_tau_s
                      : 1.0f;
        state->lift_vel += d * dt_s - k * state->lift_vel;
        /* vibration level (0.5 s average of the sample-to-sample change):
           a hand lift is smooth, a driving car shakes */
        float jump = imu_fabsf(mag - state->mag_prev);
        float noise_weight = (0.5f > dt_s) ? dt_s / 0.5f : 1.0f;
        if (state->up_n > 1u)
        {
            state->mag_noise += noise_weight * (jump - state->mag_noise);
        }
        state->mag_prev = mag;
    }

    *up_sample = median_up;
    return median_extra_g;
}

static void update_hump_peak(imu_logic_t *state, float dist_delta_mm,
                             float dt_s, float hump_angle)
{
    const imu_config_t *config = &state->cfg;
    float pitch = state->pitch_deg;
    float tilt = state->tilt_total_deg;
    if (state->hump_state != HUMP_FLAT)
    {
        /* height gained = sin(pitch) x distance moved (full-width method) */
        state->hump_h_mm += imu_sinf(pitch * DEG2RAD) * dist_delta_mm;
        if (state->hump_h_mm > state->pitch_peak_mm)
        {
            state->pitch_peak_mm = state->hump_h_mm;
        }
        if (pitch > state->current_hump_peak_pitch_deg)
        {
            state->current_hump_peak_pitch_deg = pitch;
        }
        if (hump_angle > state->hump_peak_angle)
        {
            state->hump_peak_angle = hump_angle;
        }
        /* Height uses the tilt only while ROLL dominates (the right wheel is
           the one lifted).  On the real track the caster/other end also
           crosses the hump afterwards, which shows as pitch: not the hump
           height under the wheel, so it is left out. */
        if ((config->hump_mode != IMU_HUMP_ONE_SIDED ||
             imu_fabsf(state->roll_deg) >= imu_fabsf(pitch)) &&
            tilt > state->current_hump_peak_tilt_deg)
        {
            state->current_hump_peak_tilt_deg = tilt;
        }
        if (imu_fabsf(state->roll_deg) > state->current_hump_peak_roll_deg)
        {
            state->current_hump_peak_roll_deg = imu_fabsf(state->roll_deg);
        }
        state->current_hump_one_sided =
            (config->hump_mode == IMU_HUMP_ONE_SIDED) ||
            state->current_hump_peak_roll_deg >
                imu_fabsf(state->current_hump_peak_pitch_deg);
        state->current_hump_peak_mm =
            state->current_hump_one_sided
                ? config->hump_lever_mm *
                      imu_sinf(state->current_hump_peak_tilt_deg * DEG2RAD)
                : state->pitch_peak_mm;
        state->state_timer += dt_s;
    }
}

static uint32_t enter_hump(imu_logic_t *state, float hump_angle,
                           float dist_delta_mm, float dt_s)
{
    const imu_config_t *config = &state->cfg;
    float pitch = state->pitch_deg;
    float tilt = state->tilt_total_deg;
    bool moving = dist_delta_mm > 0.0f;
    if (state->rearm_needed)
    {
        if (hump_angle < config->hump_exit_deg)
        {
            state->rearm_needed = false;
        }
        return 0;
    }
    /* Count pitch height from the moment the nose starts rising, so
     * the part of a full-width climb before entry is confirmed is
     * not lost. */
    state->pre_h_mm =
        (pitch > config->hump_exit_deg)
            ? state->pre_h_mm + imu_sinf(pitch * DEG2RAD) * dist_delta_mm
            : 0.0f;
    /* Only a MOVING car can be climbing a hump; parked on a slope
     * is not. */
    state->enter_timer = (hump_angle > config->hump_enter_deg && moving)
                             ? state->enter_timer + dt_s
                             : 0.0f;
    if (state->enter_timer >= config->hump_hold_s)
    {
        state->hump_state = HUMP_CLIMBING;
        state->enter_timer = state->state_timer = 0.0f;
        state->hump_h_mm = state->pre_h_mm;
        state->pitch_peak_mm = state->pre_h_mm;
        state->current_hump_peak_pitch_deg = pitch;
        state->hump_peak_angle = hump_angle;
        state->current_hump_peak_tilt_deg = tilt;
        state->current_hump_peak_roll_deg = imu_fabsf(state->roll_deg);
        state->current_hump_one_sided =
            (config->hump_mode == IMU_HUMP_ONE_SIDED) ||
            state->current_hump_peak_roll_deg > imu_fabsf(pitch);
        state->current_hump_peak_mm =
            state->current_hump_one_sided
                ? config->hump_lever_mm * imu_sinf(tilt * DEG2RAD)
                : state->pre_h_mm;
        return IMU_EVT_HUMP_ENTRY;
    }

    return 0;
}

/* ---------- 6. Hump state machine (B4-FR4, FR5) ----------
 * Detected from TOTAL tilt, so it works for both layouts:
 * "hump angle" = |roll| (one-sided mode, default) or total tilt (any mode)
 *   FLAT --hump angle > 3 deg for 0.1 s while moving--> CLIMBING
 *   CLIMBING --past the top--> DESCENDING
 *       full-width: pitch < -5 deg for 0.1 s (nose going down)
 *       one-sided:  tilt below 70 % of its peak for 0.1 s
 *   DESCENDING --hump angle < 1.5 deg for 0.1 s--> FLAT + HUMP_DONE
 * Height:
 *   full-width (pitch dominated): sum of sin(pitch) x distance moved
 *   one-sided  (roll dominated):  lever x sin(peak TOTAL tilt) - on the
 * real hump the car tips diagonally (roll + pitch)
 */
static uint32_t update_hump(imu_logic_t *state, float dist_delta_mm, float dt_s)
{
    const imu_config_t *config = &state->cfg;
    uint32_t events = 0;
    float pitch = state->pitch_deg;
    float tilt = state->tilt_total_deg;
    /* the angle the hump detector watches */
    float hump_angle = (config->hump_mode == IMU_HUMP_ONE_SIDED)
                           ? imu_fabsf(state->roll_deg)
                           : tilt;

    update_hump_peak(state, dist_delta_mm, dt_s, hump_angle);

    /* While the car is held up (lifted latch), tilting it in the hand is not
       a hump: no new hump starts and none is counted (car test 19 Nov 2026:
       a 21 mm "hump" was counted while the car was held). */
    if (state->lifted)
    {
        if (state->hump_state != HUMP_FLAT)
        {
            abort_hump(state);
        }
    }
    else
    {
        switch (state->hump_state)
        {
            case HUMP_FLAT:
                events = enter_hump(state, hump_angle, dist_delta_mm, dt_s);
                break;
            case HUMP_CLIMBING:
            {
                bool over_top =
                    state->current_hump_one_sided
                        ? (hump_angle <
                           0.7f * state->hump_peak_angle) /* one-sided: tilt
                                                             falling */
                        : (pitch <
                           -config->hump_enter_deg); /* full-width: nose down */
                state->desc_timer = over_top ? state->desc_timer + dt_s : 0.0f;
                if (state->desc_timer >= config->hump_hold_s)
                {
                    state->hump_state = HUMP_DESCENDING;
                    state->desc_timer = state->state_timer = 0.0f;
                }
                else if (state->state_timer > config->hump_timeout_s)
                {
                    abort_hump(state);
                }
                break;
            }

            case HUMP_DESCENDING:
                /* one-sided: a dip (hand wobble) then rising again = still
                 * climbing */
                if (state->current_hump_one_sided &&
                    hump_angle > 0.9f * state->hump_peak_angle)
                {
                    state->hump_state = HUMP_CLIMBING;
                    state->exit_timer = 0.0f;
                    break;
                }
                state->exit_timer = (hump_angle < config->hump_exit_deg)
                                        ? state->exit_timer + dt_s
                                        : 0.0f;
                if (state->exit_timer >= config->hump_hold_s)
                {
                    state->hump_state = HUMP_FLAT;
                    state->exit_timer = state->state_timer = 0.0f;
                    state->hump_count++;
                    if (state->current_hump_peak_mm >
                        state->highest_hump_peak_mm)
                    {
                        state->highest_hump_peak_mm =
                            state->current_hump_peak_mm;
                    }
                    events |= IMU_EVT_HUMP_DONE;
                }
                else if (state->state_timer > config->hump_timeout_s)
                {
                    abort_hump(state);
                }
                break;
            default:
                /* Discard an invalid transition without counting a completed
                 * hump. */
                abort_hump(state);
                break;
        }
    }

    return events;
}

static void update_lift_history(imu_logic_t *state, float dt_s, bool moving)
{
    const imu_config_t *config = &state->cfg;
    float tilt = state->tilt_total_deg;
    state->overturn_timer =
        (tilt > config->overturn_deg) ? state->overturn_timer + dt_s : 0.0f;
    if (state->lift_vel > state->rise_max)
    {
        state->rise_max = state->lift_vel;
    }
    { /* how long ago a rise (off the hump) and a free-spin speed were seen */
        bool rise_now = state->hump_state == HUMP_FLAT &&
                        state->lift_vel > config->rise_then_stop_g_s;
        state->rise_age = rise_now ? 0.0f : state->rise_age + dt_s;
        if (state->hump_state != HUMP_FLAT)
        {
            state->rise_age = 1e3f;
        }
        state->spin_age = (state->wheel_speed_mm_s > config->free_spin_mm_s)
                              ? 0.0f
                              : state->spin_age + dt_s;
    }
    /* resting tilt, taken once the wheels have been still for rest_settle_s.
       Wheel motion comes from the caller (dist is zeroed during hard turns
       for the hump detector, which must not count as "still" here). */
    if (moving || state->wheels_moving)
    {
        state->rest_valid = false;
        state->rest_timer = state->handle_timer = 0.0f;
    }
    else if (!state->rest_valid)
    {
        state->rest_timer += dt_s;
        if (state->rest_timer >= config->rest_settle_s)
        {
            state->rest_valid = true;
            state->rest_pitch = state->pitch_deg;
            state->rest_roll = state->roll_deg;
        }
    }
    {
        float dev = imu_fabsf(state->pitch_deg - state->rest_pitch);
        float dr = imu_fabsf(state->roll_deg - state->rest_roll);
        if (dr > dev)
        {
            dev = dr;
        }
        state->handle_timer = (state->rest_valid && dev > config->handle_deg)
                                  ? state->handle_timer + dt_s
                                  : 0.0f;
    }
}

static uint32_t update_lift(imu_logic_t *state, float dt_s, bool moving,
                            float median_extra_g, float median_up)
{
    const imu_config_t *config = &state->cfg;
    uint32_t events = 0;
    float tilt = state->tilt_total_deg;
    float mag = state->accel_mag_g;
    bool lifting = median_up - 1.0f > config->lift_up_g;
    float rest_before = state->rest_timer;
    update_lift_history(state, dt_s, moving);
    bool stationary_sample =
        !moving && !state->wheels_moving && dt_s <= SLOW_LIFT_MAX_SAMPLE_S;
    bool slow_rise_complete =
        stationary_sample && state->slow_rise_s >= SLOW_LIFT_RISE_S;
    if (stationary_sample && median_extra_g > SLOW_LIFT_PHASE_G)
    {
        if (!slow_rise_complete)
        {
            state->slow_rise_s += dt_s;
        }
    }
    else
    {
        state->slow_rise_s = 0.0f;
    }
    if (!state->lifted)
    {
        bool still = state->rest_timer >= 0.3f ||
                     state->rest_valid; /* wheels still >= 0.3 s */
        float rise_trigger =
            (still ? config->lift_vel_still_g_s : config->lift_vel_g_s) +
            config->lift_vel_noise_k * state->mag_noise;
        bool braked_rise =
            still && slow_rise_complete && median_extra_g < -SLOW_LIFT_PHASE_G;
        bool by_rise = braked_rise ||
                       state->lift_timer >= config->lift_hold_s ||
                       state->lift_vel > rise_trigger;
        bool by_handle = state->handle_timer >= config->handle_hold_s;
        /* wheels have just become still (0.3 s) after a recent rise */
        bool by_stop = rest_before < 0.3f && state->rest_timer >= 0.3f &&
                       state->rise_age < config->rise_memory_s &&
                       state->spin_age < config->rise_memory_s;
        by_handle = by_handle || by_stop;
        bool by_lift = by_rise || by_handle;
        bool by_tilt = state->overturn_timer >= config->overturn_hold_s;
        if (by_lift || by_tilt)
        {
            state->lifted = true;
            if (state->hump_state != HUMP_FLAT)
            {
                abort_hump(state); /* lifted mid-hump: not a hump */
            }
            state->lift_by_tilt = !by_lift;
            state->lift_by_handle = by_handle && !by_rise && !by_stop;
            state->lift_by_stop = by_stop && !by_rise;
            state->lift_peak_up_g = median_up - 1.0f;
            state->lift_peak_vel = state->lift_vel;
            state->clear_timer = 0.0f;
            events |= IMU_EVT_LIFTED;
        }
    }
    else
    {
        if (median_up - 1.0f > state->lift_peak_up_g)
        {
            state->lift_peak_up_g = median_up - 1.0f;
        }
        if (state->lift_vel > state->lift_peak_vel)
        {
            state->lift_peak_vel = state->lift_vel;
        }
        bool settled = tilt < config->overturn_deg - 10.0f && !lifting &&
                       imu_fabsf(mag - 1.0f) < 0.10f;
        state->clear_timer = settled ? state->clear_timer + dt_s : 0.0f;
        if (state->clear_timer >= config->lift_clear_s)
        {
            state->lifted = false;
            state->lift_timer = state->overturn_timer = state->clear_timer =
                0.0f;
            state->lift_vel = 0.0f;
            state->rest_valid = false; /* take the resting tilt again */
            state->rest_timer = state->handle_timer = 0.0f;
        }
    }

    return events;
}

static uint32_t update_tilt_event(imu_logic_t *state, float dt_s, bool moving)
{
    const imu_config_t *config = &state->cfg;
    float pitch = state->pitch_deg;
    uint32_t events = 0;
    /* ---------- 8. Tilt, only when not on a hump (B4-FR6) ---------- */
    if ((state->hump_state == HUMP_FLAT || !moving) &&
        (imu_fabsf(pitch) > config->tilt_deg ||
         imu_fabsf(state->roll_deg) > config->tilt_deg))
    {
        state->tilt_timer += dt_s;
        if (state->tilt_timer >= config->hump_hold_s && !state->tilt_active)
        {
            events |= IMU_EVT_TILT;
            state->tilt_active = true;
        }
    }
    else if (imu_fabsf(pitch) < config->tilt_deg - 5.0f &&
             imu_fabsf(state->roll_deg) < config->tilt_deg - 5.0f)
    {
        state->tilt_timer = 0.0f;
        state->tilt_active =
            false; /* re-arm once clearly back below threshold */
    }

    return events;
}

uint32_t imu_logic_update(imu_logic_t *state, imu_accel_t accel, float dt_s,
                          float dist_delta_mm, float motion_accel_g)
{
    uint32_t events;
    const imu_config_t *config = &state->cfg;

    if (dt_s <= 0.0f)
    {
        dt_s = 1e-3f;
    }
    state->t += dt_s;

    if (state->calibrated)
    {
        accel = to_car(state, accel); /* undo mounting angle, scale to 1 g */
    }
    float mag = imu_sqrtf(accel.fwd * accel.fwd + accel.side * accel.side +
                          accel.up * accel.up);
    state->accel_mag_g = mag;

    /* ---------- 1. Calibration: average while stationary (B4-FR2) ----------
     */
    if (!state->calibrated)
    {
        state->cal_fwd += accel.fwd;
        state->cal_side += accel.side;
        state->cal_up += accel.up;
        state->cal_n++;
        if (state->t >= config->calib_time_s && state->cal_n > 0)
        {
            imu_accel_t avg = {(float)(state->cal_fwd / state->cal_n),
                               (float)(state->cal_side / state->cal_n),
                               (float)(state->cal_up / state->cal_n)};
            /* Assumes the robot sits on LEVEL ground during calibration:
             * whatever angle we see now is the sensor's mounting angle. */
            state->pitch0 = pitch_from(avg);
            state->roll0 = roll_from(avg);
            build_rotation(state, avg);
            state->filt = to_car(state, avg); /* about (0, 0, 1) when rot_ok */
            state->calibrated = true;
        }
        return 0;
    }

    float up_sample;
    float median_extra_g =
        update_rise(state, accel, dt_s, motion_accel_g, mag, &up_sample);
    bool moving = dist_delta_mm > 0.0f;
    update_attitude(state, accel, dt_s, motion_accel_g);
    events = update_hump(state, dist_delta_mm, dt_s);
    events |= update_lift(state, dt_s, moving, median_extra_g, up_sample);
    events |= update_tilt_event(state, dt_s, moving);
    return events;
}

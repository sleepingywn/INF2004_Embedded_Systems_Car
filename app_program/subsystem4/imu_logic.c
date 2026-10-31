/*
 * imu_logic.c  -  Buddy 4 (IMU / Terrain) processing logic
 * Pure C + math.h only: no printf, no Pico SDK, no hardware.
 */
#include "imu_logic.h"
#include "imu_math.h"	/* own sqrt/atan2/sin: the template has no libm */
#include <string.h>

#define RAD2DEG (57.29578f)
#define DEG2RAD (0.01745329f)

void imu_logic_default_config(imu_config_t *c)
{
    c->calib_time_s           = 2.0f;
    c->filter_tau_s           = 0.15f;
    c->accel_gate_g           = 0.15f;
    c->hump_mode              = IMU_HUMP_ONE_SIDED;  /* our track: hump on the right only */
    c->hump_enter_deg         = 3.0f;    /* a 10-13 mm one-sided hump tilts only ~4-5 deg */
    c->hump_exit_deg          = 1.5f;
    c->hump_hold_s            = 0.10f;
    c->hump_timeout_s         = 10.0f;   /* a slow hand push kept the wheel on the hump ~5 s */
    c->hump_lever_mm          = 120.0f;  /* PLACEHOLDER until calibrated on the car */
    c->tilt_deg               = 25.0f;
    c->collision_g            = 0.8f;
    c->collision_refractory_s = 0.5f;
}

void imu_logic_init(imu_logic_t *s, const imu_config_t *cfg)
{
    memset(s, 0, sizeof(*s));
    s->cfg = *cfg;
    s->hump_state = HUMP_FLAT;
    s->scale = 1.0f;
}

void imu_logic_reset_run(imu_logic_t *s)
{
    s->highest_hump_peak_mm = 0.0f;
    s->current_hump_peak_mm = 0.0f;
    s->current_hump_peak_pitch_deg = 0.0f;
    s->current_hump_peak_tilt_deg = 0.0f;
    s->current_hump_peak_roll_deg = 0.0f;
    s->current_hump_one_sided = false;
    s->hump_count = 0;
}

const char *imu_hump_state_name(hump_state_t st)
{
    switch (st) {
    case HUMP_FLAT:       return "FLAT";
    case HUMP_CLIMBING:   return "CLIMBING";
    case HUMP_DESCENDING: return "DESCENDING";
    }
    return "?";
}

/* Pitch from gravity: how much of gravity points along the forward axis. */
static float pitch_from(imu_accel_t a)
{
    return imu_atan2f(a.fwd, imu_sqrtf(a.side * a.side + a.up * a.up)) * RAD2DEG;
}

static float roll_from(imu_accel_t a)
{
    return imu_atan2f(a.side, a.up) * RAD2DEG;
}

/* Keep an angle difference in -180..+180, e.g. +358 deg becomes -2 deg.
 * Needed because atan2 jumps from +180 to -180. */
static float wrap180(float d)
{
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

/* Build the sensor -> car rotation from the average reading taken while the
 * car stands still on level ground (gravity points straight down).
 *   up    = direction of the measured gravity reaction
 *   fwd   = the sensor's forward axis, tipped onto the horizontal plane
 *   side  = up x fwd  (+ = left)
 * This removes ANY mounting angle exactly (tilted, upside down...), so
 * rolling the car no longer leaks into pitch and vice versa. */
static void build_rotation(imu_logic_t *s, imu_accel_t avg)
{
    float n = imu_sqrtf(avg.fwd * avg.fwd + avg.side * avg.side + avg.up * avg.up);
    s->scale  = (n > 0.5f && n < 1.5f) ? 1.0f / n : 1.0f;
    s->rot_ok = false;
    if (n < 0.5f) return;

    float u[3] = { avg.fwd / n, avg.side / n, avg.up / n };
    float f[3] = { 1.0f - u[0] * u[0], -u[0] * u[1], -u[0] * u[2] };   /* x minus its vertical part */
    float fn = imu_sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (fn < 0.2f) return;            /* forward axis nearly vertical: mapping is wrong */
    f[0] /= fn; f[1] /= fn; f[2] /= fn;
    float sd[3] = { u[1] * f[2] - u[2] * f[1],
                    u[2] * f[0] - u[0] * f[2],
                    u[0] * f[1] - u[1] * f[0] };
    for (int i = 0; i < 3; i++) {
        s->rot[0][i] = f[i];
        s->rot[1][i] = sd[i];
        s->rot[2][i] = u[i];
    }
    s->rot_ok = true;
}

/* Sensor reading -> car frame, scaled so that rest = 1 g. */
static imu_accel_t to_car(const imu_logic_t *s, imu_accel_t a)
{
    imu_accel_t r = a;
    if (s->rot_ok) {
        float v[3] = { a.fwd, a.side, a.up };
        r.fwd  = s->rot[0][0] * v[0] + s->rot[0][1] * v[1] + s->rot[0][2] * v[2];
        r.side = s->rot[1][0] * v[0] + s->rot[1][1] * v[1] + s->rot[1][2] * v[2];
        r.up   = s->rot[2][0] * v[0] + s->rot[2][1] * v[1] + s->rot[2][2] * v[2];
    }
    r.fwd *= s->scale; r.side *= s->scale; r.up *= s->scale;
    return r;
}

static void abort_hump(imu_logic_t *s)
{
    /* Lasted too long: probably a static slope or a disturbance, not a hump. */
    s->hump_state   = HUMP_FLAT;
    s->rearm_needed = true;
    s->enter_timer = s->desc_timer = s->exit_timer = s->state_timer = 0.0f;
}

uint32_t imu_logic_update(imu_logic_t *s, imu_accel_t a, float dt_s,
                          float dist_delta_mm, float motion_accel_g)
{
    uint32_t ev = 0;
    const imu_config_t *c = &s->cfg;

    if (dt_s <= 0.0f) dt_s = 1e-3f;
    s->t += dt_s;

    if (s->calibrated) a = to_car(s, a);     /* undo mounting angle, scale to 1 g */
    float mag = imu_sqrtf(a.fwd * a.fwd + a.side * a.side + a.up * a.up);
    s->accel_mag_g = mag;

    /* ---------- 1. Calibration: average while stationary (B4-FR2) ---------- */
    if (!s->calibrated) {
        s->cal_fwd += a.fwd; s->cal_side += a.side; s->cal_up += a.up;
        s->cal_n++;
        if (s->t >= c->calib_time_s && s->cal_n > 0) {
            imu_accel_t avg = { (float)(s->cal_fwd / s->cal_n),
                                (float)(s->cal_side / s->cal_n),
                                (float)(s->cal_up / s->cal_n) };
            /* Assumes the robot sits on LEVEL ground during calibration:
             * whatever angle we see now is the sensor's mounting angle. */
            s->pitch0 = pitch_from(avg);
            s->roll0  = roll_from(avg);
            build_rotation(s, avg);
            s->filt   = to_car(s, avg);       /* about (0, 0, 1) when rot_ok */
            s->calibrated = true;
        }
        return 0;
    }

    /* ---------- 2. Collision: sudden spike in total acceleration ---------- */
    if (s->coll_cooldown > 0.0f) {
        s->coll_cooldown -= dt_s;
    } else if (imu_fabsf(mag - 1.0f) > c->collision_g) {
        ev |= IMU_EVT_COLLISION;
        s->coll_cooldown = c->collision_refractory_s;
    }

    /* ---------- 3. Remove the car's own acceleration (from encoders) ------- */
    imu_accel_t g = a;
    g.fwd -= motion_accel_g;

    /* ---------- 4. Low-pass filter, skipping obviously disturbed samples --- */
    float g_mag = imu_sqrtf(g.fwd * g.fwd + g.side * g.side + g.up * g.up);
    if (imu_fabsf(g_mag - 1.0f) <= c->accel_gate_g) {
        float alpha = dt_s / (c->filter_tau_s + dt_s);
        s->filt.fwd  += alpha * (g.fwd  - s->filt.fwd);
        s->filt.side += alpha * (g.side - s->filt.side);
        s->filt.up   += alpha * (g.up   - s->filt.up);
    }

    /* ---------- 5. Angles ---------- */
    if (s->rot_ok) {                  /* mounting already removed by the rotation */
        s->pitch_deg = pitch_from(s->filt);
        s->roll_deg  = roll_from(s->filt);
    } else {                          /* fallback: subtract the mounting angles */
        s->pitch_deg = wrap180(pitch_from(s->filt) - s->pitch0);
        s->roll_deg  = wrap180(roll_from(s->filt)  - s->roll0);
    }
    float p = s->pitch_deg;
    /* total tilt: angle between the car's up axis and vertical (pitch and
       roll combined).  A full-width hump shows up as pitch, a one-sided hump
       (one wheel climbs) as roll; total tilt sees both. */
    float tilt = imu_atan2f(imu_sqrtf(s->filt.fwd * s->filt.fwd + s->filt.side * s->filt.side),
                            s->filt.up) * RAD2DEG;
    s->tilt_total_deg = tilt;
    /* the angle the hump detector watches */
    float ha = (c->hump_mode == IMU_HUMP_ONE_SIDED) ? imu_fabsf(s->roll_deg) : tilt;

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
     *   one-sided  (roll dominated):  lever x sin(peak TOTAL tilt) - on the real
 *                                 hump the car tips diagonally (roll + pitch)
     */
    if (s->hump_state != HUMP_FLAT) {
        /* height gained = sin(pitch) x distance moved (full-width method) */
        s->hump_h_mm += imu_sinf(p * DEG2RAD) * dist_delta_mm;
        if (s->hump_h_mm > s->pitch_peak_mm)               s->pitch_peak_mm = s->hump_h_mm;
        if (p > s->current_hump_peak_pitch_deg)            s->current_hump_peak_pitch_deg = p;
        if (ha > s->hump_peak_angle)                       s->hump_peak_angle = ha;
        /* Height uses the tilt only while ROLL dominates (the right wheel is
           the one lifted).  On the real track the caster/other end also
           crosses the hump afterwards, which shows as pitch: not the hump
           height under the wheel, so it is left out. */
        if ((c->hump_mode != IMU_HUMP_ONE_SIDED || imu_fabsf(s->roll_deg) >= imu_fabsf(p))
            && tilt > s->current_hump_peak_tilt_deg)
            s->current_hump_peak_tilt_deg = tilt;
        if (imu_fabsf(s->roll_deg) > s->current_hump_peak_roll_deg)
            s->current_hump_peak_roll_deg = imu_fabsf(s->roll_deg);
        s->current_hump_one_sided = (c->hump_mode == IMU_HUMP_ONE_SIDED) ||
            s->current_hump_peak_roll_deg > imu_fabsf(s->current_hump_peak_pitch_deg);
        s->current_hump_peak_mm = s->current_hump_one_sided
            ? c->hump_lever_mm * imu_sinf(s->current_hump_peak_tilt_deg * DEG2RAD)
            : s->pitch_peak_mm;
        s->state_timer += dt_s;
    }

    bool moving = dist_delta_mm > 0.0f;

    switch (s->hump_state) {
    case HUMP_FLAT:
        if (s->rearm_needed) {
            if (ha < c->hump_exit_deg) s->rearm_needed = false;
            break;
        }
        /* Count pitch height from the moment the nose starts rising, so the
         * part of a full-width climb before entry is confirmed is not lost. */
        s->pre_h_mm = (p > c->hump_exit_deg)
                    ? s->pre_h_mm + imu_sinf(p * DEG2RAD) * dist_delta_mm : 0.0f;
        /* Only a MOVING car can be climbing a hump; parked on a slope is not. */
        s->enter_timer = (ha > c->hump_enter_deg && moving) ? s->enter_timer + dt_s : 0.0f;
        if (s->enter_timer >= c->hump_hold_s) {
            s->hump_state = HUMP_CLIMBING;
            s->enter_timer = s->state_timer = 0.0f;
            s->hump_h_mm = s->pre_h_mm;
            s->pitch_peak_mm = s->pre_h_mm;
            s->current_hump_peak_pitch_deg = p;
            s->hump_peak_angle = ha;
            s->current_hump_peak_tilt_deg = tilt;
            s->current_hump_peak_roll_deg = imu_fabsf(s->roll_deg);
            s->current_hump_one_sided = (c->hump_mode == IMU_HUMP_ONE_SIDED) ||
                s->current_hump_peak_roll_deg > imu_fabsf(p);
            s->current_hump_peak_mm = s->current_hump_one_sided
                ? c->hump_lever_mm * imu_sinf(tilt * DEG2RAD) : s->pre_h_mm;
            ev |= IMU_EVT_HUMP_ENTRY;
        }
        break;

    case HUMP_CLIMBING: {
        bool over_top = s->current_hump_one_sided
            ? (ha < 0.7f * s->hump_peak_angle)                /* one-sided: tilt falling */
            : (p < -c->hump_enter_deg);                        /* full-width: nose down   */
        s->desc_timer = over_top ? s->desc_timer + dt_s : 0.0f;
        if (s->desc_timer >= c->hump_hold_s) {
            s->hump_state = HUMP_DESCENDING;
            s->desc_timer = s->state_timer = 0.0f;
        } else if (s->state_timer > c->hump_timeout_s) {
            abort_hump(s);
        }
        break;
    }

    case HUMP_DESCENDING:
        /* one-sided: a dip (hand wobble) then rising again = still climbing */
        if (s->current_hump_one_sided && ha > 0.9f * s->hump_peak_angle) {
            s->hump_state = HUMP_CLIMBING;
            s->exit_timer = 0.0f;
            break;
        }
        s->exit_timer = (ha < c->hump_exit_deg) ? s->exit_timer + dt_s : 0.0f;
        if (s->exit_timer >= c->hump_hold_s) {
            s->hump_state = HUMP_FLAT;
            s->exit_timer = s->state_timer = 0.0f;
            s->hump_count++;
            if (s->current_hump_peak_mm > s->highest_hump_peak_mm)
                s->highest_hump_peak_mm = s->current_hump_peak_mm;
            ev |= IMU_EVT_HUMP_DONE;
        } else if (s->state_timer > c->hump_timeout_s) {
            abort_hump(s);
        }
        break;
    }

    /* ---------- 7. Tilt, only when not on a hump (B4-FR6) ---------- */
    if ((s->hump_state == HUMP_FLAT || !moving) &&
        (imu_fabsf(p) > c->tilt_deg || imu_fabsf(s->roll_deg) > c->tilt_deg)) {
        s->tilt_timer += dt_s;
        if (s->tilt_timer >= c->hump_hold_s && !s->tilt_active) {
            ev |= IMU_EVT_TILT;
            s->tilt_active = true;
        }
    } else if (imu_fabsf(p) < c->tilt_deg - 5.0f && imu_fabsf(s->roll_deg) < c->tilt_deg - 5.0f) {
        s->tilt_timer = 0.0f;
        s->tilt_active = false;   /* re-arm once clearly back below threshold */
    }

    return ev;
}

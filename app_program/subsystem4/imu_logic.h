/*
 * imu_logic.h  -  Buddy 4 (IMU / Terrain) processing logic
 *
 * This module turns accelerometer readings into terrain information:
 * pitch/roll, hump detection, hump peak height, tilt and lift/overturn events.
 *
 * It does NOT talk to any hardware. It only receives numbers.
 * That is what lets the same file run:
 *   - on a laptop with phone-recorded data   (imu_sim.c)
 *   - on the Pico with real GY-511 readings  (drv_gy511.c, terrain task)
 *
 * Units used everywhere in this file:
 *   acceleration : g   (1 g = 9.81 m/s^2)
 *   angles       : degrees, pitch positive = nose up
 *   distance     : mm
 *   time         : seconds
 */
#ifndef IMU_LOGIC_H
#define IMU_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

/* One accelerometer reading, already converted to g and to the CAR's axes.
 * (Mapping sensor axes -> car axes is done by whoever supplies the data.) */
typedef struct {
    float fwd;   /* along the direction of travel (+ = forward) */
    float side;  /* sideways (+ = left)                         */
    float up;    /* vertical (+ = up); reads about +1 g when flat */
} imu_accel_t;

/* Events returned by imu_logic_update(). Several can happen in one sample,
 * so they are bit flags.  (Mapped to the team's EVENT_* types later.) */
#define IMU_EVT_HUMP_ENTRY  (1u << 0)  /* started climbing a hump          */
#define IMU_EVT_HUMP_DONE   (1u << 1)  /* finished a hump -> "hump detected" */
#define IMU_EVT_TILT        (1u << 2)  /* abnormal tilt outside a hump      */
#define IMU_EVT_LIFTED      (1u << 3)  /* lifted off the ground or overturned:
                                          the car must stop (end of run)      */

#define IMU_HUMP_ANY        0
#define IMU_HUMP_ONE_SIDED  1

typedef enum {
    HUMP_FLAT = 0,
    HUMP_CLIMBING,
    HUMP_DESCENDING
} hump_state_t;

/* Tunable numbers. These are STARTING GUESSES - tune them with real data. */
typedef struct {
    float calib_time_s;           /* how long to stay still at start (B4-FR2)   */
    float filter_tau_s;           /* low-pass smoothing; bigger = smoother/slower */
    float accel_gate_g;           /* ignore samples where |a| is this far from 1 g */
    int   hump_mode;              /* IMU_HUMP_ONE_SIDED: hump angle = |roll|
                                     (immune to speeding up/braking, which only
                                     shows as pitch); IMU_HUMP_ANY: total tilt,
                                     for full-width or unknown layouts            */
    float hump_enter_deg;         /* hump angle above this = on a hump           */
    float hump_exit_deg;          /* hump angle below this = flat again          */
    float hump_hold_s;            /* condition must last this long (debounce)    */
    float hump_timeout_s;         /* give up if a hump phase lasts longer        */
    float hump_lever_mm;          /* one-sided hump: distance from the climbing
                                     wheel to the line the car tips about;
                                     height = lever x sin(peak tilt). CALIBRATE
                                     with a block of known height (see README). */
    float tilt_deg;               /* |pitch| or |roll| above this = tilt         */
    /* Lift / overturn (replaces collision detection, 12 Nov 2026).
     * Lifted: the car is accelerated upward. Measured on the car's up axis
     * (mounting removed) as the median of the last 3 samples, held for
     * lift_hold_s, so one- or two-sample jolts from bumps cannot trigger it.
     * Overturned: total tilt beyond overturn_deg (on its side/upside down,
     * or held at a steep angle).  STARTING VALUES - check on the car. */
    float lift_up_g;              /* up-axis median above 1 g + this = lifting    */
    float lift_hold_s;            /* ... for at least this long                   */
    float overturn_deg;           /* total tilt above this = overturned           */
    float overturn_hold_s;        /* ... for at least this long                   */
    float lift_clear_s;           /* level and still this long = back on the ground */
    /* Slow lift (18 Nov 2026): a calm hand lift stays under lift_up_g, so
     * also add up |a| - 1 g over time with a leak (time constant lift_vel_tau_s).
     * That is roughly the upward speed in g*s (x 9.81 = m/s).  Each sample is
     * clipped to +-lift_vel_clip_g, so a knock adds very little. */
    float lift_vel_g_s;           /* leaky sum above this (+ k x vibration) = lifted */
    float lift_vel_noise_k;       /* threshold rises by this x mean |a| change per
                                     sample: still ~0.005 g, motors driving ~0.05 g */
    float lift_vel_tau_s;
    float lift_vel_clip_g;
    /* Picked up while the wheels are still (19 Nov 2026): tilt moves more
     * than handle_deg away from where the car came to rest, for handle_hold_s.
     * A car stopped on a hump keeps its tilt, so it does not trigger. */
    float handle_deg;
    float lift_vel_still_g_s;
    float rise_then_stop_g_s;     /* rose this fast (off the hump) in the last
                                     rise_memory_s, then the wheels stopped = lifted */
    float rise_memory_s;
    float free_spin_mm_s;         /* ...and the wheels spun this fast (load gone) */     /* rise trigger while the wheels are still (no motor
                                     vibration): a slow lift that ends the run is
                                     caught once the wheels stop */
    float handle_hold_s;
    float rest_settle_s;          /* wheels still this long = resting tilt taken */
} imu_config_t;

typedef struct {
    /* ---- outputs other code can read ---- */
    bool         calibrated;
    float        pitch_deg;
    float        roll_deg;
    float        tilt_total_deg;            /* angle from vertical, pitch+roll combined */
    float        accel_mag_g;
    bool         lifted;                    /* latched: lifted or overturned  */
    float        lift_peak_up_g;            /* largest up-axis median while lifted (g above 1) */
    bool         lift_by_tilt;              /* true: triggered by overturn tilt */
    float        lift_vel;                  /* leaky sum of |a| - 1 (g*s)       */
    float        lift_peak_vel;             /* largest lift_vel while lifted    */
    bool         lift_by_handle;
    bool         lift_by_stop;              /* true: rose while driving, then wheels stopped */
    float        wheel_speed_mm_s;          /* set by the caller each cycle (forward speed) */
    float        rise_age, spin_age;        /* s since a rise / a free-spin speed was seen */            /* true: picked up (tilt moved, wheels still) */
    float        rise_max;                  /* largest lift_vel since the caller reset it */
    bool         rest_valid;
    bool         wheels_moving;             /* set by the caller each cycle (any wheel turning,
                                               incl. spin turns); false: use dist > 0 */
    float        rest_pitch, rest_roll, rest_timer, handle_timer;
    hump_state_t hump_state;
    float        current_hump_peak_mm;      /* peak of the latest hump       */
    float        current_hump_peak_pitch_deg;
    float        current_hump_peak_tilt_deg;   /* largest total tilt on this hump   */
    float        current_hump_peak_roll_deg;   /* largest |roll| on this hump       */
    bool         current_hump_one_sided;       /* roll dominated: one wheel climbed */
    float        highest_hump_peak_mm;      /* highest hump this run (FR5)   */
    uint32_t     hump_count;

    /* ---- internal state (don't touch from outside) ---- */
    imu_config_t cfg;
    float        t;                 /* time since init                     */
    double       cal_fwd, cal_side, cal_up;
    uint32_t     cal_n;
    float        pitch0, roll0;     /* mounting angles found in calibration (for info) */
    float        rot[3][3];         /* sensor -> car rotation found in calibration  */
    bool         rot_ok;            /* false: fall back to subtracting pitch0/roll0  */
    float        scale;             /* makes a car at rest read exactly 1.00 g       */
    imu_accel_t  filt;              /* smoothed acceleration                */
    float        enter_timer, desc_timer, exit_timer, state_timer;
    bool         rearm_needed;      /* after an abort, wait for flat        */
    float        hump_h_mm;         /* running height during a hump         */
    float        pitch_peak_mm;     /* peak of hump_h_mm (full-width method) */
    float        hump_peak_angle;   /* peak of the detector angle on this hump */
    float        pre_h_mm;          /* height gained just before entry      */
    float        tilt_timer;
    bool         tilt_active;
    float        up_hist[3];        /* last 3 up-axis samples (car frame)  */
    float        mag_hist[3];       /* last 3 |a| samples (slow lift)      */
    float        mag_prev, mag_noise;  /* vibration: mean |a| change per sample */
    uint32_t     up_n;
    float        lift_timer, overturn_timer, clear_timer;
} imu_logic_t;

void     imu_logic_default_config(imu_config_t *cfg);
void     imu_logic_init(imu_logic_t *s, const imu_config_t *cfg);

/* Call once per sample.
 *   a               : reading in g, car axes
 *   dt_s            : time since the previous sample
 *   dist_delta_mm   : distance travelled since previous sample (from P2's encoders)
 *   motion_accel_g  : forward acceleration of the car from P2's encoders
 *                     (0 if unknown). Removes the "speeding up looks like
 *                     a hill" error that an accelerometer-only IMU has.
 * Returns a combination of IMU_EVT_* flags (0 = nothing happened). */
uint32_t imu_logic_update(imu_logic_t *s, imu_accel_t a, float dt_s,
                          float dist_delta_mm, float motion_accel_g);

/* Sensor chip re-initialised after a fault: keep the calibration and the
 * run's hump results, drop the filter, hump in progress and lift latch. */
void     imu_logic_sensor_restart(imu_logic_t *s);

/* START / RESET command: new run, clear the highest hump (B4-FR10). */
void     imu_logic_reset_run(imu_logic_t *s);

const char *imu_hump_state_name(hump_state_t st);

/* ---------------------------------------------------------------------
 *  Motion classification (B4-FR11).  Fits the 3-bit motion_class field
 *  of the S4 snapshot.  When several apply, the first in this list wins:
 *  LIFTED, TILTED, ON_HUMP, then the wheel-based classes.
 * --------------------------------------------------------------------- */
typedef enum {
    IMU_MOTION_UNKNOWN = 0,   /* not calibrated, or no wheel data           */
    IMU_MOTION_STOPPED,       /* |speed| < 20 mm/s and |turn| < 15 deg/s     */
    IMU_MOTION_STRAIGHT,      /* moving (forward or back), barely turning    */
    IMU_MOTION_TURNING,       /* moving and turning >= 15 deg/s (an arc)     */
    IMU_MOTION_SPINNING,      /* turning >= 60 deg/s with |speed| < 40 mm/s  */
    IMU_MOTION_ON_HUMP,       /* hump state is CLIMBING or DESCENDING        */
    IMU_MOTION_TILTED,        /* abnormal tilt (the TILT event is active)    */
    IMU_MOTION_LIFTED         /* lifted or overturned (the LIFTED latch)     */
} imu_motion_t;

#define IMU_MOTION_STOP_MM_S    20.0f
#define IMU_MOTION_TURN_DPS     15.0f
/* hysteresis: once TURNING, stay TURNING until |turn| drops below this,
 * so a curve at ~15 deg/s does not flip TURNING <-> STRAIGHT (seen on the car) */
#define IMU_MOTION_TURN_EXIT_DPS 10.0f
#define IMU_MOTION_SPIN_DPS     60.0f
#define IMU_MOTION_SPIN_MM_S    40.0f

/* speed_mm_s / turn_dps: the car's forward speed and turn rate (+ = left)
 * from subsystem 2's encoders, ideally smoothed over ~0.2 s.
 * wheels_valid: false when there is no wheel data. */
imu_motion_t imu_classify_motion(const imu_logic_t *s, float speed_mm_s,
                                 float turn_dps, bool wheels_valid,
                                 imu_motion_t prev);
const char  *imu_motion_name(imu_motion_t m);

#endif

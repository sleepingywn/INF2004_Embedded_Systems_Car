/*
 * bench_main.c  -  Buddy 4: GY-511 bench test (plain Pico SDK, no RTOS)
 *
 * Reads the GY-511 every 20 ms, runs the unchanged imu_logic.c on it and
 * prints the results over USB serial. No car or encoders needed.
 *
 * Keys (type into the serial monitor):
 *   h  help                 s  I2C scan
 *   n  normal status view   r  raw axis view (for axis mapping)
 *   c  CSV log (copy into a .csv and replay with ./imu_sim --g file.csv)
 *   m  toggle "pretend moving" (hump detection only runs while moving)
 *   z  restart: recalibrate (keep still 2 s) and clear the run
 */
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "drv_gy511.h"
#include "imu_logic.h"

#define PERIOD_MS          20        /* same period as the planned terrain task */
#define PRINT_EVERY        5         /* status line every 5 x 20 ms = 100 ms    */
#define BENCH_SPEED_MM_S   100.0f    /* pretend speed when 'm' is on            */
#define FAULT_AFTER        5         /* consecutive failed reads -> IMU fault   */
#define REINIT_EVERY       50        /* while faulted, retry init every 1 s     */

/* ------------------------------------------------------------------------
 * AXIS MAPPING: chip X/Y/Z -> car forward / side / up.
 * Defaults assume the board lies flat, components up, with the X arrow on the
 * board pointing to the car's front. Use the 'r' view to check:
 *   flat              -> the "up" axis reads about +1 g
 *   lift the NOSE     -> the "fwd" axis goes positive
 *   lift the LEFT side-> the "side" axis goes positive
 * If an axis goes the wrong way, flip its sign; if it is the wrong axis,
 * change the axis number (0 = X, 1 = Y, 2 = Z).
 * ------------------------------------------------------------------------ */
typedef struct { uint8_t axis; int8_t sign; } axis_sel_t;
static const axis_sel_t MAP_FWD  = { 0, +1 };
static const axis_sel_t MAP_SIDE = { 1, +1 };
static const axis_sel_t MAP_UP   = { 2, +1 };

static float pick(const gy511_accel_t *r, axis_sel_t m)
{
    float v = (m.axis == 0) ? r->x : (m.axis == 1) ? r->y : r->z;
    return m.sign * v;
}

static imu_accel_t map_axes(const gy511_accel_t *r)
{
    imu_accel_t a = { pick(r, MAP_FWD), pick(r, MAP_SIDE), pick(r, MAP_UP) };
    return a;
}

typedef enum { VIEW_STATUS, VIEW_RAW, VIEW_CSV } view_t;

static void print_help(void)
{
    printf("\nKeys: h help | s I2C scan | n status | r raw axes | c CSV log\n"
           "      m toggle pretend-moving | z recalibrate + clear run\n"
           "Keep the sensor STILL and FLAT for 2 s after start or 'z' (calibration).\n\n");
}

static void print_events(uint32_t ev, const imu_logic_t *s, float t, bool csv)
{
    const char *pre = csv ? "# " : "";
    if (ev & IMU_EVT_HUMP_ENTRY)
        printf("%s[%7.2f s] HUMP ENTRY     pitch %+5.1f roll %+5.1f\n",
               pre, t, s->pitch_deg, s->roll_deg);
    if (ev & IMU_EVT_HUMP_DONE)
        printf("%s[%7.2f s] HUMP DETECTED  peak %.1f mm (peak pitch %.1f deg), "
               "highest %.1f mm, count %u\n", pre, t, s->current_hump_peak_mm,
               s->current_hump_peak_pitch_deg, s->highest_hump_peak_mm,
               (unsigned)s->hump_count);
    if (ev & IMU_EVT_TILT)
        printf("%s[%7.2f s] TILT           pitch %+5.1f roll %+5.1f\n",
               pre, t, s->pitch_deg, s->roll_deg);
    if (ev & IMU_EVT_COLLISION)
        printf("%s[%7.2f s] COLLISION      |a| %.2f g\n", pre, t, s->accel_mag_g);
}

int main(void)
{
    stdio_init_all();
    /* Wait up to 5 s for the serial monitor so the start-up messages are seen. */
    for (int i = 0; i < 50 && !stdio_usb_connected(); i++) sleep_ms(100);
    sleep_ms(200);

    printf("\n=== Buddy 4 GY-511 bench test ===\n");
    gy511_i2c_scan();

    while (!gy511_init()) {
        printf("GY-511 not responding, retrying in 2 s...\n");
        sleep_ms(2000);
        gy511_i2c_scan();
    }
    printf("GY-511 OK: %s at 0x%02X (100 Hz, +-2 g)\n",
           gy511_chip_name(), gy511_address());
    print_help();

    imu_config_t cfg; imu_logic_default_config(&cfg);
    imu_logic_t  s;   imu_logic_init(&s, &cfg);

    view_t   view = VIEW_STATUS;
    bool     moving = false, announced_cal = false, faulted = false;
    uint32_t loop_n = 0, fail_run = 0, overruns = 0, exec_max_us = 0;
    uint64_t t_start = time_us_64(), t_prev = t_start;
    absolute_time_t next = get_absolute_time();

    printf("Calibrating: keep still...\n");

    for (;;) {
        next = delayed_by_ms(next, PERIOD_MS);
        loop_n++;

        /* ---- keyboard ---- */
        int ch = getchar_timeout_us(0);
        switch (ch) {
        case 'h': print_help(); break;
        case 's': gy511_i2c_scan(); break;
        case 'n': view = VIEW_STATUS; printf("-- status view --\n"); break;
        case 'r': view = VIEW_RAW;    printf("-- raw view: chip X/Y/Z in g, then mapped car fwd/side/up --\n"); break;
        case 'c': view = VIEW_CSV;
                  printf("time_s,fwd_g,side_g,up_g,moving\n"); break;
        case 'm': moving = !moving;
                  printf("%spretend moving: %s\n", view == VIEW_CSV ? "# " : "",
                         moving ? "ON (100 mm/s)" : "OFF"); break;
        case 'z': imu_logic_init(&s, &cfg); announced_cal = false;
                  t_start = time_us_64(); exec_max_us = 0; overruns = 0;
                  printf("%srestart: keep still 2 s...\n", view == VIEW_CSV ? "# " : ""); break;
        default: break;
        }

        /* ---- one terrain-task cycle: read + process (timed) ---- */
        uint32_t t0 = time_us_32();
        gy511_accel_t raw;
        bool ok = gy511_read(&raw);
        uint64_t now = time_us_64();
        float dt = (float)(now - t_prev) * 1e-6f;
        float t  = (float)(now - t_start) * 1e-6f;
        uint32_t ev = 0;
        imu_accel_t a = {0};

        if (ok) {
            t_prev = now;
            fail_run = 0;
            if (faulted) { faulted = false; printf("IMU fault cleared\n"); }
            a  = map_axes(&raw);
            ev = imu_logic_update(&s, a, dt, moving ? BENCH_SPEED_MM_S * dt : 0.0f, 0.0f);
        } else if (++fail_run >= FAULT_AFTER) {
            if (!faulted) {
                faulted = true;
                printf("IMU FAULT: %u failed reads in a row (total errors %u)\n",
                       (unsigned)fail_run, (unsigned)gy511_error_count());
            }
            if (fail_run % REINIT_EVERY == 0) gy511_init();
        }
        uint32_t exec_us = time_us_32() - t0;
        if (s.calibrated && exec_us > exec_max_us) exec_max_us = exec_us;

        /* ---- output (not counted in the execution time above) ---- */
        if (s.calibrated && !announced_cal) {
            printf("%sCalibrated. Mounting offset pitch %.1f, roll %.1f deg\n",
                   view == VIEW_CSV ? "# " : "", s.pitch0, s.roll0);
            announced_cal = true;
        }
        print_events(ev, &s, t, view == VIEW_CSV);

        if (ok) {
            if (view == VIEW_CSV) {
                printf("%.3f,%.4f,%.4f,%.4f,%d\n", t, a.fwd, a.side, a.up, moving);
            } else if (loop_n % PRINT_EVERY == 0) {
                if (view == VIEW_RAW) {
                    float mag = sqrtf(raw.x * raw.x + raw.y * raw.y + raw.z * raw.z);
                    printf("chip X %+6.3f  Y %+6.3f  Z %+6.3f  |a| %5.3f g   ->  "
                           "fwd %+6.3f  side %+6.3f  up %+6.3f\n",
                           raw.x, raw.y, raw.z, mag, a.fwd, a.side, a.up);
                } else if (s.calibrated) {
                    printf("t %7.2f  pitch %+6.1f  roll %+6.1f  |a| %4.2f  %-10s "
                           "moving %s  peak %5.1f  highest %5.1f  humps %u  | "
                           "exec max %u us  overruns %u  i2c err %u\n",
                           t, s.pitch_deg, s.roll_deg, s.accel_mag_g,
                           imu_hump_state_name(s.hump_state), moving ? "Y" : "N",
                           s.current_hump_peak_mm, s.highest_hump_peak_mm,
                           (unsigned)s.hump_count, (unsigned)exec_max_us,
                           (unsigned)overruns, (unsigned)gy511_error_count());
                }
            }
        }

        /* ---- wait for the next 20 ms tick; count missed deadlines ---- */
        if (absolute_time_diff_us(get_absolute_time(), next) < 0) {
            overruns++;
            next = get_absolute_time();
        }
        sleep_until(next);
    }
}

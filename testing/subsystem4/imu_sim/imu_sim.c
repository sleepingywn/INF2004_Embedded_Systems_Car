/*
 * imu_sim.c  -  laptop test program for imu_logic.c
 *
 * Usage:
 *   ./imu_sim run.csv [speed_mm_s]   replay a phyphox "Acceleration with g" CSV
 *   ./imu_sim --g bench.csv          replay a GY-511 bench log (bench_main 'c' view:
 *                                    time_s,fwd_g,side_g,up_g,moving)
 *   ./imu_sim --fake                 built-in synthetic test (no phone needed)
 *   ./imu_sim --fake --nocomp        same, but WITHOUT encoder compensation
 *
 * Writes imu_out.csv (time, pitch, roll, |a|, state, height) - open it in
 * Excel / Google Sheets and plot pitch vs time.
 *
 * Phone setup assumed: lying flat, screen up, top of phone pointing forward.
 * Then: forward = phone y, side = phone x (sign flipped so + = left), up = phone z.
 */
#include "imu_logic.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define G_MS2 9.80665f

/* --- Change this if the phone / sensor is held differently --- */
static imu_accel_t map_axes(float x, float y, float z)
{
    imu_accel_t a = { y, -x, z };
    return a;
}

static FILE *out;
static float t_hump_entry;

static void report(const imu_logic_t *s, uint32_t ev, float t)
{
    if (ev & IMU_EVT_HUMP_ENTRY) {
        t_hump_entry = t;
        printf("[%7.2f s] HUMP ENTRY      pitch %+5.1f deg\n", t, s->pitch_deg);
    }
    if (ev & IMU_EVT_HUMP_DONE)
        printf("[%7.2f s] HUMP DETECTED   peak %.1f mm, peak pitch %.1f deg, "
               "duration %.2f s\n", t, s->current_hump_peak_mm,
               s->current_hump_peak_pitch_deg, t - t_hump_entry);
    if (ev & IMU_EVT_TILT)
        printf("[%7.2f s] TILT            pitch %+5.1f, roll %+5.1f deg\n",
               t, s->pitch_deg, s->roll_deg);
    if (ev & IMU_EVT_COLLISION)
        printf("[%7.2f s] COLLISION       |a| = %.2f g\n", t, s->accel_mag_g);
}

static void log_row(const imu_logic_t *s, float t)
{
    fprintf(out, "%.4f,%.2f,%.2f,%.3f,%s,%.1f\n", t, s->pitch_deg, s->roll_deg,
            s->accel_mag_g, imu_hump_state_name(s->hump_state),
            s->hump_state == HUMP_FLAT ? 0.0f : s->hump_h_mm);
}

/* ---------------- Replay a phyphox CSV ---------------- */
static int run_csv(const char *path, float speed_mm_s, bool bench_g)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 1; }

    imu_config_t cfg; imu_logic_default_config(&cfg);
    imu_logic_t s;    imu_logic_init(&s, &cfg);

    char line[512];
    float t_prev = -1.0f, t0 = 0.0f;
    long rows = 0;
    bool announced = false;

    while (fgets(line, sizeof line, f)) {
        for (char *p = line; *p; p++)                 /* tolerate ; tab and quotes */
            if (*p == ';' || *p == '\t' || *p == '"') *p = ',';
        char *p = line;
        while (*p == ',' || *p == ' ') p++;
        if (!(*p == '-' || *p == '.' || (*p >= '0' && *p <= '9'))) continue; /* header */

        float v[5]; int n = 0;
        while (n < 5 && *p) {
            char *end; v[n] = strtof(p, &end);
            if (end == p) { p++; continue; }
            n++; p = end;
        }
        if (n < 4) continue;

        float t = v[0];
        if (t_prev < 0) { t_prev = t; t0 = t; }
        float dt = t - t_prev; t_prev = t;
        if (rows == 0) dt = 0.01f;

        imu_accel_t a;
        float dist = speed_mm_s * dt;
        if (bench_g) {                   /* already car axes, in g */
            a.fwd = v[1]; a.side = v[2]; a.up = v[3];
            if (n >= 5 && v[4] == 0.0f) dist = 0.0f;   /* "moving" column */
        } else {
            a = map_axes(v[1] / G_MS2, v[2] / G_MS2, v[3] / G_MS2);
        }
        uint32_t ev = imu_logic_update(&s, a, dt, dist, 0.0f);
        rows++;

        if (s.calibrated && !announced) {
            printf("[%7.2f s] calibrated (mounting offset pitch %.1f, roll %.1f deg)\n",
                   t - t0, s.pitch0, s.roll0);
            announced = true;
        }
        report(&s, ev, t - t0);
        log_row(&s, t - t0);
    }
    fclose(f);

    if (rows == 0) {
        printf("No data rows found. Export from phyphox as CSV "
               "(comma, decimal point).\n");
        return 1;
    }
    printf("\n%ld samples (~%.0f Hz). Humps: %u. Highest hump: %.1f mm\n",
           rows, rows / (t_prev - t0 + 1e-6f), s.hump_count, s.highest_hump_peak_mm);
    printf("Note: height assumes a constant %.0f mm/s while moving "
           "(the phone has no encoder).\n", speed_mm_s);
    return 0;
}

/* ---------------- Synthetic test ---------------- */
static float noise(void) { return ((float)rand() / RAND_MAX - 0.5f) * 0.04f; }

static int run_fake(bool use_comp)
{
    const float dt = 0.02f, v = 100.0f;           /* 50 Hz, car at 100 mm/s */
    const float A = 12.0f, T = 3.0f;              /* hump: +-12 deg over 3 s */
    imu_config_t cfg; imu_logic_default_config(&cfg);
    imu_logic_t s;    imu_logic_init(&s, &cfg);
    srand(1);

    printf("Synthetic scenario (50 Hz, 100 mm/s):\n"
           "  0-2 s still | 2-4 s drive | 4-7 s hump (+-12 deg) | 7-9 s drive\n"
           "  9-9.5 s speed up 0.3 g | 9.5-10 s brake 0.3 g | 10.5 s knock\n"
           "  12-14 s parked on a 30 deg slope\n");
    printf("Encoder compensation: %s\n\n", use_comp ? "ON" : "OFF");

    float expected = 0.0f;
    for (float t = 0.0f; t < 15.0f; t += dt) {
        float pitch = 0.0f, lin = 0.0f, dist = 0.0f, knock = 0.0f;
        if (t >= 2.0f && t < 12.0f) dist = v * dt;
        if (t >= 4.0f && t < 4.0f + T)
            pitch = A * sinf(2.0f * 3.14159f * (t - 4.0f) / T);
        if (t >= 9.0f && t < 9.5f)  lin = 0.3f;
        if (t >= 9.5f && t < 10.0f) lin = -0.3f;
        if (t >= 10.5f && t < 10.5f + dt) knock = 1.5f;
        if (t >= 12.0f && t < 14.0f) { pitch = 30.0f; dist = 0.0f; }

        float pr = pitch * 0.0174533f;
        imu_accel_t a = { sinf(pr) + lin + noise() + knock,
                          noise(),
                          cosf(pr) + noise() };
        uint32_t ev = imu_logic_update(&s, a, dt, dist, use_comp ? lin : 0.0f);
        report(&s, ev, t);
        log_row(&s, t);
    }
    /* true peak height of the hump = integral of sin(pitch) * v over the climb */
    for (float t = 0.0f; t < T / 2; t += 0.001f)
        expected += sinf(A * 0.0174533f * sinf(2.0f * 3.14159f * t / T)) * v * 0.001f;

    printf("\nTrue hump height %.1f mm vs measured highest %.1f mm\n",
           expected, s.highest_hump_peak_mm);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s run.csv [speed_mm_s] | --g bench.csv [speed_mm_s]"
                        " | --fake [--nocomp]\n", argv[0]);
        return 1;
    }
    out = fopen("imu_out.csv", "w");
    if (!out) { perror("imu_out.csv"); return 1; }
    fprintf(out, "time_s,pitch_deg,roll_deg,accel_mag_g,hump_state,height_mm\n");

    int rc;
    if (strcmp(argv[1], "--fake") == 0)
        rc = run_fake(!(argc > 2 && strcmp(argv[2], "--nocomp") == 0));
    else if (strcmp(argv[1], "--g") == 0 && argc > 2)
        rc = run_csv(argv[2], argc > 3 ? strtof(argv[3], NULL) : 100.0f, true);
    else
        rc = run_csv(argv[1], argc > 2 ? strtof(argv[2], NULL) : 100.0f, false);

    fclose(out);
    printf("Wrote imu_out.csv\n");
    return rc;
}

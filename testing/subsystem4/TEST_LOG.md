# Subsystem 4 — test log

**Subsystem:** IMU / terrain monitoring (GY-511, LSM303D)  ·  **Student ID:** <student ID>  ·  **Code:** `app_program/subsystem4/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S4-U01 | `flg_terrain` and `mtx_terrain` exist | IF-06, IF-07 | auto | `subsystem4_test.c` | `[TEST] S4-U01 PASS` |
| S4-U02 | terrain_task runs every 20 ms (≥ 140 cycles in 3 s) | timing (C03) | auto | `subsystem4_test.c` | `[TEST] S4-U02 PASS` |
| S4-U03 | no overruns | timing (C03) | auto | `subsystem4_test.c` | `[TEST] S4-U03 PASS` |
| S4-U04 | GY-511 found on I2C0 (GP16/GP17) | wiring | auto | sensor connected | `[TEST] S4-U04 PASS` |
| S4-U05 | read + process < 5 ms | timing (C03) | auto | `subsystem4_test.c` | `[TEST] S4-U05 PASS`; the `S4 timing` line gives exec and jitter |
| S4-U06 | still car reads ~1 g, pitch and roll within 2° | calibration | auto | car still and level from power-up | `[TEST] S4-U06 PASS` |
| S4-U07 | snapshot keeps updating | IF-06 | auto | `subsystem4_test.c` | `[TEST] S4-U07 PASS` |
| S4-U08 | no IMU fault bit with the sensor connected | IF-07 | auto | `subsystem4_test.c` | `[TEST] S4-U08 PASS` |
| S4-U09 | tilt event | B4 tilt requirement | manual | normal image; lift the car's front past 25° and hold | console `terrain: TILT`; `S4_EVT_TILT` set |
| S4-U10 | collision event | B4 collision requirement | manual | normal image; tap the car sharply | console `terrain: COLLISION` |
| S4-U11 | stalled bus: dead sensor cannot freeze the car | fault handling (C03) | manual | normal image; pull SDA while running, then reconnect | `IMU FAULT` within ~100 ms, other tasks keep printing, `overruns 0`; `IMU fault cleared` after reconnecting |
| S4-U12 | hump detected and height reported | FR5 | manual | bench `m` key or the PRETEND_MOVING build (until encoder data exists); push the car's right wheel over the hump | `HUMP ENTRY` then `HUMP DETECTED` with a peak; compare with the block |
| S4-U13 | own maths matches the C library | imu_math.h | auto (laptop) | `imu_sim/math_check.c` | `PASS` |
| S4-U14 | logic regression (synthetic hump, knock, slope) | imu_logic.c | auto (laptop) | `imu_sim --fake` | hump 18.8 mm vs 19.9 mm true; COLLISION; TILT on the slope |
| S4-U15 | minimum-bit snapshot: size, round trip, saturation, wrap | change requirement, C06 | auto (laptop) | `imu_sim/pack_check.c` | `PASS`; 8 bytes; error ≤ half a step |
| S4-U16 | packed snapshot on target: 8 bytes, saturates, 15-bit age | change requirement, C06 | auto | `subsystem4_test.c` | `[TEST] S4-U16 PASS` |
| S4-U17 | one-sided right-side hump (10–13 mm) detected from roll; height = lever × sin(peak tilt); no false hump from speeding up/braking | FR5, one-sided layout | auto (laptop) | `imu_sim --fake1 10`, `11.5`, `13` | `HUMP DETECTED (one-sided)` within ±10 mm; no hump at the 0.15 g speed-up/brake |
| S4-U18 | lever calibration on the car | FR5 | manual | right wheel on the hump (height h), car still; read total tilt θ; lever = h / sin(θ) | lever recorded; then S4-U12 on the real hump within ±10 mm |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| 2026-10-28 | S4-U13 | (this commit) | Claude, for Andy | PASS | laptop | sqrt rel. error 1.1e-7, atan2 1.9e-6 rad, sin 7.4e-7 |
| 2026-10-28 | S4-U14 | (this commit) | Claude, for Andy | PASS | laptop | same results as before the maths change |
| 2026-10-28 | S4-U15 | (this commit) | Claude, for Andy | PASS | laptop | 8 bytes (was 38); worst error 0.25°, 1°/s, 0.5 mm |
| 2026-10-31 | S4-U17 | (this commit) | Claude, for Andy | PASS | laptop | 10→9.3, 11.5→10.7, 13→12.1 mm (lever 120 mm placeholder); no false hump at 0.15 g speed-up/brake; full-width (mode ANY) 18.9 vs 19.9 mm |
| 2026-10-31 | S4-U12 | c909cf0 (bench firmware) | Andy | PASS (detection) | `logs/2026-10-31_A_bench_car.txt` | Real right-side hump (10–13 mm), hand push with `m`: 4 humps counted, peaks 11.4–13.9 mm; one slow pass (~5 s on the hump) was cancelled by the 5 s timeout → fixed (10 s, dip tolerance) |
| 2026-10-31 | S4-U12 | (this commit) | Claude, for Andy | PASS (replay) | `logs/2026-10-31_A8_hump_pass.csv` + `logs/2026-10-31_A_bench_car.txt` | Real passes replayed through the fixed logic: CSV pass 12.7 mm, cancelled pass now detected 14.6 mm (true 10–13 mm, lever still 120 mm placeholder) |
| 2026-10-31 | S4-U09 | c909cf0 (bench firmware) | Andy | NOT RUN | `logs/2026-10-31_A_bench_car.txt` | Front only lifted to ~11°, below the 25° threshold; re-test lifting further |
| 2026-10-31 | S4-U10 | c909cf0 (bench firmware) | Andy | FAIL | `logs/2026-10-31_A_bench_car.txt` | Finger tap reached only 0.45 g, below the 0.8 g threshold; re-test with a realistic knock (car pushed into a book/wall) |
| 2026-10-31 | S4-U09 | 5cfbb0a (bench firmware) | Andy | PASS | `logs/2026-10-31_A_bench_tilt_collision_calibration.txt` | Front lifted to +29° → `TILT` at 21.52 s |
| 2026-10-31 | S4-U10 | 5cfbb0a (bench firmware) | Andy | PASS | `logs/2026-10-31_A_bench_tilt_collision_calibration.txt` | Firm knock → `COLLISION` at 1.93 g |
| 2026-10-31 | S4-U18 | 5cfbb0a (bench firmware) | Andy | PASS | `logs/2026-10-31_A_bench_tilt_collision_calibration.txt` | Right wheel on the hump (~11.5 mm), still ~8 s: roll −5.4°, pitch −4.1°, total tilt 6.75° → lever = 11.5 / sin(6.75°) ≈ 98 mm (set in this commit) |
| 2026-10-31 | S4-U12 | (this commit) | Claude, for Andy | PASS (replay) | `logs/2026-10-31_A8_hump_pass.csv` + `logs/2026-10-31_A_bench_car.txt` | With lever 98 mm: real passes read 10.4 mm and 11.9 mm (hump 10–13 mm) |
| 2026-10-31 | S4-U05 | 1e3d802 (kernel, no core pin) | Andy | FAIL | `logs/2026-10-31_B_kernel_first_run.txt` | First run under µT-Kernel: exec max 20351 µs, overruns 4–5, `i2c err 0` |
| 2026-10-31 | S4-U11 | 1e3d802 + diag (kernel) | Andy | PASS | `logs/2026-10-31_B_kernel_diag_sda_pull.txt` | SDA pulled → `IMU FAULT (5 failed reads in a row)`, reconnected → `IMU fault cleared` (came back at 0x1e: SA0 floating, tie it to 3.3 V) |
| 2026-10-31 | S4-U05 | 1e3d802 + diag, task on core 0 | Andy | FAIL | `logs/2026-10-31_B_kernel_core0.txt` | Timing breakdown: read 10–11.7 ms every cycle (logic ~0.6 ms, print ~0.6 ms), overruns 6 → the I2C read is the slow part, not the logic |
| 2026-10-31 | S4-U05 | (this commit), task on core 1 | Andy | PASS | `logs/2026-10-31_B_kernel_core1.txt` | exec max 1349 µs (read 758, logic 631), slow 0, jitter max 321 µs, overruns 0. Cause: the I2C "done" interrupt is handled on core 1; a task on core 0 only woke at the next 10 ms tick. Fix: `S4_CORE = TP_PRC2` |
| 2026-10-31 | S4-U03 | (this commit), task on core 1 | Andy | PASS (status line) | `logs/2026-10-31_B_kernel_core1.txt` | overruns 0 over the run; the auto `[TEST] S4-U03` check still to run with the TEST image |
| 2026-10-31 | S4-U12 | a3a6e95 (kernel, PRETEND_MOVING, core 1) | Andy | PASS | `logs/2026-10-31_C_kernel_hump.txt` | Real right-side hump (10–13 mm) under µT-Kernel: 3 passes, 3 humps counted, 11.3 / 10.3 / 10.5 mm; no false hump on flat; exec max 1425 µs, overruns 0, i2c err 0 |

## Bench evidence (before integration)

Recorded with the standalone bench firmware (`imu_bench/`, plain Pico SDK)
on the Maker Pi Pico, before this code was in the team repo, so there is no
commit to quote. Kept as evidence that the sensor, wiring and logic work.

| Date | What | Result | Log |
|---|---|---|---|
| 2026-10-22 | I2C scan, chip ID | LSM303D at 0x1D (`0x0F = 0x49`), `i2c err 0` | `logs/2026-10-22_bench_axes_tilt.txt` |
| 2026-10-22 | Axis directions | front up +45°, front down −29°, right wheel up −33°, left wheel up +35° | `logs/2026-10-22_bench_axes_tilt.txt` |
| 2026-10-22 | Tilt event | `TILT` fired at each tilt past 25° | both logs |
| 2026-10-22 | Timing (bench loop) | exec max 365–378 µs per 20 ms, `overruns 0` | both logs |

Not yet run on hardware: the rotation-based mounting calibration (it removes
the false pitch of 4–5° seen when rolling), collision, hump and stalled-bus
tests, and anything under micro T-Kernel (now run: see the results above).

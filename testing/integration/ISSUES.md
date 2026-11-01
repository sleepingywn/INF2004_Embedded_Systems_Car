# Integration branch `integration/s2-s3-s4`: fixes and open issues

Status after the first runs of Buddy 2 (motion), Buddy 3 (IR) and Buddy 4
(IMU / terrain) together on the car, 31 Oct 2026. Evidence for every item is
in `TEST_LOG.md` (this folder), `../subsystem4/TEST_LOG.md` and `logs/`.

Commit hashes refer to this branch.

---

## 1. Fixed

| # | Problem found on the car | Fix | Where | Commit |
|---|---|---|---|---|
| F1 | S2/S3 did not compile: `<string.h>` brings newlib's `size_t`, which clashes with the kernel's `size_t` in `tk/syslib.h` | use the compiler built-ins for `memset` / `memcpy` / `memcmp` | `motion.c`, `motion_ir_bridge.c`, `motion_logic_cases.h`, `ir_runtime.c` | `6e59744` |
| F2 | Motion latched `HANDOFF` and IR dropped to `FAULT` right after `READY`: both locked their mutex with a single `TMO_POL` try, which fails whenever another task holds it for a few µs | `lock_mailbox()` / `lock_runtime()`: retry the poll up to 50 times, then wait at most 2 ms | `motion.c`, `ir_runtime.c` | `9cce050` |
| F3 | IR stopped after ~5 min of running: the 15-frame ring overflowed (`flt 8`) when the IR task was held up > 15 ms | ring 16 → 64 frames (63 ms); the stale-frame limit follows the ring size | `ir_acquisition.c/.h` | `3904da9` |
| F4 | S1/S2 TEST results never appeared: output before the USB serial monitor connects is lost | 5 s wait before the TEST run | `app_main.c` | `52cb01c` |
| F5 | No keyboard input on the USB-only build: the USB console threw received bytes away | 64-byte receive ring + `tm_usb_console_getc()` (non-blocking) | `lib/libtm/.../usb_console.c`, `usb_tinyusb_glue.c` | `5668332` |
| F6 | Motor/encoder directions and wheel geometry were 0 placeholders | measured: left motor −1, right +1, left encoder −1, right +1; 65 mm wheels; 125 mm track; encoders enabled (0 invalid edges) | `motion_config.h` | `d57b8ca` |
| F7 | S4 used a fake speed; a hump could not be detected with real driving | S4 reads S2's raw encoder counts (IF-09): distance, speed, turn rate, forward acceleration | `task_terrain.c` | `e7a5808`, `fa6bb4f`, `93a2eca` |
| F8 | Spinning on the spot produced a false 12 mm hump (turning pushes the IMU sideways = fake roll) | no hump detection above 60 °/s turn rate (derivation in the code); disturbed-sample filter 0.15 g → 0.10 g | `task_terrain.c`, `imu_logic.c` | `4de3c0b` |

---

## 2. Temporary fixes and test-only settings (decide before the final build)

| # | Setting | Value now (was) | Why it is temporary |
|---|---|---|---|
| T1 | `IR_ACQ_MAX_JITTER_US` (`ir_acquisition.h`) | **800 µs** (100) | 100 µs tripped within a second; 500 µs nearly tripped (scan 490 µs seen). The cause of 200–500 µs interrupt latency on core 0 is **not found**. Raising the limit hides it; it does not fix it |
| T2 | `MOTION_BENCH_DUTY_MAX` / `MOTION_BENCH_TIME_MAX_MS` (`motion_config.h`) | **600 ‰ / 4000 ms** (200 / 500) | only so the console can drive the car with `g`. After each pulse the motors also rest for `MOTION_BENCH_TIME_MAX_MS` (now 4 s) |
| T3 | `MOTION_LEFT/RIGHT_COUNTS_REV` (`motion_config.h`) | **2774** (0) | rough: one hand turn of each wheel gave 2679 L / 2869 R. A 1 m push test should replace it |
| T4 | `S4_TURN_SCALE` (`task_terrain.c`) | **1.14** | corrects the turn rate (read ~12 % low). Probably compensating for T3; re-check after the 1 m push test |
| T5 | Console drive trim (`int_console.c`, `<` `>`) | right motor **+20 %** | open-loop only; the car still drifts left or right from run to run (see O-B2-1) |
| T6 | Integration console (`app_program/int_console/`) | on (`APP_CONSOLE 1`) | test tool. `APP_CONSOLE 0` removes it. Its `o` key clears the IR fault so the motors can be tested without IR |
| T7 | `motion_debug_line()` / `motion_debug_barcode()` (`motion.c/.h`) | added | test accessors for the console; not part of the agreed interface |

---

## 3. Open issues: Buddy 2 (motion)

| # | Issue | Evidence | Suggested next step |
|---|---|---|---|
| O-B2-1 | **Speed control is not tuned.** `MOTION_CALIBRATED 0`; speeds, PID gains, duty limit, stall/age/manoeuvre timeouts and line/straight gains are still 0, so VELOCITY, DISTANCE, TURN and FOLLOW are all refused. Only fixed-power bench pulses work, and those cannot drive straight: at equal power the left wheel went 1.18–1.23× further | `logs/2026-10-31_I12_drive_drift.txt` | fill the remaining `motion_config.h` values (start: 15 % ≈ 75 mm/s, 35 % ≈ 230 mm/s on the mat), tune the wheel PID lifted, then on the floor, then the straight correction |
| O-B2-2 | Motion latches a `TIMEOUT` fault at every boot (one update > 100 ms late during start-up), plus `EXTERNAL` from IR not yet calibrated. An explicit RESET is needed after every boot | every console log | ignore the update timeout for the first ~1–2 s after `motion_task` starts |
| O-B2-3 | Line following, distance moves, turns and barcode manoeuvres have **never run** | – | after O-B2-1 |
| O-B2-4 | Barcode decoder (`motion_barcode_decode`) only tested on synthetic data (S2-U06) | – | real barcode test (O-B3-4) |
| O-B2-5 | Counts per wheel turn are rough (T3) | `logs/2026-10-31_I5_encoder_hand_turn.txt` | push the car exactly 1 m and read both encoders |
| O-B2-6 | Review the changes made to Buddy 2's files on this branch: F1, F2, F6, T2, T3, T7 | – | Buddy 2 |

---

## 4. Open issues: Buddy 3 (IR)

| # | Issue | Evidence | Suggested next step |
|---|---|---|---|
| O-B3-1 | **Interrupt latency on core 0 of 200–500 µs** makes the 1 kHz sampling marginal (T1) | `max_lateness_us`, `max_scan_us` in every console log | find what disables interrupts on core 0 (USB, Wi-Fi, kernel critical sections); or move IR to core 1 and re-measure |
| O-B3-2 | IR acquisition stops at every boot (`flt 8`, ring overflow ~0.6 s after power-on: the IR task is held up > 64 ms, probably Wi-Fi start-up) | every boot log | harmless today (calibration `c` restarts it); could start acquisition after the start-up stall |
| O-B3-3 | OUTSIDE line mode: if the line passes completely outside both sensors, both read white and the state is `ON`, err 0 (centred) for up to 2 s before `LOST`. During following an overshoot looks like being on track | `logs/2026-10-31_I11_line_position.txt` | remember the side the line was last seen and keep steering that way |
| O-B3-4 | **Barcode reading never tested on a real barcode.** The open-loop drive curved and the barcode sensor missed it | `logs/2026-10-31_I12_drive_drift.txt` | push the car by hand at a steady speed over a barcode (console prints the decode), then repeat with speed control |
| O-B3-5 | Junction (`BOTH_BLACK`) is detected but nothing acts on it | `logs/2026-10-31_I12_junction.txt` | define the junction behaviour with the Vehicle Controller |
| O-B3-6 | Calibration is RAM-only: `c` … `f` after every boot (B3-FR2: calibrate before autonomous operation) | – | decided to keep for now |
| O-B3-7 | S3-U04 (GP28 stuck) from 27 Oct is no longer reproduced: GP28 calibrates with contrast ~3200 | `logs/2026-10-31_I12_junction.txt` | re-run S3-U04 and record a PASS |
| O-B3-8 | Review the changes made to Buddy 3's runtime on this branch: F1, F2, F3, T1 (the runtime files were written by Buddy 2) | – | Buddy 3 / Buddy 2 |

---

## 5. Not done yet (whole car)

| # | Item | Owner |
|---|---|---|
| N1 | **Vehicle Controller**: nothing decides start / follow / barcode turn / hump / stop; the console stood in for it | team |
| N2 | Buddy 1 MQTT / telemetry is not in this firmware | Buddy 1 |
| N3 | S4 → S2: stop on collision or tilt (`motion_stop`, `motion_set_fault(MOTION_HEALTH_TERRAIN)`) is not wired | Buddy 4 with the Vehicle Controller |
| N4 | ~~B4-FR9: each terrain event needs its own timestamp~~ **done** `bd8421a` (`s4_get_events()`, S4-U23) | Buddy 4 |
| N5 | ~~B4-FR10: public `s4_reset_run()`~~ **done** `bd8421a` (S4-U22). The Vehicle Controller still has to call it on START/RESET | Buddy 4 |
| N6 | ~~B4-FR11: motion classification~~ **done** `bd8421a` (S4-U21, S4-U24). SPINNING and IMPACT not yet seen on the car (USB cable in the way) | Buddy 4 |
| N7 | GY-511 SA0 pin to 3.3 V so the I2C address cannot change on reconnect | Buddy 4 |
| N8 | Interface numbers IF-06 … IF-08 used by both S2/S3 and S4 (`TEAM.md` note); team to renumber | team |

# System — test log

Tests of the complete product against its requirements, run on the final hardware set-up. Run the full set before every demo and before hand-in.

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| SYS-01 | power-up gives zero motor output | FR1 | manual + video | 1. Car on the track, USB/battery on. 2. Wait 10 s, do not press GP20. 3. Console `s`. | wheels never move; `MOTION IDLE`, duty L +0 R +0 |
| SYS-02 | GP20 starts and stops a run | FR1, FR9 | manual + video | 1. Press and release GP20. 2. After about 5 s press GP20 again. 3. Repeat 5 times. | starts only after release; stops on the second press, duty 0; 5/5 |
| SYS-03 | full circuit line following, incl. tight curves | FR2 | manual + video | 1. Start at the start mark with GP20. 2. Let it run the whole circuit. 3. Repeat 5 times; note where it fails. | completes the circuit without leaving the line; record x/5 and the failing curve |
| SYS-04 | line lost -> bounded recovery, then stop | FR2 | manual + video | 1. During a straight run, lift the car 2 cm so both IR sensors see no line, or slide a white sheet under them. | car stops by itself (IR fault / line lost), duty 0; GP20 retry offered |
| SYS-05 | barcode decode for every letter used | FR3 | manual + video | 1. Run over each barcode used (A, B, C, D, Z) 5 times each. 2. Save the `[route] SCAN ... -> X` lines. | right letter each time, never a wrong letter; record x/5 per letter |
| SYS-06 | junction without a barcode is crossed straight | FR3 | manual + video | 1. Drive to a junction with no barcode before it. | crosses straight, keeps line following |
| SYS-07 | left arc, right arc, U-turn return to line following | FR4 | manual + video | 1. Set mappings so A = right, B = left, D = U-turn (defaults). 2. Run each 5 times. | turn completes and V5 resumes on the new branch; x/5 per action |
| SYS-08 | straight action distance | FR4 | manual + photo | 1. Map a letter to straight (C). 2. Mark the start, run, measure the travelled distance with a ruler. 3. 5 times. | about 100 mm (record each; calibration is provisional) |
| SYS-09 | turn exit not found -> stop | FR4 | manual + video | 1. Cover the exit branch of a turn with white paper. 2. Run the turn. | car stops within the turn limit (arc 6 s, pivot 12 s), console reason 16 or 13 |
| SYS-10 | hump detected and height reported | FR5 | manual + video | 1. Measure the hump height with a ruler. 2. Drive over it 5 times. 3. Save `terrain: HUMP` lines. | 5/5 humps detected, height within 3 mm of the ruler |
| SYS-11 | lift while driving stops the motors | FR5, FR9 | manual + video | 1. Start a run. 2. Lift the car by hand. 3. 10 times. | `terrain: LIFTED`, motors stop; x/10 |
| SYS-12 | pick-up while stopped is detected | FR5 | manual + video | 1. Car still, wheels stopped. 2. Pick it up. 3. 10 times. | `terrain: LIFTED` each time; x/10 |
| SYS-13 | tilt over 45 degrees / overturn stops the motors | FR5, FR9 | manual + video | 1. Start a run. 2. Tilt the car past 45 degrees. 3. 5 times. | `TILT` then `LIFTED`, motors stop; x/5 |
| SYS-14 | IMU failure -> terrain invalid, recovery | FR5 | manual + video | Do last. 1. Car stopped. 2. Unplug the IMU SDA wire for 2 s, plug it back. | `terrain: IMU FAULT`, dashboard terrain shows unavailable; recovers after re-plug and recalibration |
| SYS-15 | dashboard shows live motion, line, barcode and terrain data | FR6 | manual + screen recording | 1. Wi-Fi/MQTT build, dashboard open. 2. Do one run over a barcode and the hump. | speeds, line error, barcode letter, pitch/roll, hump count and highest hump update during the run |
| SYS-16 | dashboard marks invalid data as unavailable | FR6 | manual + screen recording | 1. During SYS-14 (IMU unplugged) watch the dashboard. | terrain fields show "—", not old values |
| SYS-17 | remote start and STOP from the dashboard; GP20 still stops | FR7 | manual + video | 1. Start from the dashboard. 2. Stop from the dashboard. 3. Start again, stop with GP20. | each command acts once; GP20 stops a remote run |
| SYS-18 | lost link stops a remote run | FR7, NFR3 | manual + video with timer | 1. Start a remote run. 2. Turn off the laptop Wi-Fi (or stop Mosquitto) and start a stopwatch. | car stops about 5 s after the last keepalive |
| SYS-19 | route mapping edit persists | FR8 | manual + video | 1. Change A to left on the dashboard after a stopped run. 2. Power-cycle the car. 3. Run over A. | car turns left after the power cycle |
| SYS-20 | restart after STOP needs a fresh command | FR9 | manual + video | 1. Stop a run with GP20. 2. Press GP20 once. 3. Release, press and release again. | first press only prints `retry ready`; second press starts |
| SYS-21 | task timing with Wi-Fi on | NFR1 | manual | 1. Wi-Fi/MQTT build. 2. Run 2 minutes. 3. Console S4 report and IR status. | terrain exec max < 5 ms of 20 ms, overruns 0; IR late << 1 ms |
| SYS-22 | host checks for timing limits, commands and packed formats | NFR2, NFR3, NFR4 | auto | `python testing/subsystem2/run_route_tests.py`, `run_regression_tests.py`, `node --test tools/dashboard/*.test.js`, firmware build (static asserts) | all pass; build succeeds |
| SYS-23 | heading from the wheel encoders (S4 turn rate / `hdg`) | S4 terrain data (Buddy 4; not an FR) | manual + video | 1. Car still, square to a straight edge, motors off. 2. Note `hdg`. 3. Rotate the car by hand 90 deg left on the spot, wheels rolling. 4. Rotate it 90 deg back. | `hdg` rises by 90 +/- 10 % (81-99), sign + for left; back within a few deg of the start |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| 2026-11-25 | SYS-10 | `eab03b8` | Andy | PASS | `logs/2026-11-25_SYS-10_hump_video.txt`, video: (link to add) | 10 mm hump (ruler on video). One pass on video: `HUMP DETECTED (one-sided) peak mm +9.9`, count 4 (passes 1-3 earlier in the same session, highest 11.0 mm). V5 drove on over the hump without stopping. Repeatability: 21 car passes on 25 Nov read 9.7-11.7 mm, see `../subsystem4/TEST_LOG.md` S4-U12 |
| 2026-11-25 | SYS-11 | `eab03b8` | Andy | PASS | `logs/2026-11-25_SYS-11_lift_driving_video.txt`, video: (link to add) | One lift on video during a V5 run (v 27-31 mm/s): `terrain: LIFTED up +263 mg rise 187 mm/s -> motors stopped`, motion STRAIGHT -> LIFTED, v 0 after; latch cleared to STOPPED once set down level; GP20 retry offered, no auto-restart. Repeatability: 6/6 lifts while driving on 25 Nov, see `../subsystem4/TEST_LOG.md` S4-U10 |
| 2026-11-25 | SYS-13 | `eab03b8` | Andy | PASS (car stationary) | `logs/2026-11-25_SYS-13_overturn_stationary_video.txt`, video: (link to add) | Car rolled slowly by hand onto its side, motors off: `TILT pitch -9.4 roll +28.2` (motion TILTED), still TILTED at 42.8 deg, then `OVERTURNED tilt +46.1 -> motors stopped` just past the 45 deg limit. Done with the car still because a running car cannot be tipped by hand without lifting it first, which SYS-11 covers. Mid-run tilt on 19 Nov (28-30 deg): TILT then LIFTED, motors stopped |
| 2026-11-25 | SYS-23 | `eab03b8` | Andy | PASS | `logs/2026-11-25_SYS-23_heading_hand_turn_video.txt`, video: (link to add) | Hand turn 90 deg left: `hdg` +0 -> +82 deg (turn +7..+16 dps), -9 %, inside the 10 % limit, sign + for left. Turned by hand, so the angle is only about 90 deg and wheel scrub when pressed down lowers the reading; S4_TURN_SCALE 1.14 kept (calibrated from a motor spin, S4-U19, 31 Oct) |

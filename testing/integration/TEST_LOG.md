# Integration — test log

Tests that two or more subsystems work together. There is one test (or more) for every interface listed in TEAM.md section 5. Both owners agree on the test and sign off the result.

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| INT-01 | samples flow from S2 to S3 (≥ 5 in 2 s) | IF-01, IF-02 | auto | `subsystem3_test.c` | `[TEST] INT-01 PASS` |
| INT-02 | summary lines flow from S3 to S4 (≥ 2 in 3.5 s) | IF-03 | auto | `subsystem4_test.c` | `[TEST] INT-02 PASS` |
| INT-03 | motion request admission, busy/expiry/renewal | IF-06 | planned harness | S2-U07/U12 matrix, two producer tasks, reused ids and epochs | one admitted request; rejected/expired work cannot drive |
| INT-04 | stop/fault handoff under full slot and reset races | IF-07 | planned instrumented test | S2-U12/U13, assert each health source while commands/reset compete | stop is independent, asserted fault retained, no replay; measure latency |
| INT-05 | S3 sample/line/barcode to B2 manoeuvre | IF-08 | planned synthetic then hardware | S2-U14; gaps, invalid/stale/duplicate messages, positive/negative line error, A-D | correct steering/decode; malformed data does not steer; new post-turn line required |
| INT-06 | B2 wheel snapshot to B4/B1 | IF-09 | planned concurrent readers | S2-U15; resets, count/time wrap, stale samples and lock contention | coherent units/time/epoch; consumers reject invalid inputs |
| INT-12 | live line position and junction from S3 on the car | B3-FR3, B3-FR5 | manual | console `l`; slide the car across the line, then put both line sensors on a crossing line | err sign and size follow the line; EDGE_ONLY on full black; BOTH_BLACK with both sensors on black |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | INT-01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
| 2026-10-31 | INT-01 | combined S2+S3+S4 TEST build (this branch) | Andy | PASS | `logs/2026-10-31_I1b_combined_TEST.txt` | All subsystems in one image: S1 2, S2 8, S3 2, S4 9, S5 1 passed, 0 failed; S4 exec max 1557 µs, overruns 0 |
| 2026-10-31 | INT-07 | combined build + console (before lock fix) | Andy | FAIL | `logs/2026-10-31_I2_console_reset_refused.txt`, `logs/2026-10-31_I4_console_handoff_fault.txt` | At boot IR acquisition stops itself (late 217-262 µs > 100 µs limit; or ring overflow, flt 8, during boot); motion boots with `timeout external` (cleared by reset). After reset, motion latched `handoff` whenever a status reader held the mutex → fixed by the lock retry commit |
| 2026-10-31 | INT-07 | combined build + console, lock fix, 500 µs IR limit | Andy | PASS (manual) | `logs/2026-10-31_I5_console_motors_ir.txt` | IR BEGIN/FINISH: contrast GP26 1897, GP27 2785, GP28 2948 → READY ok, acquisition stayed on (max late 468 µs); motion IDLE, no faults; BEGIN stops motion as designed (`external` until reset). IR still faults during boot: calibrate after every boot |
| 2026-10-31 | INT-06 | combined build + console | Andy | PASS (manual) | `logs/2026-10-31_I5_console_motors_ir.txt`, `logs/2026-10-31_I5_encoder_hand_turn.txt` | Raw counts reach motion_get_status: +duty left → left wheel backward, enc L +998/pulse; +duty right → right wheel forward, enc R +956; one hand turn forward: L −2679, R +2869; 0 invalid edges. Signs and geometry recorded in motion_config.h |
| 2026-10-31 | INT-12 | I11 build (`0a2d13f` without the ring change) | Andy | PASS | `logs/2026-10-31_I11_line_position.txt` | Line between sensors: err 0 ON; right sensor on the line: err -48 to -93 (EDGE_ONLY at R 94); left sensor: +31 to +67; LOST after 2 s off the line. Weakness: a line that passes outside both sensors reads as centred (ON, 0) until LOST |
| 2026-10-31 | INT-12 | I12 build (`3904da9`, 500 us limit) | Andy | PASS | `logs/2026-10-31_I12_junction.txt` | BOTH_BLACK 4 times with both sensors on a crossing line (L 99-100, R 80-86); line crossings by the barcode sensor did not produce a barcode (< 5 elements dropped) |
| 2026-10-31 | INT-07 | I11 build | Andy | FAIL (fixed) | `logs/2026-10-31_I11_ir_overflow.txt` | IR stopped after ~5 min: ring overflow (flt 8). Ring 16 -> 64 frames |
| 2026-10-31 | INT-05 | I12 build, console g 15 % | Andy | NOT RUN | `logs/2026-10-31_I12_drive_drift.txt` | Barcode test not possible: open-loop drive curves (left wheel 1.18-1.23x further at equal power), the barcode sensor missed the barcode. Needs Buddy 2's speed control or a hand push |
| 2026-10-31 | INT-06 | `5f2ad8b` (console `g` drive, S4 on wheel counts) | Andy | PASS | `logs/2026-10-31_I6_drive_over_hump.txt`, `logs/2026-10-31_I6_drive_hump_35pct.txt` | Car driven over the real right-side hump (10-13 mm) by the motors; S4 distance from S2 encoder counts. 15 %, ~75 mm/s: 13.1 mm, 10.8 mm; 35 %, ~236 mm/s: 9.4 mm. Two earlier runs drove off the end of the raised mat (fall: COLLISION 1.8 g, false 25 mm "hump"); not hump results. S4 exec max 2417 µs, overruns 0 |

2026-10-29 preparation: INT-03..INT-06 are **NOT RUN**. Owner review, calibration,
live S3 runtime validation and the user's test/flash approval remain prerequisites
for the relevant powered cases. No existing result was changed.

## Live S3 runtime validation plan (NOT RUN)

| ID | Case | Expected result |
|---|---|---|
| INT-07 | Startup / calibration / explicit B2 reset | No movement at boot; IR fault remains until valid three-channel calibration and fresh handoff; only a later explicit reset plus new command can move |
| INT-08 | Stop/reset while a barcode is buffered or partly captured | New epoch discards old frames and partial barcode; no old turn is replayed |
| INT-09 | ADC error, missed deadline, queue overflow or lost IRQ under USB/WiFi load | Acquisition stops, fault visible, B2 brakes; BEGIN/recalibration and explicit B2 reset required |
| INT-10 | Concurrent BEGIN/FINISH/status requests and motion updates on two cores | Coherent copies, explicit busy errors, no new calibration fault cleared by an old successful sample; finite recovery without replay |
| INT-11 | SMP=0/SMP=1, normal boot and raw-us timer wrap | Correct shared 1 MHz timebase, non-conflicting IRQ ownership, monotonic modulo-ms timestamps and no spurious wrap fault |

Added on 2026-10-29 as planned cases only. No build, automated test, timing
measurement, powered calibration or flash has been executed for this candidate.

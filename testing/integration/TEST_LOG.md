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

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | INT-01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |

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

# Subsystem 3 — test log

**Subsystem:** Consumer (example)  ·  **Student ID:** <student ID>  ·  **Code:** `app_program/subsystem3/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S3-U01 | statistics self-consistent (min ≤ max, 0..100) | | auto | `subsystem3_test.c` | `[TEST] S3-U01 PASS` |
| S3-U02 | Barcode sensor (IR Left, GP26/ADC0) distinguishes white from black | B3-FR1, B3-FR2 | manual | Flash `ir_bench_test`; car flat on white, press `r`; slide sensor across black line / barcode bar | min–max ≥ 600 counts; label flips WHITE/BLACK correctly |
| S3-U03 | Line sensor A (IR Centre, GP27/ADC1) distinguishes white from black | B3-FR1, B3-FR2 | manual | As S3-U02 | As S3-U02 |
| S3-U04 | Line sensor B (IR Right, GP28/ADC2) distinguishes white from black | B3-FR1, B3-FR2 | manual | As S3-U02 | As S3-U02 |
| S3-U05 | `ir_normalise`: raw → 0..100 for both polarities, clamped, uncalibrated → 0 | B3-FR2 | auto (on target) | `ir_line_bench` unit tests | `[TEST] S3-U05 PASS` |
| S3-U06 | Calibration flags a channel with < 600 counts contrast and forces it to read 0 | B3-FR2, B3-FR10 | auto (on target) | `ir_line_bench` | `[TEST] S3-U06 PASS` |
| S3-U07 | Line error and states for the selected `LINE_MODE` (OUTSIDE: e = A − B, both white = centred, LOST after 2 s; EDGE: ratio, LOST after 200 ms, common-mode rejection) | B3-FR3, B3-FR5 | auto (on target) | `ir_line_bench` (EDGE variant: `cmake -DLINE_MODE=0`) | `[TEST] S3-U07 PASS` |
| S3-U08 | 29-element barcode: every width exact, sent on the first sample after the last bar | B3-FR6, B3-FR7, B3-FR9 | auto (on target) | `ir_line_bench` | `[TEST] S3-U08 PASS` |
| S3-U09 | Short blip (< 5 elements) is not sent | B3-FR11 | auto (on target) | `ir_line_bench` | `[TEST] S3-U09 PASS` |
| S3-U10 | Shorter barcode ends after 150 ms of white and is sent with its real count (< 29) | B3-FR9, B3-FR11 | auto (on target) | `ir_line_bench` | `[TEST] S3-U10 PASS` |
| S3-U11 | Long pattern is cut at 29 elements; grey noise does not split an element (hysteresis) | B3-FR7 | auto (on target) | `ir_line_bench` | `[TEST] S3-U11 PASS` |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | S3-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
| 2026-10-27 | S3-U02 | `644ecee` | Buddy 3 | PASS | `logs/2026-10-27_ir_bench_test.txt` | White ≈ 150–250, black up to ≈ 3360 (contrast ≈ 3200 counts). Reading is higher on black. Run on the bench-test code before it was committed; same code as `644ecee`. |
| 2026-10-27 | S3-U03 | `644ecee` | Buddy 3 | PASS | `logs/2026-10-27_ir_bench_test.txt` | White ≈ 150–170, black up to ≈ 3340. Higher on black. Intermediate values (1000–2000) seen on the line edge. |
| 2026-10-27 | S3-U04 | `644ecee` | Buddy 3 | FAIL | `logs/2026-10-27_ir_bench_test.txt` | Stuck at 147–161 (≈ 0.12 V) on white, black and in the air. GP28 is not receiving the sensor's A0. Suspected wiring: right IR A0 not on GP28 (HTML had it on GP5), VBAT-sense jumper closed, or a motor lead previously on GP28. Re-test after rewiring. |
| 2026-10-27 | S3-U05 | `488ae26` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit.txt` | On target (Pico WH), `ir_line_bench` built from this commit's logic files |
| 2026-10-27 | S3-U06 | `488ae26` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit.txt` | |
| 2026-10-27 | S3-U07 | `488ae26` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit.txt` | Edge mounting assumed; outside mounting would show LOST when centred (see `app_program/subsystem3/README_IR.md`) |
| 2026-10-27 | S3-U08 | `488ae26` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit.txt` | |
| 2026-10-27 | S3-U09 | `488ae26` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit.txt` | |
| 2026-10-27 | S3-U10 | `488ae26` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit.txt` | |
| 2026-10-27 | S3-U11 | `488ae26` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit.txt` | |
| 2026-10-27 | S3-U05..U11 | `f7ac6f5` | Buddy 3 | PASS | `logs/2026-10-27_ir_logic_unit_line_modes.txt` | After adding LINE_MODE. All 7 pass in both the EDGE variant and the default OUTSIDE build. |

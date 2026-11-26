# Subsystem 3 — test log

**Subsystem:** Consumer (example)  ·  **Student ID:** <student ID>  ·  **Code:** `app_program/subsystem3/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| 2026-11-26 (indexed) | S3-11 | Unverified | SE08 | Bends observed | [0:48](https://youtu.be/IBY-M7DeD0M?t=48) | Left and right bend footage shared with M11. Raw sensor traces and white-gap recovery are unverified. |
| 2026-11-26 (indexed) | S3-12 | Unverified | SE08 | One Z decoded | [1:43](https://youtu.be/IBY-M7DeD0M?t=103) | Dashboard shows Z and 29/29. Five-pass A/Z comparison not established. |
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

## Live runtime cases added 2026-10-29 — NOT RUN

The earlier bench results above are preserved. They do not validate the new
micro T-Kernel ADC sampler, task, calibration API or B2 handoff. User approval
is required before builds, host tests or any target execution.

| ID | Case | Planned method / expected evidence |
|---|---|---|
| S3-U12 | Alarm/ADC timing and channel order | Motor power disconnected; verify GP26/27/28 independent raw values, 1 kHz frames, maximum launch lateness and scan duration against provisional 100 us bounds; measure under USB/WiFi load |
| S3-U13 | Live calibration lifecycle | BEGIN, measured white/black on all channels, FINISH; busy/out-of-order rejection; READY only after sufficient contrast; flat GP28 must fail with bit 2; empty calibration must fail |
| S3-U14 | Fault and restart | Inject ADC/FIFO error, queue saturation, late scan and lost IRQ; acquisition stops, health fault asserted, no partial barcode escapes; BEGIN restarts with empty calibration |
| S3-U15 | Sample continuity and timestamp wrap | Synthetic adapter cases before hardware; gap/duplicate timestamps return an error; 64-bit-us to 32-bit-ms time across low-word wrap retains exactly 1 ms schedule |
| S3-U16 | Minimum-bit layout and concurrency | Record sizeof raw frame/status/calibration, ring RAM and stack watermark; concurrent status/command operations never expose torn packed fields; IRQ/task ring copies remain coherent |

GP28's existing S3-U04 FAIL is still unresolved. Retest wiring and the open
VBAT jumper before accepting calibration or moving the vehicle.

## 24 November 2026 · GP26 analogue barcode sensor with hood fitted

At operating height with the car stopped, 22 USB diagnostic snapshots per labelled spot gave these 12-bit raw ADC values. GP27 and GP28 were outside this calibration.

| Spot | Minimum | Median (upper middle) | Maximum |
|---|---:|---:|---:|
| White margin | 180 | 188 | 195 |
| Narrow black bar | 324 | 333 | 337 |
| Wide black bar | 2878 | 2885 | 2889 |
| Thin white gap | 226 | 234 | 244 |

Retained white 200, bar start 240, minimum rise 60, reference cap 300, half-peak end rule and 8-sample hold. The fixed threshold recognises the white margin. The thin gap needs the relative rule. Even the weakest stationary narrow bar was 80 counts above the highest stationary thin-gap reading.

For a sensitivity check, retained the historical Z trace geometry and noise and fitted a second model to the hood medians (centred narrow bar assumed to fill 1/1.6 of the spot). In the existing spot-scale 0.6–0.9 simulation, the installed settings decoded 77/80 traces. A temporary candidate with white 210, bar start 260, rise 70 and cap 320 decoded 60/80. These figures are simulations. The route suite and the historical 21-scan replay passed.

Two physical Z passes were captured with the installed settings. The first ended at 27 elements; logged gap minima included 830 and 1118. The second reached 29 elements and decoded Z by time and distance; the highest reported gap minimum was 283. The dashboard reported Z. A missing bar/space pair is consistent with weak separation during the first pass. Extrema alone cannot identify the missed edge. The console marked both scans outside an active V5 run; they do not establish the driving success rate.

[Raw stationary readings](logs/2026-11-24_hood_static_adc.csv) · [USB widths and extrema from both passes](logs/2026-11-24_hood_Z_passes.txt)

### Five driving passes reported at 18:20 Singapore time

The USB capture ended around 18:19:36 and the dashboard subsequently saw a new Pico boot (1932663337). MQTT event frames carried three distinct nonempty barcode snapshots from that boot. The previous boot's sequence 11 was already present before this test and is excluded.

| Barcode sequence | Motion epoch | Sample time since boot | Elements | Letter |
|---:|---:|---:|---:|---|
| 3 | 5 | 31.580 s | 29 | Z |
| 5 | 8 | 48.976 s | 5 | unreadable |
| 8 | 11 | 71.827 s | 29 | Z |

Two distinct Z decodes confirmed. Snapshot sequence gaps prevent assigning every capture to a physical pass. The five-element fragment may be incidental. This batch has no complete set of width or raw-level traces, so its five-pass success rate is undetermined. No threshold adjustment based on this batch.

### 18:24 single driving pass on Z

The active decoder returned Z by time and distance on 29 elements. In the data character the wide space measured 398 ms versus 150 ms for the next widest, and 252 versus 102 encoder counts (minimum ratio 1.25). Across the scan, the lowest bar peak was 289 ADC and the highest gap minimum was 219. The old diagnostic decoder returned unknown. The route console armed the U-turn action. At review time the dashboard file had no new receipt for this pass. [USB scan evidence](logs/2026-11-24_hood_Z_drive_single.txt)

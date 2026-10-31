# Subsystem 2 — test log

**Subsystem:** Buddy 2 motion (plus compatibility producer)  ·  **Student ID:** 2501308  ·  **Code:** `app_program/subsystem2/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S2-U01 | sample values stay within 0..100 | | auto | `subsystem2_test.c` | `[TEST] S2-U01 PASS` |
| S2-U02 | free slots and free blocks within pool size | IF-02 | auto | `subsystem2_test.c` | `[TEST] S2-U02 PASS` |
| S2-U03 | quadrature direction, stable input and impossible transition | B2-FR2 | auto, pure logic | `motion_logic_cases.h`, host or TEST=1 | +4/-4 per complete x4 cycle; impossible transition flagged |
| S2-U04 | count deltas across uint32 wrap in both directions | B2-FR2 | auto, pure logic | same runner | exact +5/-5, no signed overflow |
| S2-U05 | PID saturation, anti-windup, reverse sign, reset and zero dt | B2-FR3 | auto, pure logic | same runner | bounded output, no windup, zero output for invalid dt |
| S2-U06 | Code39 A-D both directions and malformed frames | B2 decoding | auto, pure logic | same runner | correct commands; no command for truncated/overflow/saturated/ambiguous frames |
| S2-U07 | calibrated commands reject unknown configuration | B2-FR2/3/8 | target, no movement | constructor/submit with default config and out-of-range wide arguments | E_PAR; no driver write; no truncation accepted |
| S2-U08 | motor pins, pulse limit, brake and reversal rest | B2-FR1/6 | manual powered | after separate approval, local bench requests for each wheel/direction | GP8/9 left, GP10/11 right; <=20%, <=500 ms request; brake/rest; record physical response separately |
| S2-U09 | encoder counts/sign/capture capacity | B2-FR2 | reference hardware | known wheel revolutions and reference pulse stream, both directions, under load | counts agree; any ambiguity flagged; document max validated rate |
| S2-U10 | PID and straight-line accuracy | B2-FR3/4 | measured powered | calibrated speed steps and repeated straight travel | compare to agreed speed/path bounds; retain CSV and independent reference |
| S2-U11 | distance and 90/180-degree turns | B2-FR5 | measured powered | forward/reverse and both turn directions with external position/angle reference | both wheels complete once or timeout; document accuracy and slip |
| S2-U12 | stop/expiry/reset, admission and renew races | B2-FR6/8/9 | integration | fill slot; stop during movement and while reset/renew competes; expired/replayed ids and epochs | stop independent of slot; terminal outcome; no old movement restart; reset never drives |
| S2-U13 | source faults, stall, invalid edges, control lateness | B2-FR9 | fault injection | inject each cause at controlled speed; clear source without reset | brake, latched fault, invalid status; no auto restart |
| S2-U14 | S3 bridge and barcode manoeuvre lifecycle | IF-08 | integration | valid calibrated stream, stale/gapped/invalid samples, repeated frames, turn completion and new line | correct steering sign; no stale/partial decode; exclusive turn; new line needed after turn |
| S2-U15 | B4/B1 coherent wheel snapshots | B2-FR7 / IF-09 | integration | concurrent readers, delayed input, reset, wrap and out-of-range travel | paired wheels/time; E_TMOUT retry; invalid/old epochs rejected by consumers |
| S2-U16 | release jitter, execution, output-stop latency and memory | K4 / minimum-bit constraint | instrumentation | WiFi/IR/IMU load; compare field representations and target map | record results against agreed targets; no timing or RAM claim from compilation |
| S2-U17 | explicit unsigned packing of signed physical data | minimum-bit / Barr 5.3a | auto, pure logic | boundary roundtrips and known negative masks | exact values; no signed bit-fields or signed shifts |
| S2-U18 | steering sign and mean speed under saturation | B2/S3 handoff | auto, pure logic | positive/negative error and cruise close to maximum | positive error turns left; negative turns right; mean unchanged; invalid input rejected |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | S2-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |

Implementation status, 2026-10-29: S2-U03..U18 prepared but **NOT RUN**.
No compilation, test program, firmware run or flash occurred during integration.
The user's permission is required before those steps. Earlier standalone motor
observations do not count as passes for this RTOS version.

Host-only command proposed for the first approved stage (from repository root):

```sh
cc -std=c11 -Wall -Wextra -Wconversion -Werror testing/subsystem2/motion_logic_test.c app_program/subsystem2/motion_logic.c -o /tmp/buddy2-motion-logic
/tmp/buddy2-motion-logic
```

Then compile both SMP variants and TEST=0/1 with the documented toolchain before
requesting separate approval for flashing. The host cases do not cover MMIO,
RTOS concurrency, physical stopping, PID tuning or end-to-end sensor acquisition.

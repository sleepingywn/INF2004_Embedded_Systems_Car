# Subsystem 3 — test log

**Subsystem:** Consumer (example)  ·  **Student ID:** <student ID>  ·  **Code:** `app_program/subsystem3/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S3-U01 | statistics self-consistent (min ≤ max, 0..100) | | auto | `subsystem3_test.c` | `[TEST] S3-U01 PASS` |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | S3-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
| 2026-11-26 (indexed) | S3-11 | Unverified | SE08 | Bends observed | [0:48](https://youtu.be/IBY-M7DeD0M?t=48) | Left and right bend footage shared with M11. Raw sensor traces and white-gap recovery are unverified. |
| 2026-11-26 (indexed) | S3-12 | Unverified | SE08 | One Z decoded | [1:43](https://youtu.be/IBY-M7DeD0M?t=103) | Dashboard shows Z and 29/29. Five-pass A/Z comparison not established. |

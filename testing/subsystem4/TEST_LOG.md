# Subsystem 4 — test log

**Subsystem:** Monitor (example)  ·  **Student ID:** <student ID>  ·  **Code:** `app_program/subsystem4/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S4-U01 | message buffer max line = S4_SUMMARY_MAX | IF-03 | auto | `subsystem4_test.c` | `[TEST] S4-U01 PASS` |
| S4-U02 | LED pauses and resumes every 6 reports | IF-05 | manual | 1. Flash normal image. 2. Watch LED and console ~20 s. | console shows "LED task paused"/"resumed"; LED stops/starts |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | S4-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
| 2026-11-26 (indexed) | S4-U12 | Unverified | SE08 | Hump events observed | [2:46](https://youtu.be/IBY-M7DeD0M?t=166) | Dashboard advances to one hump and 13 mm. Separate USB footage reports a 9.9 mm peak. |
| 2026-11-26 (indexed) | S4-U10 | Unverified | SE08 | Lift and overturn stops observed | [3:29](https://youtu.be/IBY-M7DeD0M?t=209) | Serial stop reports accompany the lift and the overturn ([3:54](https://youtu.be/IBY-M7DeD0M?t=234)). |
| 2026-11-26 (indexed) | S4-U19 | Unverified | SE08 | Partial | [3:41](https://youtu.be/IBY-M7DeD0M?t=221) | Serial heading reaches +82 degrees. A measured reference angle is unavailable. |

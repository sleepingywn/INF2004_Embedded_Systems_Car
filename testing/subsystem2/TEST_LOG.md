# Subsystem 2 — test log

**Subsystem:** Producer (example)  ·  **Student ID:** <student ID>  ·  **Code:** `app_program/subsystem2/`

See `testing/README.md` for how to run tests and record results.

## Test cases

Define a test here before running it. Test IDs never change.

| ID | What is tested | Requirement / interface | Method | Steps / how | Expected result |
|---|---|---|---|---|---|
| S2-U01 | sample values stay within 0..100 | | auto | `subsystem2_test.c` | `[TEST] S2-U01 PASS` |
| S2-U02 | free slots and free blocks within pool size | IF-02 | auto | `subsystem2_test.c` | `[TEST] S2-U02 PASS` |

*Method:* **auto** = `TEST_CHECK` in code, result printed on the console;
**manual** = a person observes the result (LED, scope, meter, phone app…).

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test ID | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | S2-U01 | `abc1234` | | PASS / FAIL | `logs/YYYY-MM-DD_….txt` | |
| 2026-11-26 (indexed) | M13 | Unverified | SE08 | Partial | [0:31](https://youtu.be/IBY-M7DeD0M?t=31) | Dashboard Start/Stop and an onboard restart are shown in separate runs. Stop latency was not measured. |
| 2026-11-26 (indexed) | M11 | Unverified | SE08 | Two example passes | [0:48](https://youtu.be/IBY-M7DeD0M?t=48) | Opposite bends shown. The video does not establish all repetitions or the speed comparison. |
| 2026-11-26 (indexed) | M12 | Unverified | SE08 | Return-line run observed | [2:00](https://youtu.be/IBY-M7DeD0M?t=120) | The robot pivots at the junction and returns along the incoming line. Missing-line trial not shown. |

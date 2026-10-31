# Buddy 2 calibration, PID tuning and accuracy evidence

Status: NOT RUN. No numbers below are measured. Build/tests/flash require the
user's approval. Source implementation is not a PID tuning report or accuracy pass.

## Calibration record to fill

| Item | Value / evidence |
|---|---|
| Motor pins / wheel identity | User confirmed left GP8/9; right GP10/11, 2026-10-29 |
| Encoder model, electrical output and supply | Pending |
| Encoder A/B pins, positive direction | Proposed left GP4/5, right GP2/3; verify |
| Motor signs (+1/-1) | Pending forward-direction observation |
| X4 counts per output-wheel revolution, each wheel | Pending known-turn reference; include gearbox |
| Loaded wheel circumference and axle track width | Pending physical measurement |
| Reliable speed range, maximum duty, stop state | Pending; board specifies both LOW=brake |
| Source maximum age, no-progress timeout, reversal rest | Pending measured justification |
| Configuration ID and Git commit | Pending frozen calibrated build |

## PID tuning protocol (after approval)

Record timestamp, requested/actual left/right speed, raw counts, applied duty,
error and operating conditions for repeatable forward/reverse speed steps.
Start with integral/derivative disabled and a conservative duty limit; establish
the measured response before adding gains. Check saturation, anti-windup, zero
target, command expiry and stop/reset. Repeat with WiFi, IR and terrain load.
Do not increase gains to hide a reversed/missing encoder or incorrect calibration.

| Configuration | Kp/Ki/Kd | Speed step | Rise/settling time | Steady error | Overshoot | Evidence |
|---|---|---|---|---|---|---|
| Pending | | | | | | |

## Motion accuracy protocol (after approval)

Measure commanded and actual travel against an independent ruler/track reference;
test forward/backward and both turn directions, including 90/180 degrees. Use an
external angle reference; encoder agreement alone is not proof of physical angle.
Record surface, load, battery/supply, speed, trials, mean/max error and failures.
The design's proposed repeated-turn targets must be agreed before judging a pass.

| Configuration | Command | Trial count | Mean error | Max error | Stop/cancel result | Evidence |
|---|---|---|---|---|---|---|
| Pending | | | | | | |

## Memory and timing evidence

- Record `sizeof(motion_request_t)`, `sizeof(motion_status_t)`,
  `sizeof(motion_pid_t)` with the TARGET compiler, including padding.
- Compare equal-capacity/equal-function unpacked reference representations with
  the current fields, plus total firmware map `.data`/`.bss` and stack high-water.
  Comparing main's sample producer to a whole motion controller is not a fair
  bit-packing savings measurement.
- Record request/status wire serialization sizes separately from C object sizes.
  No native bit-field struct is an MQTT frame.
- Measure ISR worst observed time and missed/invalid counts against a generator
  or independent encoder reference under load. The proposed 2,000 events/s/wheel
  is a requirement candidate, not a result.
- Measure motion release jitter, execution, stop-receipt-to-output response and
  physical wheel stopping separately. Include both cores and full handoff load.
- Run B2/B4 stale, invalid, epoch-reset and wheel-time alignment cases before
  evaluating hump height. Raw wheel data is not a verified terrain estimate.

## Approval log

No build, static-analysis tool, host executable, firmware test or flash has been
run for this integration. Approval for the earlier standalone motor test does not
override the user's new explicit requirement to ask before testing this work.

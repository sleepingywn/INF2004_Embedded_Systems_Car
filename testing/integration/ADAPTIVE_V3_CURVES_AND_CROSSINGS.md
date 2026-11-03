# Adaptive v3: curve transitions and stop classification

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

2026-11-04. Implemented for software validation. Not flashed or physically
tested. The installed firmware remains ADAPTIVE v2.

## Evidence and scope

The v1 straight-run capture stopped at 5486 ms on two backward right-encoder
counts during a commanded pause. The physical cause of those backward counts
is unresolved. V2 reduced overly abrupt corrections and retained encoder checks.

The recorded v2 run traversed the bend, with staged rotation and delayed body
alignment at the exit. Its saved history shows intentional left-wheel pauses,
right-wheel targets near 600 counts/s and measured right speeds generally close
to those requests. At 17261 ms it stopped at the transverse stripe with both
normalized readings high and step 15, not a motor/encoder/IR fault. See
`ADAPTIVE_LINE_CONTROLLER.md` for the video and capture references.

The user requested improved curve transitions and stop handling, and explicitly
selected: "Try straight ahead with bounded reacquisition, then stop if no line
is found" for a crossing without a barcode route instruction. This is an
explicit GP20 trial policy, not inference of junction geometry from two sensors.

## Curve changes

- Ordinary curves retain positive rolling wheel targets. A tracking pause now
  requires raw error magnitude at least 75, agreement with filtered proportional
  demand, and 60 ms of continuous evidence. Derivative spikes cannot enter it.
- A paused inside wheel resumes when raw error falls to 55 or crosses to the
  opposite side, even if the filtered error still reflects the earlier turn.
- During a confirmed pivot, cap the outer target at 450 counts/s instead of 600.
  This reduces requested angular speed. There is no fixed maximum curve time.
- Ramp the tracking inside-wheel target down to zero using the existing
  4000 counts/s-per-second deceleration limit instead of dropping it directly
  to zero. Existing per-wheel PI feedback and output limits still apply.
- Faults and manual STOP bypass these target ramps and stop the outputs.

These thresholds and speeds are provisional hardware/controller settings for
all layouts. There is no course position, bend count or known video timeline
in the code. The changes do not measure chassis heading or prove body-path
alignment; sensor-to-axle geometry, traction and physical response remain to be
validated. The encoder backward-count allowance is unchanged.

## Crossing and reacquisition

`line_crossing.h` owns the bounded encounter state independently of steering.
The first both-black observation starts one non-renewable encounter budget.
Instead of stopping immediately, command approximately 300 counts/s per wheel.
A bounded correction from the difference in wheel travel holds the entry
orientation. This cannot correct an already-wrong entry heading or identify
which branch is intended.

While the mark persists, stop at either 400 counts of maximum wheel travel or
1800 ms. Across the whole encounter and reacquisition, stop at either 900 counts
or 3500 ms. These are count-domain limits, not calibrated track-width estimates.
Existing unresolved line-loss budgets still apply; entering a mark cannot
extend them. The first exhausted bound wins.

After the mark clears:

- Real directional readings can steer immediately.
- Both-white is uncertain, so the existing limited advance and bounded search
  attempt reacquisition rather than declaring success.
- Replenish the crossing budget only after 60 ms of clear evidence: one sensor
  at or below 10 and the other at or above 20 on the calibrated 0-100 scale.
  Gray/gray readings, isolated sightings and black/white flicker cannot renew it.
- A later, distinct crossing gets a new budget only after that confirmation.

Both-white centered-line ambiguity is still present with this sensor placement.
The robot may need a search to obtain evidence and may still stop if the geometry
provides no distinguishable observation. The bounds prevent unlimited travel.

## Barcode command boundary

The adaptive trial now checks correctly decoded captures against its own 100 ms
freshness bound, the current epoch, fresh healthy line data and strictly newer
capture time. A new run fences out pre-start captures. This does not enable the
uncalibrated production navigation path.

- C (straight) agrees with the trial policy and permits continuing.
- A, B or D (left, right or U-turn) causes a route-required stop, not an automatic
  turn. This avoids silently driving straight past an instruction the trial
  cannot execute yet.
- Stale, malformed, wrong-epoch or duplicate captures are rejected.

Full barcode-directed navigation remains unfinished; this version supports the
user-selected straight-crossing experiment and an explicit stop for other routes.

## Stop reporting and protections

Existing step numbers remain stable. Step 17 is crossing exhaustion. Append
`reason` to the existing diagnostic/history rows; earlier fields stay in place.

| Reason | Meaning |
| --- | --- |
| 0 | No adaptive reason; inspect step, motion fault and encoder/source details |
| 1 / 2 | Line recovery time / travel exhausted |
| 3 | Both bounded search sweeps exhausted |
| 4 / 5 | Persistent mark time / travel exhausted |
| 6 / 7 | Whole crossing time / travel exhausted |
| 8 | Decoded route requires unsupported manoeuvre |
| 9 | Missing/invalid calibrated pair |

Adaptive modes 7 and 8 indicate advancing across a mark and crossing exhaustion.
Mode 3 is reserved from previous versions. Step 15 remains route-required
cancellation without a motor fault. Crossing exhaustion is step 17 with the IR
position/reacquisition fault; it does not itself mean ADC hardware failed.

GP20 and console STOP, producer health faults, stale or wrong-epoch input,
encoder invalid-edge/reversal checks, stall detection, 20% PWM limit and the
30-second overall trial limit remain active. No stop guard was globally delayed.

## Verification and next physical check

`python testing/subsystem2/run_adaptive_tests.py` exercises six executables plus
two invalid-profile checks. Added coverage includes both clock-wrap cases and
left/right curve exits at 20/40/60 ms, ramped pause entry, crossing success,
permanent marks, missing line after a crossing, independent distance/time bounds,
gray/noisy observations, and non-renewing encounter budgets. Twenty-two actual
motor-owner scenarios include STOP and health/encoder/stale-input failures while
crossing, A/B/C/D commands, pre-start, stale, duplicate and wrong-epoch captures.
The unchanged legacy regression runner covers another 38 executables.

The old v2 image is preserved locally as
`tools/dashboard/.local/adaptive-v2-installed-16014709.uf2`. V3 uses the same
adaptive build filename with a new hash and `ADAPTIVE v3` banner. A separate
`adaptive-v3-manifest.json` records the final build; do not use the v2 manifest
to flash the newly built artifact.

After software checks, request the user's flash approval and verify the version
and zero outputs after reboot. Then evaluate the same bend and crossing with
unchanged parameters. Record body alignment, smoothness and exact stop reason.
Software test success is not a claim that unexpected physical stops are resolved.

## Completed software verification

All 44 test executables (6 adaptive plus 38 legacy) and two invalid-profile
checks passed. The final firmware build completed without compiler warnings.
SHA256: `2D09D2303B411C52D2A6B917C27D8974342C86F36DE65E7B205F397517BD74E8`.
No serial commands, flash, or motor tests were performed for this implementation.

## Approved v3 flash and stationary verification

After explicit user approval, verified the sole RPI-RP2 BOOTSEL drive and the
v3 SHA256 above, then copied the image. Normal USB startup reported ADAPTIVE v3.
Six status snapshots showed zero motor duties, no motion faults, and GP20
armed/unpressed. IR acquisition reported no faults. Only h/i read commands
were sent; no movement was started. Evidence:
`tools/dashboard/.local/gp20-adaptive-v3-stationary-20261104-015405.{json,log}`.
The v3 manifest records installation and stationary verification. Physical
curve and crossing behavior remains unverified.

## Two physical v3 runs: comparison and unresolved encoder stop

The user supplied video_2026-11-04_01-58-56.mp4 (16.83 s, right-side start)
and video_2026-11-04_01-58-52.mp4 (15.17 s, left-side start), both in
C:/Users/joell/Downloads/Telegram Desktop. Per the user's explicit ordering,
the LEFT-side start is the latest run, despite filename order. Full-run frame
sequences and half-second final sequences were inspected. Both enter/follow
part of the bend and stop before the progress achieved in the recorded v2 run.
Neither video alone establishes the exact electrical cause of its stop.

The powered latest-run capture is:
tools/dashboard/.local/gp20-adaptive-v3-result-20261104-020007.json and .log.
All 64 rows recovered, epoch/id 3/2, no conflicts, ADAPTIVE v3 banner verified.
The latest run stops at 12567 ms, step 4, motion fault 0x1, encoder reason
0x20 (right count receding), speed-layer rejection zero. Right raw counts
3815 -> 3814, with previous targets 450/0 and right duty zero. The current
per-pause allowance is only one cumulative backward count. With these stopped
wheel conditions, prior exhaustion of that allowance is consistent with the
rejection, but the saved diagnostic does not expose the used allowance.
Invalid encoder edges were unchanged at zero. IR acquisition and handoff
reported no faults; failing line age was zero and error -87, showing directional
line evidence still present. Only the latest run's reason is available.

Before the stop, retained requests typically kept the inside right wheel near
250 counts/s and outside left near 540-600, then switched into several 450/0
pivots near the end. Measured speeds broadly follow those requests. No crossing
state (mode 7) occurs in the retained interval, and the stop is not a crossing
limit. The v3 policy deliberately required stronger evidence before pivoting,
released earlier, and reduced pivot outer speed from 600 to 450. These changes
reduced pivot authority; the videos/logs do not isolate which change affected
path accuracy or prove that smoothing alone caused the earlier stop.

The previously recorded v2 right-start run progressed farther and stopped at a
crossing with no motion fault. The new left-start run is the opposite turn
sense, so it is not a matched v2/v3 comparison. Do not call v3 a validated
improvement or infer that its crossing logic has failed from this evidence.
Recommended next comparison: restore v2 steering as a controlled baseline,
retain bounded crossing behavior, and add paused-wheel diagnostics (allowance
used, raw deltas and commanded state) to investigate rollback/noise without
blindly raising the reversal allowance. No code changes, flashing, or motor
commands were made during this comparison; only h/i/t reads were sent.

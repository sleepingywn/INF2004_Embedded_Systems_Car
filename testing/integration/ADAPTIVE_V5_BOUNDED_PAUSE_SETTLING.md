# Adaptive v5: bounded encoder settling during intentional wheel pauses

2026-11-04. Implemented after user approval. Software validation complete;
initially not flashed or physically tested. See the flash verification and user
feedback records below for the subsequent installed/tested state.

## Evidence and scope

V4 capture `tools/dashboard/.local/gp20-adaptive-v4-result-20261104-021735.json`
identified an encoder stop at 14158 ms. The right wheel was intentionally paused
(target and duty zero), and reported two backward counts at pause ages 40 and
60 ms. The second exceeded the previous one-count allowance. IR was fresh,
invalid quadrature transitions were zero, and the 30-second limit was not reached.
This does not prove whether the motion was physical rollback or electrical noise.

V5 addresses that confirmed failure without altering steering gains, wheel
speed targets, pause-entry/release decisions, crossing behavior, or motor wiring.
The recorded repeated pause/restart oscillation remains an unresolved, separate
tracking concern. The comparison must not be presented as a steering fix or a
guarantee of completing arbitrary courses.

## Policy

Only the adaptive profile changes. Other profiles retain their one-count policy.
A backward sample is allowed only when all conditions hold:

- The previous interval deliberately paused that wheel, with target AND duty zero.
- The settling episode is active and less than 120 ms old (120 ms is rejected).
- Gross backward travel in the episode, including this sample, is at most two counts.
- Existing raw validity, input freshness, epoch, timing, invalid-edge, absolute
  negative-distance and other encoder/speed checks pass.

Two counts are the smallest increase that admits the recorded pattern. The
120 ms window is a provisional two-maximum-control-period bound, not calibrated
physical distance, a course-specific timer, or a measured universal settling time.
No electrical reverse drive is introduced. A stationary paused wheel does not
fault merely because its settling window expires; further backward motion does.

Forward encoder bounce does not refund gross backward counts. Brief positive
target pulses do not reset the count allowance or window. Rearming requires
at least 60 ms of consecutive sampled forward progress and at least three
forward counts, with previous and next targets at least the existing minimum
rolling target (250 counts/s), and both corresponding duty values positive.
An interruption resets that qualification. A new settling window then begins
when output is next commanded paused. This is independent for each wheel.

Acceptance updates both the raw-progress baseline and the speed-controller
baseline so the same negative delta is not rejected a second time downstream.
All changes commit only after the complete controller step succeeds. Powered
wheel reversal, excess counts, late backward motion, invalid transitions,
STOP, stale/missing IR, health faults, stalls and overall run limits retain
stop behavior. GP20 remains the deliberate start/stop button; no autostart.

## Diagnostics

V4 frozen fields are retained. A new `[line] encoder window_age_ms=...` line
reports each settling episode's age and the applicable window limit. Episode
age can exceed the current continuous pause age after a brief restart because
that restart does not replenish the budget. `4294967295` means inactive;
window limit zero identifies the unchanged legacy policy.

Startup banner begins `ADAPTIVE v5: v2 steering baseline, bounded straight
crossing, bounded pause settling`.

## Validation

Eight adaptive executable suites and two invalid-profile rejection checks pass.
The actual motor-owner suite now has 39 scenarios (50 through 88), including
both-wheel acceptance of the recorded 40/60 ms sequence, a third backward
count, a three-count burst, first backward movement at 120 ms, invalid edges,
powered outside-wheel reversal, and STOP/health faults during settling.
Fault diagnostics remain frozen after controller reset; final outputs are zero.
Old late-reversal tests now expect rejection at the first late backward count,
rather than accepting one count at an arbitrary time during a pause.

The new settling suite covers both sides, 20/40/60 ms entry cadence, raw/time
wrap, the 119/120 ms boundary, gross-count bounce, brief restarts with and
without forward ticks, no-progress renewal prevention, and qualified rearming.
Existing controller/steering/crossing/admission/console/handoff checks pass.
All 38 legacy-profile executable regression suites pass: 46 executables total,
plus the two configuration rejection checks. No hardware was accessed.

Firmware build completed without compiler warnings/errors. SHA-256:
`A152BE67F5CEF02319A5B4E1A8E5E4D32F7FA69311A08D53CECDE81FB6ABC3C8`.

V4 is separately preserved as
`tools/dashboard/.local/adaptive-v4-installed-11b01a7e.uf2`.
V5 artifact and installed-state distinction are recorded in
`tools/dashboard/.local/adaptive-v5-manifest.json`. The v5 stationary verifier
and history reader are prepared and syntax-checked but have not been run.

## Next physical comparison

After explicit approval, flash with wheels lifted and verify the exact v5
banner, zero outputs, no faults and GP20 armed. Run from the same placement
and direction as the last v4 comparison, recording the motion. Preserve power
and read saved history if it stops. A complete run without this fault does not
by itself resolve oscillation or establish that the encoders are noise-free.

## Flash and stationary verification

2026-11-04: after explicit user approval, the hash-verified v5 image was
copied to the identified RPI-RP2 BOOTSEL drive. The rebooted firmware reported
the exact ADAPTIVE v5 banner. Six snapshots confirmed zero motor outputs,
no motion/IR faults, and GP20 armed. Armed LED mode was observed three times.
Only help/status commands were sent; no motor-start commands or movement test.
Evidence: `tools/dashboard/.local/gp20-adaptive-v5-stationary-20261104-023636.json`.

## User feedback after the installed V5 run

The user reported "ok this is good" after flashing V5 and asked for the changes
to be pushed in a PR. Record this as positive operator feedback from a physical
run, not independent confirmation of a complete 30-second run or arbitrary-course
reliability. No new video, run duration or post-run telemetry was supplied with
that feedback. Steering oscillation and the underlying source of backward
encoder counts remain open validation items.

For the current build, operating instructions and reproducible software checks,
see [the V5 release guide](ADAPTIVE_V5_RELEASE.md). Earlier documents in this
folder describe historical trials, not additional changes to the installed V5.

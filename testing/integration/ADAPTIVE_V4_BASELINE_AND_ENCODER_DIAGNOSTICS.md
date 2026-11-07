# Adaptive v4: v2 steering with bounded crossings and encoder diagnostics

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

2026-11-04. Prepared for a controlled comparison after explicit user approval
to restore v2 steering while retaining v3 crossing handling. Not flashed or
physically tested at implementation time. Installed version remains v3.

## Motion changes

Restore the v2 tracking behavior that completed the bend in the recorded run:

- Remove v3's additional raw-error pause-entry gate of 75.
- Remove v3's immediate pause-release gate at raw error 55.
- Use v2's filtered proportional-demand hysteresis and 60 ms confirmation.
- Remove v3's 450 counts/s pivot-only outer cap; use the existing target mapping
  and normal maximum of 600 counts/s.
- Restore v2's immediate commanded inside-wheel pause. Positive target changes
  still use the established ramps and wheel-speed PI controller.

The v2 continuous filter, gains, soft startup/restart, speed feedback and output
cap are unchanged. This is restoration of the tracking baseline, not a full
rollback to the old firmware: v3's bounded straight crossing, bounded
reacquisition, reason codes and barcode-command boundary remain in place.
An accepted A/B/D barcode still stops for unsupported route execution; C agrees
with the straight trial policy. No arbitrary-course completion is claimed.

## Diagnostic additions; no expanded reversal allowance

The latest v3 run stopped on a backward right-encoder count during a pause.
The old report could not show whether the cumulative one-count allowance had
already been consumed. V4 freezes the following before the motor owner resets
its controller after an encoder failure:

- Forward-positive left/right count deltas for the rejected sample.
- Previous commanded targets and duty magnitudes (not new requested targets).
- Pause-eligibility mask: bit 0 left, bit 1 right. Eligibility requires an
  intentional pause plus target zero and output zero in the prior interval.
- Cumulative allowance used per wheel and the unchanged one-count budget.
- Age of each eligible pause and time since its last accepted settling count.
  `4294967295` means the corresponding age does not apply.

The existing raw counters, previous raw counters, invalid-edge count, time
interval and rejection flags are retained. New console lines begin
`[line] encoder pause` and `[line] encoder pause_age_ms`; ordinary history row
format remains unchanged from v3. Diagnostic timestamps never affect control.

The guard and diagnostic use the same pause-eligibility predicate. The owner
captures eligibility before stepping the controller, so a new steering intent
cannot rewrite the description of the previous output interval. The new fields
remain available through the stopped observer after reset and subsequent idle
cycles. Their presence does not itself distinguish physical tyre rollback from
electrical encoder noise; that may still require a separate physical check.

All reversal/invalid-edge checks, stall detection, IR freshness/epoch checks,
GP20/console STOP, health-fault handling and the 30-second test limit remain.
The same encoder fault can still occur; this revision deliberately does not
raise a tolerance before the cause is understood.

## Verification

The adaptive runner now has seven executable suites plus two rejected-profile
checks. The old combined v3 test was split into `line_crossing_test.c` and
`line_adaptive_baseline_test.c`: crossing behavior remains checked while v2's
tracking distinctions are explicitly asserted on both sides, at 20/40/60 ms,
and across clock wrap. The baseline tests distinguish a sustained error 60,
filtered release after error 55, the 600 outer cap and immediate pause entry
from v3's intentionally removed behavior.

Twenty-seven actual-owner scenarios include five new diagnostic cases:
two separate one-count reversals and one two-count reversal on each paused
wheel, plus reversal on the powered outside wheel. They verify fault reason,
prior commands, eligibility, used budget, timestamps, frozen observer lifetime
and zero final motor outputs. The existing 38-executable regression suite is
also run for legacy and other firmware profiles.

## Build and next run

The separately preserved installed image is
`tools/dashboard/.local/adaptive-v3-installed-2d09d230.uf2`.
Use `adaptive-v4-manifest.json` for the new hash and build result. The expected
startup banner begins `ADAPTIVE v4: v2 steering baseline, bounded straight
crossing, paused-encoder diagnostics`.

Flash only after user approval, then verify the banner, stopped outputs and
GP20 armed state. For a useful comparison, start with the same direction and
placement as the successful v2 video. Record the opposite direction separately.
If it stops, preserve power and collect the new encoder fields before another
run or reboot. Do not attribute a shorter run to steering alone when its saved
reason is a separate encoder fault.

## Completed software validation

All 45 test executables (7 adaptive, 38 legacy) and both invalid-profile
rejection checks passed. The firmware build completed without compiler warnings
or errors. SHA-256: `11B01A7EFBF82568E0BFE81CEBE66A27C85F574A4F1BD35627BE4FF34267E740`.

The v4 read/verification helpers are prepared and syntax-checked, but have not
been run against hardware. No flashing or motor run occurred during this change.

## Flash and stationary verification

2026-11-04: after explicit user approval, the hash-verified v4 image was
copied to the identified RPI-RP2 BOOTSEL drive. The rebooted firmware reported
the exact ADAPTIVE v4 banner. Six snapshots confirmed zero motor outputs,
no motion/IR faults, and GP20 armed. Armed LED mode was observed four times.
Only help/status commands were sent; no motor-start commands or movement test.
Evidence: `tools/dashboard/.local/gp20-adaptive-v4-stationary-20261104-021138.json`.

## First v4 run: diagnosed encoder stop

Preserved capture: `tools/dashboard/.local/gp20-adaptive-v4-result-20261104-021735.json`
and its companion log. All 64 history rows recovered for epoch/id 3/2 without
conflicts. Exact v4 banner verified. An older startup status appears in the
buffer; repeated current reports identify the run failure below.

At 14158 ms, encoder reason 0x20 stopped the run. Right forward-positive delta
was -1, with pause eligibility 0x2, prior target 600/0, prior duty magnitude
117/0, used allowance 0/1 and budget 1. Right pause age was 60 ms; its prior
accepted settling count was 20 ms earlier. This confirms two backward counts
across successive samples during the same commanded pause. Invalid quadrature
edges were zero; IR age was zero with no IR or handoff faults. No commanded
reverse drive is involved. Physical rollback versus valid-looking electrical
noise remains unresolved.

The retained history also shows repeated right-wheel pauses/restarts and error
swings, so eliminating this stop would not establish smooth curve tracking.
Two brief crossing-mode entries occurred before the encoder failure; crossing
exhaustion and the 30-second run bound did not cause this stop. Powered wheel
speeds generally followed requests. No new motion test, firmware edit, or flash
was performed while collecting this report.

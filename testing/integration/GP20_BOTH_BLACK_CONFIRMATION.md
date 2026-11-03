# Bounded both-black confirmation during an established curve

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

## Triggering evidence

The 23:34:42 video and preserved trace
`tools/dashboard/.local/gp20-curve-recovery-result-20261103-233533.{log,json}`
show the tighter-turn fallback working, followed by an immediate rejection of
LINE_BOTH_BLACK at firmware elapsed 11638 ms. The input was fresh (3 ms),
without acquisition/handoff or encoder faults. The old controller stopped on
the first both-black sample. It did not expire a lost-line search or stall.
See `GP20_CURVE_TRIGGER.md` for the full run interpretation and limitations.

## Change

Buddy 2's search-enabled line profile now permits a short both-black candidate
only during a run with prior forward travel, a real curve error of at least 30
within the preceding 200 ms, and no active lost-line search. Admission still
rejects both-black; it cannot start the car. Other line profiles still reject
it immediately. Buddy 3's sensor classification/calibration and Buddy 4 are
unchanged by this patch.

On the first eligible sample, remember the current wheel targets and reduce
each positive target toward at most 350 counts/s using the existing target
ramp. Zero remains zero, preserving an intentional inside-wheel pause. A small
positive restart target is not raised. Normal speed feedback, output slew,
20% maximum duty and encoder/stall protection remain active. The target cap is
not a claim that actual wheel speed drops instantaneously.

Stop on a continuing candidate after 120 ms or 80 forward encoder counts on
either wheel from the first sample, whichever is detected first at a control
update. These are count-domain bounds, not calibrated distance. Distinct
diagnostic results are appended without renumbering previous values:

- step 13: LINE_STEP_OVERLAP_TIMEOUT, mapped to the IR motion fault;
- step 14: LINE_STEP_OVERLAP_RANGE, mapped to the range motion fault.

When a non-black sample returns, normal control immediately uses that real
input, including opposite steering evidence. Another overlap budget is granted
only after 60 ms of continuous actual directional evidence (magnitude >=20).
Short gaps and both-white cannot repeatedly reset the allowance. Both-black
does not update curve memory or reset/extend tight-turn and search timers.
An already-active search rejects it. The tight-turn maximum duration still
applies while holding an overlap. GP20/console STOP, external faults and the
30-second overall limit still take precedence.

Trace recording now captures entry to and exit from BOTH_BLACK even if they
occur between normal 100 ms samples. This improves diagnosis of short events
without changing the trace format or sensor message. Extra entries can shorten
the retained time span of the existing 64-row ring. Precise simultaneous raw
IR values are still not recorded in that ring.

## Validation and review

All 38 host test executables passed via local runner
`tools/dashboard/.local/validate_gp20_overlap.py`; complete results are in
`tools/dashboard/.local/gp20-overlap-validation.log`.

New regression sources:

- `testing/subsystem2/line_overlap_test.c`: both turn directions, real opposite
  evidence, pause preservation and bounded restart, strict admission, recent
  versus stale curve context, 120 ms/80-count limits, flicker, confirmation,
  reset and unsigned timer wrap. Missing/stale/faulty/malformed input, epoch
  mismatch, encoder faults, disconnect, run expiry and tight-turn timeout are
  also exercised. Compiled for both search and ordinary line profiles.
- `testing/subsystem2/line_overlap_owner_test.c`: actual motion owner with fake
  hardware, checking motor outputs end at zero after persistent overlap,
  excessive travel, STOP during overlap, stale input, external fault and
  sensor fault. A short overlap clears and resumes; trace entry/exit survives
  the normal sampling interval. The tests also retain bounded startup output.

Existing regressions cover startup, paused encoder settling, handoff, ADC raw
sample propagation, speed feedback, curve/search/tight-turn decisions and
other firmware profiles. Code review confirmed that the new state cannot
start a run, bypass sample freshness or grant extra search/tight-turn time.
No additional speed/gain/placement changes accompany this patch.

Reproduce the focused checks with GCC from the repository root:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -DMOTION_LINE_DEMO=1 -DMOTION_LINE_CIRCUIT=1 -DMOTION_LINE_SEARCH=1 testing/subsystem2/line_overlap_test.c app_program/subsystem2/motion_logic.c -o "$env:TEMP/robocar-line-overlap.exe"
& "$env:TEMP/robocar-line-overlap.exe"
gcc -std=c11 -Wall -Wextra -Werror -DMOTION_LINE_DEMO=1 -DMOTION_LINE_CIRCUIT=1 -DMOTION_LINE_SEARCH=1 -Wno-type-limits -Wno-sign-compare -Iinclude testing/subsystem2/line_overlap_owner_test.c app_program/subsystem2/motion_logic.c -o "$env:TEMP/robocar-line-overlap-owner.exe"
& "$env:TEMP/robocar-line-overlap-owner.exe"
```

## Artifact and physical check

Firmware build completed without compiler warnings/errors, text/data/BSS
398140/184/90108 bytes. Profile SMP1 / USB CDC / CYW43 / LWIP / MQTT /
LINE_DEMO=1 / LINE_CIRCUIT=1 / LINE_AUTOSTART=0 / LINE_SEARCH=1.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_search.uf2`

SHA256: `5D37B98A78FFD9ABA6B6B334EFD6C1161774793D96C51F21F9BEFD5517267F2B`

Build log: `tools/dashboard/.local/gp20-overlap-build.log`.
The currently installed EFC9109B image was hash-verified and backed up to
`tools/dashboard/.local/gp20-curve-trigger-installed-efc910.uf2` before building.

The initial flash attempt was blocked because the Pico remained in normal USB
mode. The subsequent `video_2026-11-03_23-50-51.mp4` is therefore not evidence
of this candidate's performance. A read-only history/version attempt was
interrupted by USB disconnection before its output was saved, so no new stop
reason is claimed for that video. The user then explicitly entered BOOTSEL
and requested another flash attempt.

The sole E: RPI-RP2 volume, Board-ID RPI-RP2 and exact candidate hash above
were verified, then the candidate was copied successfully. Stationary checks:
`tools/dashboard/.local/gp20-overlap-stationary-20261103-235241.{log,json}`.
The new OVERLAP banner and the existing startup/curve/tight/search banners all
matched. Six snapshots showed zero motor outputs, no motion faults, GP20
unpressed/armed and LED armed mode. All six IR snapshots had zero faults and
increasing sample counts; stationary GP28 raw was 220..242 (not a contrast
calibration). Only help and diagnostic commands were sent, with zero motor
start commands. The candidate is now installed; physical driving is untested.

The next run should use the same placement/track
so the effect of the stop-condition change can be assessed. Retain power after
an unexpected stop and collect history before reset or another run.

The retained physical trace does not reveal how long both-black would have
lasted without the old immediate stop. The 120 ms window may be insufficient
for a real overlap, and a true junction or persistent overlap should still
stop. Repeated steering corrections remain visible in the last video. Software
checks cannot certify circuit completion or settle all mechanical/optical
uncertainties; this candidate addresses the specific observed rejection.

## First physical run: overlap clears, later tight-turn timeout

Video: `C:/Users/joell/Downloads/Telegram Desktop/video_2026-11-03_23-56-00.mp4`
(20.593 s). The car proceeds farther around the top bend, rotating toward the
left-hand part of the track, then stops near the end of the clip. Starting
button activity occupies the early part of the video; video timestamps are
not the firmware's motor-run elapsed times. Exact optical footprints remain
partly hidden by wiring, so trajectory alone is not a definitive fault report.

The user preserved power and USB connection. Read-only `h`/`i`/`t` evidence:
`tools/dashboard/.local/gp20-overlap-result-20261103-235708.{log,json}`.
The OVERLAP banner is present. All 64 retained rows were collected, epoch/id
3/2, with consistent duplicates and no capture error. History starts at
7617 ms. The improved local reader saves received text and parsed metadata
incrementally, so a later USB interruption need not discard the whole capture.

The new behavior worked at firmware elapsed 11497 ms: state BOTH_BLACK was
accepted, targets 0/420 while ramping toward the bounded hold. At 11517 ms,
the next control sample reports EDGE_ONLY / error -12 and normal control
resumes. The observed interval between those samples is 20 ms, not a precise
measurement of the optical event's onset or duration.

Final stop: elapsed 13757 ms, step 9 LINE_STEP_TIGHT_TIMEOUT, timeout fault
0x4. Fresh line state EDGE_ONLY, error +56, age 0 ms. Search remained inactive;
no overlap expiry, motor stall, encoder fault or acquisition/handoff fault
was reported. Fresh diagnostics confirm final electrical duty 0/0.

The final uninterrupted tight turn lasted to the 800 ms limit. Its exact
entry is between retained 12917 and 13017 ms samples (12957 ms is inferred
from the timeout and configured duration, not a retained entry timestamp).
Its target was left 0 / right 500 cps. Retained measurements:

| Elapsed ms | Actual error | Filtered left / right cps |
| --- | --- | --- |
| 13317 | +88 | 0 / 479 |
| 13517 | +87 | 0 / 485 |
| 13617 | +84 | 0 / 469 |
| 13717 | +67 | 0 / 492 |
| 13757 | +56, terminal | 0 / 495 (last successful control update) |

The left wheel's stop is intentional, and the right wheel is following its
target while the actual IR error begins falling. The exit rule requires
same-side magnitude <=45 for 100 ms or opposite evidence; +56 has not met it.
`line_tight_step()` checks the fixed 800 ms timeout before those exit checks.
The terminal trace's prior speed/targets do not imply powered output after
fault_stop, which sets both electrical outputs to zero.

Recommendation: review the fixed tight-turn deadline for sustained curves.
A short, nonrenewable extension gated by fresh improving directional input
and encoder progress, with an absolute time/travel limit, would address this
case more selectively than removing the timeout. The evidence does not prove
that extra time will complete the curve, and repeated pause/restart corrections
remain visible. No firmware changes, flashing or motor-start commands were
made during this investigation; 5D37B98A remains installed.

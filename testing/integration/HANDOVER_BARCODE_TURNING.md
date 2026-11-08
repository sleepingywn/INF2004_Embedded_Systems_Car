# Handover: barcode, junction turns and V5 (for the next buddy)

Reviewed 8 Nov 2026 by Buddy 4 (Andy). Branch head `510cf9b` (Ken, 6 Nov).
Nothing in this review was pushed. Read this together with Ken's
`testing/subsystem3/ir_line_bench/BARCODE_HANDOFF.md`, which is accurate.

## 1. Where things stand

- **Barcode reading works again with the hump.** It works after Ken moved to
  fixed raw-ADC thresholds (white ≤ 200, black ≥ 240) and 333 µs sampling.
  The one open item is that the barcode sensor height keeps changing.
- **Junction turns are hard-coded.** A = right arc and B = left arc. A
  junction is detected when both line sensors read black for 20 ms. The
  decoded letter does **not** choose the turn yet.
- **After a turn the car finds the line again but does not keep following
  it.** The car picks up the outgoing line after the arc, then fails to
  trace it. The left turn has not been tested on the track.

**All of this lives only in the standalone Pico SDK bench**
(`testing/subsystem3/ir_line_bench`, `-DBARCODE_TERMINAL=ON`). None of it
is in the µT-Kernel car firmware. There, line following is Joel's V5 in
`app_program/subsystem2`, and a decoded A/B/D barcode simply stops the run.

## 2. Commits since `feature/barcode-terminal` (33b96c3)

| Commit | What |
|---|---|
| 1bbb41b, 4083fb3 | Fixed raw-ADC barcode thresholds, plus raw diagnostics |
| 2ea77c2 | 333 µs barcode sampling. The quiet time is now counted in samples (`BC_QUIET_SAMPLES`). Production still defaults to 1000 µs, so nothing changes there. |
| c88f455 | First try: A as a 90° right turn at startup |
| 4904784 | Right turn at junctions; C = straight 100 mm, D = 180° U-turn (pivot, about 2667 counts per wheel) |
| fe13267, 120da5c | Acquire and trace the right branch after the arc; tighter arc |
| 52969cc, 510cf9b | Left junction trial; B is now the automatic junction turn |

**Checks on 8 Nov:**
- every host test passes: the scan_drive mock, barcode_terminal in both
  profiles, and Joel's 38 regression + 8 adaptive tests;
- the bench image and the kernel V5 line image both build with 0 warnings.

## 3. How the junction trial works (`scan_drive.h`)

1. **Approach.** After a 2 s delay the car drives straight at 450 counts/s
   for up to 8 s.
2. **Junction.** Both line sensors at 75 or more (`LINE_BOTH_BLACK`) for
   20 ms starts an arc: 200 / 750 counts/s, inner and outer wheel.
3. **Exit acquisition.**
   - The arc must have turned at least 150 counts of wheel difference
     (about 5°) before the turn-side sensor counts. After 300 counts (about
     10°), either sensor counts.
   - A "sighting" needs that sensor at 35 or more and at least 10 more than
     the other sensor, held for 10 ms, and not both black.
4. **Line trace.** Uses `motion_line_targets(error, 450, 750, gain 3.0)`:
   - plain proportional steering: left = 450 − 3·err, right = 450 + 3·err,
     correction capped at ±300;
   - stops on Buddy 3 `LINE_LOST` (2 s of both white), stale data (100 ms),
     or after 8 s (reason 17).

## 4. Likely reasons tracing fails after the turn (to confirm with a log)

1. **The trace controller is the simplest possible.** It is plain P steering:
   - no inner-wheel pause;
   - no memory of which side the line left from;
   - both white gives error 0, so the car drives **straight**.

   After a 90° arc the car is still at an angle to the branch. The line
   crosses the sensor and leaves, and the car drives straight off it. It
   stops 2 s later (reason 18) or at 8 s (reason 17). Joel's V5 controller
   (`line_adaptive.h` / `line_follow.h`) already handles this: pivots, an
   uncertain mode and search sweeps. It is plain header C with no kernel
   calls, so the bench could call `line_adaptive_step()` in the trace phase
   instead of `motion_line_targets()`.
2. **Acquisition happens too early.**
   - After only about 5° of a 90° arc, the turn-side sensor can already
     count a "sighting". At a junction that may be the rest of the crossing
     bar or the corner, not the branch itself.
   - The car then switches to tracing while pointing about 80° away from
     the branch.

   The log line `RIGHT/LEFT EXIT: line acquired (A=… B=… err=…)` together
   with the heading at that moment would show this. A safer rule: acquire
   only after about 45–60° of turn, or when the **outer** sensor crosses the
   line.
3. **The line sensors are too close together.**
   - On the car on 4 Nov, the two line sensor boards were mounted touching,
     so their spots are about 15–17 mm apart, roughly the line width.
   - Buddy 3's OUTSIDE mode expects the line to fit **between** the sensors,
     with both white when centred.
   - With them touching, "centred" reads both partly dark, the error jumps
     on and off, and "both white" only happens once the line has already
     slid out.

   This caused jerky line following and lost lines in tight curves in every
   test before the boards were spread out. **Check that the boards have
   been spread apart** so that each spot sits 4–6 mm outside the line
   edge.
4. **Tracing stops after 8 s** (`SCAN_LINE_FOLLOW_US`), even if it is going
   well. Reason 17 is that limit, not a failure.

## 5. Suggested next steps

1. **Get a log of one failed right-turn trace.** Send `p` for live
   readings, and note the `EXIT` line, the `drive=` phases and the final
   `DRIVE stopped: reason=`.
   - 18 = line lost or stale;
   - 17 = the 8 s trace limit;
   - 16 = the exit was never found.
2. **Confirm the line sensor spacing** (section 4.3).
3. **Hand the trace phase to Joel's V5 controller** (`line_adaptive_step`)
   instead of plain P. Optionally hand over only once the car has turned
   most of the way.
4. **Move acquisition later in the arc.**
5. **Let the decoded letter choose the turn.** Use the agreed mapping:
   - the project notes (`README_MOTION.md`) say A = left, B = right,
     C = straight, D = U-turn; turn angles are + for left, so −90° is a
     right turn;
   - the trial currently uses A = right and B = left on purpose;
   - agree one mapping with the team before integrating.
6. **Integration into the kernel firmware, later.** Barcode widths there are
   still 8-bit with a 150 ms quiet time, which is too short for this
   barcode at line-following speed. Measuring widths by encoder distance is
   the tribunal's fallback.

## 6. Update (8 Nov, later): V5 + barcode + turns integrated in the bench

New local branch **`local/v5-barcode-turning`** (commit `939fc36`, on top of
`barcode-turning` 510cf9b). **Not pushed**; Andy pushes. Only bench files
changed: `scan_drive.h`, `ir_line_bench.c`, `CMakeLists.txt`,
`testing/subsystem3/scan_drive_test.c`. Kernel firmware is untouched.

**Why the bench and not the kernel:** the bench already had working barcode
capture (16-bit widths, 1.5 s quiet, 333 µs) and the junction arcs. Joel's V5
is header-only C, so it drops in unchanged. Porting to the kernel would first
need the barcode capture changes there (8-bit widths / 150 ms quiet are too
short), so it was the bigger job.

**How it runs**
1. Startup (2 s delay) or GP20 press/release starts **V5 line following**:
   `line_follow_step_observed()` with the same profile as the kernel V5 image
   (`MOTION_LINE_DEMO=1 MOTION_LINE_CIRCUIT=1 MOTION_LINE_ADAPTIVE=1`). It is
   called from the 5 ms motor timer every ≥20 ms. Encoder counts are fed with
   the kernel's raw signs (left negative forward).
2. Barcode samples are accepted during V5. A full 29-element decode of
   **A/B/C/D arms one manoeuvre** for the next junction (`ROUTE: X armed`).
   Z or partial captures arm nothing. A later barcode replaces the armed one.
3. **Junction** (both black 20 ms) with a route armed:
   A = right arc, B = left arc (Ken's mapping), C = straight 100 mm,
   D = 180° pivot. The route is used once. With nothing armed, V5 handles the
   both-black mark itself (straight across), as on Joel's branch.
4. After an arc, exit acquisition (Ken's rule, unchanged) hands over to
   **V5**, not plain P. After C/D completes (reason 14/12) V5 resumes after
   300 ms in the same session.
5. Serial `s` still runs Ken's straight scan-only pass with retries.

**New stop reasons:** 17 = V5 30 s limit; 19 = V5 stopped (the log prints
`line_step`, `adaptive_mode`, `stop_reason`). 18 now only means stale/invalid
line data during an arc.

**Tunables to check on the track**
- `SCAN_ROUTE_MIN_TRAVEL` (default 0): if the line sensors pass over the
  barcode *after* it decodes, they will see both black and trigger the turn
  early. Set this to the encoder counts between the barcode sensor and the
  line sensors.
- `SCAN_ARC_MIN_LEAD` / `SCAN_ARC_EITHER_SIDE_LEAD`: exit acquisition is still
  early (~5°/10°). V5 should recover from a sharp angle (pivot + search), but
  raise these if it locks onto the crossbar.
- Barcode widths are time-based, so V5's speed changes (250–600) while
  scanning could hurt decoding. Rank-only decoding tolerates some of it.

**Checks:** scan_drive mock test (new V5 cases: start, A/B/C/D, resume, unarmed
junction, fault stop) and barcode_terminal tests pass; bench UF2 builds with
0 warnings. Not yet tested on the car.

## 7. Status at end of 8 Nov (supersedes section 6 where they differ)

**Bench: works on the track.** Branch `local/v5-barcode-turning`, head `fb9ade7` (W3).
- **Full run, consistently:** barcode A, right turn, V5 follows; then barcode Z,
  left turn, V5 follows. Tested on Andy's mat.
- **Key fixes after section 6:**
  - W1 keeps the arc's motor power when V5 takes over. Without it, V5's
    80‰ from-rest start stalled the inner wheel.
  - W2 adds Z = left (test mapping).
  - W3 adds V5 stop-detail logging only.
- **Decoder:** A–Z, bars and spaces classified separately. Distance first,
  time as fallback; if they disagree, nothing is armed.
- **Discarded:** V6–V8 (steady drive while scanning) caused stalls, and W4
  (ignoring the outer bars) was rejected by Andy as unsafe. Neither is on the
  branch.
- **Physical lessons:**
  - Barcode sensor height is critical: the small Z barcode only reads at the
    lowered height.
  - No direct sunlight.
  - Barcodes go on the car's left, off the black line, on a straight, with
    clean white after the last bar. A crossing or paper edge just past the
    last bar corrupts the capture.
- **Thresholds:** Andy asked that NO thresholds be changed (junction 75, V5,
  raw barcode cutoffs, Ken's exit rule).

**Kernel port: built and host-tested, not yet tested on the car.** Branch
`local/kernel-v5-barcode-route` (`1ad88c4`, `0ecefa4`) on Joel's V5
`d3a939a`. See `testing/integration/LINE_ROUTE_KERNEL.md` on that branch.
- **Build:** profile `LINE_ROUTE=1`, built in the same profile as the K1 V5
  image.
- **Tests:** `python testing/subsystem2/run_route_tests.py`. Joel's 38 + 8
  suites still pass.
- **Sampling:** the kernel keeps 1 kHz IR sampling; the bench used 333 µs.
  Distance widths come from motion-task odometry history.

Nothing from this session has been pushed. Andy pushes.

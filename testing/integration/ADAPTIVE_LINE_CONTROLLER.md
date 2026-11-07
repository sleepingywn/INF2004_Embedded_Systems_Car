# General line controller: first integration stage

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

2026-11-04. Initial software implementation followed by an approved flash and

stationary verification (see flash record below). No movement test of this

profile yet. This is a foundation for varied courses, not a claim of universal

track completion.

## Objective and boundaries

Project FR1 requires continuous autonomous black-line following. FR2/FR3 add

barcode decoding and left/right/straight/U-turn commands. A controller for the

whole project therefore needs both local steering and route decisions.

The previous run stopped with `LINE_STEP_TIGHT_TIMEOUT` after a fixed 800 ms

inside-wheel pause even while error was decreasing. Extending that deadline

would not establish general course handling. The new optional profile removes

that steering state machine entirely. There is no turn count, course location,

or expected curve duration in its tracking controller.

The existing GP20 test envelope remains: deliberate press/release, at most

30 seconds, no automatic power-on movement, maximum 20% PWM, STOP and health

guards. Those are experiment limits, not an autonomous navigation policy.

## Implemented

- The S2 IR bridge publishes both calibrated 0â€“100 readings atomically with

  the existing line message, timestamp and epoch. The packed S3 line-message

  format is unchanged. Old publishers explicitly clear pair validity.

- `line_adaptive.h` is a deterministic controller with no hardware access. It

  uses filtered left-minus-right error and its rate of change to set wheel

  speeds. Speed decreases as steering increases. Encoder-based wheel-speed

  feedback remains in the existing motor owner.

- Strong opposite sensor evidence overrides the direction of old filtered

  steering. No timer stops a continuously observed curve just because it lasts

  longer than a previous curve.

- The motor's provisional minimum rolling speed is retained. Requests below

  that speed choose an inside-wheel pause with hysteresis, avoiding repeated

  requests for an unreliable crawl. Startup/restart power and normal ramps

  remain bounded; encoder settling permits only one backward count during a

  continuous intentional pause.

- Both-white is explicitly uncertain with this OUTSIDE sensor placement.

  Limited slow travel is followed by bounded pivots using differential encoder

  counts. Recovery first searches toward the last observed side, then the

  opposite side. Without a previous direction the initial tie-break is left.

  Brief sightings can steer immediately but require sustained visibility to

  reset the recovery budget. Exhaustion stops with step 16.

- Both-black is an unresolved mark, not proof of a branch direction. Outputs

  stop while it is classified. A continuous mark for 100 ms ends the trial

  with step 15 (`ROUTE_REQUIRED`), reported as cancellation without an IR fault.

  A brief mark that clears resumes from current sensor readings.

- Stopped-run history and live diagnostics include the packed pair and

  controller mode. Mode transitions are retained even between the usual

  100 ms history samples. Existing row fields remain before the appended fields.

Modes: 0 tracking, 1 uncertain, 2 searching, 3 unresolved mark, 4 route required,

5 recovery exhausted, 6 invalid input. Decode `levels` only with

`line_levels_valid`; left is bits 0â€“6, right bits 7â€“13, validity bit 15.

## Calibration versus course-specific behavior

`line_adaptive_config` holds one configuration for all layouts: feedback gains,

filtering, visibility hysteresis, motor speeds, and recovery limits. The current

starting values include 450 counts/s cruise, 250 minimum rolling, 600 maximum,

120 counts of uncertain travel, 160/320 differential-count sweeps, and an

800-count/3000-ms overall recovery bound. These are provisional engineering

limits, not measured distances, angles, or validated optimum settings.

The existing wheel controller still owns its gains, startup power, output cap,

and stall detection. Calibration of those hardware properties is legitimate;

changing them for each bend to make one video succeed is not the acceptance

criterion. Freeze one hardware configuration before evaluating unseen layouts.

## Important unfinished work

1. **Route/navigation integration.** The trial does not consume barcode route

   commands or execute junction/U-turn manoeuvres. The calibrated production

   FOLLOW path remains disabled by `MOTION_CALIBRATED=0`. Step 15 is a diagnostic

   boundary for the future navigation adapter, not a completed route handoff.

2. **Observation ambiguity.** Two sensors seeing white cannot distinguish a

   centered narrow line from a missing line. Two seeing black cannot distinguish

   a wide curve, stripe, or junction. In particular, stopping on a mark may keep

   the sensors on that mark and require route handling rather than spontaneous

   recovery. This profile can therefore still stop on an ordinary wide bend.

   Software cannot guarantee arbitrary patterns from identical observations.

   A different placement or more sensing coverage may ultimately be needed.

3. **Physical validation.** Synthetic sensor/encoder tests validate transitions

   and guards, not tyre traction, sensor footprints, power-bank response, or

   motor dynamics. Gains and recovery travel must be checked on hardware.

4. **Mission duration.** The 30-second GP20 bound is retained during development.

   Removing it requires a deliberate mission lifecycle, completion criteria,

   and a tested stop mechanism; it is not silently removed here.

Next engineering stages are to connect decoded route commands to explicit

approach/turn/reacquire states, validate command freshness and cancellation,

then evaluate multiple layouts with unchanged configuration. Do not present

this first stage as a solution to all project requirements.

## Software validation

Run with GCC on PATH (no serial access):

```text

python testing/subsystem2/run_adaptive_tests.py

```

Four executable suites plus two rejected-build-profile checks cover:

- left/right symmetry, multi-second curves, curvature reversals, straights;

- unchanged configuration at 20/40/60 ms and clock wraparound;

- uncertain startup, distance-bounded sweeps, noisy sightings, reacquisition;

- short marks versus route-required stop; latched terminal states;

- the real line wrapper, PI pipeline, stale/epoch-invalid input, invalid encoder

  edges, disconnected input, missing levels, stall, bounded encoder settling;

- nine actual motion-owner scenarios ending with both electrical outputs zero,

  including STOP, external health fault, route-required and failed recovery;

- actual S3-to-S2 handoff races, atomic pair publication, and GP20 console guards.

The existing 38-executable regression suite also passed after integration.

The firmware compiled for Pico W SMP/USB/Wi-Fi/MQTT with no build warnings.

No motor commands, flashing or hardware tests were performed for this stage.

## Build and review

Use `LINE_ADAPTIVE=1 LINE_SEARCH=0 LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0`

with the established SMP=1 USB/Wi-Fi/MQTT build. Adaptive and legacy search

cannot be combined, and adaptive autostart is rejected. The profile identifier

forces objects to rebuild when switching modes.

Output: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_adaptive.uf2`.

The separate `_search.uf2` artifact is preserved for rollback. Before any later

flash, verify the artifact hash recorded in the accompanying local build

manifest; after an approved flash, verify the `ADAPTIVE v1` banner and stopped

outputs before asking for a physical run.

## Physical acceptance sequence â€” requires approval, not performed

1. Stationary white/black/centered/left/right readings at actual running height;

   reject poor contrast or an invalid sensor before motion.

2. Lifted-wheel checks for both turn directions, soft startup and GP20 STOP;

   verify requested speeds against measured encoder speeds.

3. Low-speed straight and left/right curves, then consecutive opposite curves

   and varying curvature. Collect both sensor levels, requested/measured speeds,

   mode and stop reason. Keep the same settings across layouts.

4. After route integration, test junctions, barcode commands and U-turns plus

   missing lines, conflicting commands, disconnects and sensor failures.

5. Use unseen layouts and repeated runs with different battery states to assess

   generalization. Record failures rather than adjusting a timer for each bend.

## Approved flash and stationary verification

Following explicit user approval with both wheels lifted, the adaptive image

was copied to the sole RPI-RP2 BOOTSEL drive after verifying its board ID and

SHA256 `D42B0183C5D13FD94B2445D85553BF4B2E410B406A56A12C8D8F03735893D128`.

The rebooted firmware reported `ADAPTIVE v1`. Six status samples showed both

motor duties zero, no motion faults, and GP20 armed/unpressed. IR acquisition

was running without reported faults; these lifted-wheel readings do not

validate black/white contrast at running height. Only help/status commands

were sent; no motor-start command or movement test was performed.

Evidence: `tools/dashboard/.local/gp20-adaptive-stationary-20261104-003513.json`

and the matching `.log`. The local build manifest records the flashed hash.

## First physical run and v2 correction â€” v2 not flashed

The user reported stopping before the bend and preserved power while connecting

USB. Read-only help/status/history commands recovered all 64 retained rows,

epoch/id 3/2, without row conflicts. Evidence:

`tools/dashboard/.local/gp20-adaptive-result-20261104-003808.{json,log}`.

The current stop was at 5486 ms, step 4, encoder reason `0x20` (right count

receding), fault `0x1`. The right raw count moved from 1780 to 1778 while its

target and duty were zero. The one-count intentional-pause settling allowance

therefore rejected the sample. IR acquisition reported no faults, no handoff

errors, and fresh input. Both motor outputs were zero in the subsequent status

snapshots. This was not the 30-second limit or route-required stop. Old startup

text in the serial buffer is not the stop record for request 2.

The retained history shows repeated opposite corrections, wheel pauses, and

tracking/uncertain/search transitions. It does not prove whether the physical

backward counts were tyre rollback, vibration or electrical encoder effects.

It does establish abrupt commanded steering as an issue worth correcting.

The new continuity regression failed on v1: a single calibrated 40/3 reading

after an uncertain gap requested targets 0/598. V1 reset the filtered error to

each newly visible reading, bypassing smoothing at precisely those transitions.

V2 changes:

- Preserve and decay filtered error through ambiguous gaps; do not snap to the

  raw reading at reacquisition. Opposite evidence cancels the old direction.

- Reduce proportional/derivative gains from 6000/80 to 3500/40 (existing milli

  units), so weak edge sightings produce rolling corrections.

- Permit tracking pauses only after proportional demand and raw direction agree

  continuously for 60 ms. Derivative spikes alone cannot initiate a pause.

  Ambiguity or opposite evidence clears the pending confirmation. This is input

  debounce, not a deadline on how long an observed curve may last.

- Retain the encoder backward-count limit, fault handling, recovery limits,

  motor output cap, GP20 behavior and overall test duration.

Five adaptive test executables now pass, including intermittent opposite edge

sightings, isolated strong readings, sustained curves, clock wrap and 20/40/60 ms

timing. The existing 38 regression executables and two invalid-profile checks

also pass. These are software checks, not proof the next physical run will pass.

The encoder fault may still recur if rollback/noise exceeds the existing bound.

V2 firmware builds without warnings; text/data/BSS 397692/184/90700. SHA256:

`16014709A5E9E80020F5954FD983E1E737C8703B2B838B595D156F8D77D4DB5D`.

The expected banner begins `ADAPTIVE v2: continuous error filter, confirmed

tracking pauses`. The new local manifest is `adaptive-v2-manifest.json`.

The installed v1 artifact was preserved as

`tools/dashboard/.local/adaptive-v1-installed-d42b0183.uf2`. No v2 flash or

movement command was performed during this investigation; request approval

before installing it. Route handling and sensor-placement limitations remain.

## V2 approved flash and stationary check

After the user replied “ready to flash,” the v2 image above was hash-verified

and copied to the sole RPI-RP2 BOOTSEL drive. Its normal USB startup reported

ADAPTIVE v2. Six status snapshots showed zero duties, no motion faults, and

GP20 armed/unpressed. IR acquisition reported no faults. Only h/i read commands

were sent; no movement was started. Evidence:

`tools/dashboard/.local/gp20-adaptive-v2-stationary-20261104-004529.{json,log}`.

The v2 manifest now records installation and stationary verification. The next

physical course run remains unverified.

## V2 recorded course run: crossing/mark stop

Video: C:/Users/joell/Downloads/Telegram Desktop/video_2026-11-04_00-49-34.mp4
(20.10 seconds). The user confirmed an uncommanded stop and preserved power.
The full-run frame sequence and half-second exit sequence show traversal of the
initial straight and semicircular bend, followed by heading corrections onto
the next straight. The final position is at the transverse black stripe.
Exact sensing spots are partly obscured in the video.

Read-only capture: tools/dashboard/.local/gp20-adaptive-v2-result-20261104-005358.json
and matching .log. All 64 rows were recovered, epoch/id 3/2, without conflicts,
with the ADAPTIVE v2 banner.

| Run elapsed | Left/right normalized | Controller mode | Result |
| --- | --- | --- | --- |
| 17061 ms | 66 / 7 | tracking | targets 250 / 568 counts/s |
| 17161 ms | 100 / 80 | unresolved mark | both targets and duties zero |
| 17261 ms | 100 / 91 | route required | step 15, trial cancelled |

Fresh stopped status shows motion faults 0x0, both outputs zero, IR faults 0x0,
and zero handoff errors. This captured stop is the programmed both-black route
boundary, not an encoder reversal, stall, loss timeout or 30-second limit.
The crossing stripe is a plausible cause supported by the video location and
saved readings; the two sensors alone do not establish branch geometry.

Other unexpected stops mentioned by the user have no saved reason in this
capture, so this diagnosis cannot be applied to all runs. One run demonstrates
bend traversal, not repeatable arbitrary-course completion. The next functional
work is crossing/junction route handling with bounded reacquisition, keeping
motion-fault checks intact. No steering code change, flashing or motor-start
command was performed during this investigation.

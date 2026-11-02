# Buddy 2 calibration, PID tuning and accuracy evidence

Status: Partial bench measurements recorded on 2026-11-02; see the dated
results below. Earlier pending entries are a historical checklist, not a claim
that no hardware has been tested. PID tuning and loaded-motion accuracy remain
unvalidated. Hardware tests require the user's approval.

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

## Powered encoder measurements — 2026-11-02

Both wheels lifted; independent single-wheel runs at 20% duty for 2000 ms,
followed by 250 ms settling capture. No counts/revolution constant controlled
these pulses. The user counted tape revolutions as the independent reference.

| Wheel | Signed raw delta | Physical turns reported by user | Provisional x4 counts/rev |
|---|---:|---|---|
| Right | +3874 | Slightly over 1.5 | Not precisely determined; exactly 1.5 would imply 2582.67, more turns implies lower |
| Left | -3831 | Exactly 1.5 | 2554, single visual observation; repeat required |

Both captures reported zero invalid transitions, no sample-buffer overflow,
no opposite-wheel count change during the pulse, and stable counts over the
last 100 ms. Forward raw-count signs match existing settings. Each run produced
115 timed samples; firmware completion and final stop were acknowledged.
Physical revolution observations have not been independently video-checked.

These unloaded runs do not establish straight driving, loaded speed, motor
power trim, wheel circumference, PID gains or final production calibration.
No calibration constant or MOTION_CALIBRATED flag was changed from these results.
Full setup, evidence locations and firmware hash: encoder_bench/README.md.

Left repeat: raw delta -3841, opposite wheel unchanged, no invalid transitions
or overflow, final counts stable. User reported slightly over 1.5 turns.
Count totals differ by about 0.26% between the two left runs; the exact turn
fraction remains unknown. Keep 2554 counts/rev provisional and do not average
with a fabricated second turn count. No production settings were changed.

Left video run (firmware id 4): same 20% / 2000 ms command, raw delta -2680,
right delta 0, zero invalid transitions, 115 samples, no overflow and stable
final counts. Completion and final stop acknowledged. Video/physical turn
measurement pending. Count magnitude is approximately 30.2% below the previous
left run; cause unknown. No counts/revolution estimate or production-setting
change is justified until the independent turn measurement is available.

Left repeat (firmware id 5): same 20% / 2000 ms, raw delta -3835, right delta 0,
zero invalid transitions, 115 samples, no overflow and stable final counts.
Completion and final stop acknowledged. Physical turns/video pending. This
returns to within 0.16% of the first two left count magnitudes; the intervening
2680-count run remains unexplained. No production calibration changes.

Video review - 2026-11-02: user supplied video_2026-11-02_17-04-08.mp4
following firmware run id 5. Inspected sampled frames spanning the entire clip
and enlarged stationary start/end frames. The orange marker starts near the
bottom, passes right/top/left/bottom once, then right/top, ending slightly past
the opposite starting position. Independent visual estimate: approximately
1.5 revolutions, slightly more; no precise fractional-angle calibration claimed.
Camera/chassis motion and perspective limit endpoint measurement accuracy.
Assuming this clip corresponds to id 5 (-3835 counts), exactly 1.5 revolutions
would imply 2556.67 x4 counts/rev; slightly more revolutions implies a lower
value. This broadly supports the earlier provisional estimate, but does not
resolve the 2680-count outlier or establish right-wheel calibration. Production
settings remain unchanged. Extracted review images are in the ignored
tools/dashboard/.local/video-analysis directory.

Right video run (firmware id 6): 20% / 2000 ms, right raw delta +3835,
left delta 0, zero invalid transitions, 115 samples, no overflow and stable
final counts. Completion and final stop acknowledged. Physical turns/video
pending; equal count magnitude to left id 5 does not independently establish
equal counts/revolution or loaded wheel speed. No production settings changed.

Right-wheel video review - 2026-11-02: supplied
video_2026-11-02_17-06-51.mp4 following firmware run id 6 (+3835 counts).
Reviewed 30 sampled frames across the entire clip and enlarged stationary
start/end frames 61 and 153. Marker travels from bottom through left/top/right,
returns to bottom, then passes left and finishes near top. This independently
supports approximately 1.5 output-wheel turns; no precise endpoint angle is
claimed because camera/chassis movement and perspective affect the view.
Assuming the clip corresponds to id 6, 3835 / 1.5 = 2556.67 x4 counts/revolution
is an approximate estimate, not a verified exact constant. Both latest videos
are consistent with roughly 2550 counts/revolution on each wheel, but do not
establish exact equality, loaded speed matching or straight driving. The prior
2680-count left pulse remains unexplained. Production settings unchanged.
Review images: ignored tools/dashboard/.local/video-analysis/right-overview.jpg
and right-endpoints.jpg. No motor command or flash was performed for this review.

## First encoder-feedback floor run - 2026-11-02

User authorised implementation/testing and confirmed a flat floor. Added the
standalone feedback_bench: 500 ms pulse, 20% maximum duty, P-only accumulated
wheel-rotation synchronisation, 14% minimum, 0.5 percentage-point slew per
20 ms update. Provisional 2550 counts/revolution on both sides; no production
calibration flag or constants changed. Host safety/controller checks and Pico
SDK build passed; see feedback_bench/README.md for limits, hash and evidence.

Flashed the bench and sent one forward command. Firmware id 1: 500707 us
powered, -699 left / +667 right raw counts, zero invalid transitions, 40 samples,
no overflow, final counts stable and final stop acknowledged. Left commanded
20% initially, then reduced to 17%; right stayed 20%. User: "Stayed straight".
This is a single short visual observation, not proof of long-distance tracking.
The 32-count total mismatch remains; no exact distance/speed claim is made.
Bench remains flashed and idle. No second floor run was sent.

One-second feedback floor check (v2, 2026-11-02), explicitly requested by user:
host checks and firmware build passed, then flashed and sent one forward run.
Actual powered duration 1000250 us, -1417 left / +1391 right raw counts,
zero invalid transitions, 65 samples, no overflow, final counts stable and
stop acknowledged. Active sampled duty left 17.2..20%, right 19.8..20%.
Final mismatch 26 counts (~1.85% of mean count magnitude); physical path
observation pending. Current bench duration is 1000 ms, travel cap 2800 counts;
other controller/stop settings unchanged. No production calibration changes.

User observation for the one-second feedback run: "SLIGHTLY veered to the
right by a tiny bit". Direction is consistent with the 26-count left lead.
No further movement or calibration/controller change was made on receipt
of this observation.

## One-second feedback repeat - 2026-11-02

After operator readiness, repeated the unchanged v2 settings with exactly
one forward command. Firmware id 2: powered 1000164 us, total capture
1250949 us; left raw delta -1466, right +1439, zero invalid transitions,
65 samples and no overflow. Final counts stable; completion and final stop
acknowledged. Sampled active duty left 17.7..20%, right 18.9..20%.
Left final lead 27 counts (~1.86% of mean count magnitude), similar to the
prior 26-count lead. Physical path observation pending. No further run sent.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-171705
JSON/CSV/log.

User confirmed the one-second repeat also drifted a tiny amount right. Both
runs therefore show the same observed direction and similar 26/27-count left
lead. Prepared feedback bench v3 with proportional rotation gain raised 25%
to 1.25 permille/count; all other limits unchanged. Host checks and build pass.
Not flashed or physically tested; v2 remains on the stopped robot. See
feedback_bench/README.md for prepared image hash and limits.

V3 gain-1.25 floor trial, after readiness (2026-11-02): flashed verified image
and sent one 1000 ms forward command. Powered 1000250 us, -1466 left / +1439
right, zero invalid transitions, 65 samples, no overflow, final counts stable
and final stop acknowledged. Sampled duty left 17.4..20%, right 20%. Final
27-count left lead matches previous v2 repeat; no count-agreement improvement
demonstrated. Physical-path observation pending; robot stopped on v3.

Two-second feedback floor run, v4 gain 1.25 (2026-11-02): explicitly requested
by user; updated host checks/build passed, verified image flashed and one run
sent. Actual powered 2000304 us, left -2782 / right +2739 raw counts, zero
invalid transitions, 115 samples, no overflow, stable final counts and final
stop acknowledged. Sampled duty left 15.5..20%, right 19.5..20%. Final left
lead 43 counts (~1.56% of mean count magnitude). Physical path observation
pending. Current bench duration 2000 ms, travel cap 5600 counts; otherwise
unchanged. Production settings unchanged. No automatic repeat.

## Second-second rightward drift analysis - 2026-11-02

User observed rightward veering after the first second of the v4 run.
CSV confirms left lead was 21 counts at 1.005232 s, 36 at powered stop
(2.000304 s), and 43 after settling. Coasting added 7 counts of mismatch;
most mismatch developed while powered. At 1.950037 s, lead was 33 counts
with left duty 15.9%, right 20%. No invalid encoder transitions were reported.
The controller is P-only accumulated-rotation feedback; any nonzero steady
power correction requires a nonzero rotation error. A carefully bounded
integral correction is a candidate for persistent bias, but has not been
implemented or validated. Its benefit/stability must not be assumed. Wheel
calibration uncertainty, slip and tether forces remain unresolved contributors.
User observation is stored with the local capture. No settings changed or
further hardware run sent during this analysis.

## Continuous speed-control implementation v5 - 2026-11-02

User requested the next improvement. Implemented independent per-wheel speed
PI in feedback_bench, using actual elapsed time and filtered encoder speed.
Provisional target 1200 normalised counts/s, feed-forward 18%, Kp 0.03 and
Ki 0.12, integral bounded +/-4 percentage points with duty/slew anti-windup.
Small accumulated rotation-error correction reduces the leading wheel target.
Active duty remains 14..20%, first physical trial still capped at two seconds;
existing stop checks retained, invalid-speed fault and speed diagnostics added.

Actual-main-loop safety/telemetry checks and five illustrative 30-second model
checks passed; firmware built with warnings as errors. Synthetic convergence
is not physical calibration or proof of straight driving. Prepared image hash
and test details: feedback_bench/README.md. No flashing or physical test was
performed for v5; the robot remains on stopped v4. Production settings unchanged.

V5 first physical trial (2026-11-02), explicitly authorised: flashed verified
image, sent one two-second forward run. Powered 2000030 us, left -2469 / right
+2378 raw counts; zero invalid transitions, 114 samples, no overflow, stable
final counts and stop acknowledged. Last powered filtered speed 1169/1138 cps
versus targets 1080/1200. Final left lead 91 counts (~3.75%), larger than v4's
43; no physical tracking improvement established. Path observation pending.
Robot stopped on v5; no repeat or gain change sent.

User confirmed v5 drifted right. Trace: left lead 59 counts at 1.006 s,
84 at powered stop and 91 after coasting. At 1.006 s, filtered left speed
1244 exceeded target 1082; right was 1180 against 1200. Prepared v6 changing
only Ki from 0.12 to 0.24 for faster correction of persistent speed error.
Software models/safety checks and firmware build pass. Corrected an obsolete
one-sided host mismatch assertion to use the same absolute 100-count bound;
the model's -1 count endpoint is valid for bidirectional mismatch correction.
V6 not flashed or physically tested. Robot stopped on v5; readiness needed.

V6 first physical trial (2026-11-02), after readiness: verified image flashed,
one two-second forward run. Powered 2000037 us, -2435 left / +2379 right,
zero invalid transitions, 114 samples, no overflow, stable final counts and
stop acknowledged. Final left lead 56 counts vs previous v5's 91. Last
filtered speeds 1162/1135 cps versus targets 1094/1200. Physical observation
pending; no repeated-improvement claim. Robot stopped on v6.

V6 path observation (2026-11-02): user reported "tiny bit of right veering".
This agrees in direction with the remaining 56-count left lead. Compared with
the preceding v5 trial, the count mismatch is lower, but repeatability remains
unverified. Recommend one identical two-second v6 repeat before further tuning.
No controller settings, flash or additional movement changed during this update.

## V6 unchanged two-second repeat - 2026-11-02

After operator readiness, sent exactly one command with unchanged v6 settings;
no reflash. Firmware id 2: powered 2000895 us, total capture 2251707 us;
left raw delta -2453, right +2392, zero invalid transitions, 114 samples,
no overflow. Final counts stable; completion and final stop acknowledged.
Sampled active duty left 14.8..20%, right 16.9..20%. Last powered filtered
speeds 1144/1111 cps, targets 1086/1200; integral -29586/+3846 milli-permille.
Each wheel had 3 output/slew-limit samples. Final left lead 61 counts versus
56 on the prior v6 trial; the residual lead is similar across these two runs.
Physical-path observation pending. Robot stopped; no automatic repeat sent.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-173745 JSON/CSV/log.

V6 repeat observation: still veered right by a bit. Two v6 trials have similar
56/61-count left leads and approximately -30/+4..5 permille integral offsets.
Prepared v7 with independent provisional feed-forward estimates 15.5% left,
18.5% right; actual startup remains 20% each before slewed speed feedback.
PI gains, targets, 14..20% output limits, duration and fault logic unchanged.
Endpoint offsets are transient evidence, not calibrated steady-state curves.
Host checks/model scenarios/build pass. V7 not flashed or physically tested;
robot stopped on v6. Production calibration unchanged.

V7 first physical trial (2026-11-02), after readiness: verified image flashed,
one two-second run. Powered 2000175 us, -2437 left / +2377 right, zero invalid
transitions, 114 samples, no overflow, stable final counts and stop acknowledged.
Final left lead 60 counts, essentially unchanged from prior v6 56/61. Last
filtered speeds 1176/1138 cps versus targets 1088/1200. Physical observation
pending; no improved tracking claim. Robot stopped on v7.

## Wheel calibration shelved; line integration prepared - 2026-11-02

User requested no more wheel-calibration trials and to integrate line tracing.
Added a guarded local two-second line mode to the full firmware, reusing B3's
line error and shared v7 speed PI. Source: forward_speed.h / line_follow.h;
standalone bench now includes the same speed core. Steering can reduce the
inner wheel below 14% to zero; outer target stays <=1200 counts/s, duty <=20%.
Metric calibration stays disabled. No B3/B4 source changed for this integration.

Actual-owner-loop host tests, IR steering/guard tests, console arm/default-profile
tests, existing motion/console regressions and both MQTT firmware builds pass.
The new image has not been flashed or physically tested. Robot remains on v7
bench firmware. IR black/white setup and GP28 response must be checked before
any line movement. See testing/integration/LINE_FOLLOW_INTEGRATION.md.

Integrated line image stationary check (2026-11-02): explicitly authorised,
flashed hash 9E1051B253EA18E5FD2838A940E568EBDB62E8624E801CB58B03D7E33F8B3F28,
verified line-profile banner on COM6 (MTK3PICO). Only h/i/x sent; no motors,
reset/arm or IR setup commands. Ten snapshots: duty/counts 0/0, inactive line,
IR sampling running without acquisition/handoff faults, no motion timeout.
IR uncalibrated source remains asserted as expected. Raw GP27 180..210 and
GP28 291..1833 do not establish contrast without known surfaces. Next is a
stationary black/white IR check; integrated movement remains untested.

## White-surface IR capture and setup begun - 2026-11-02

User confirmed all three sensors over white. Eight snapshots: GP26 157..187,
GP27 171..198, GP28 162..191 raw counts; acquisition running, no faults,
motor duty 0/0. Evidence: ignored ir-white-20261102-180647 JSON/log.
Then sent h/x/c/i only to start the existing stationary IR setup while still
on white. BEGIN accepted; state CALIBRATING, 988 calibration samples,
acquisition faults 0, no handoff errors, duty 0/0 and line inactive. No reset,
arm or motor command sent. IR setup remains accumulating min/max in RAM;
black exposure and FINISH are pending. Do not reset/reflash before completion.
Evidence: ignored tools/dashboard/.local/ir-white-calibration-begin.log.

## Black-surface response and completed IR setup - 2026-11-02

User confirmed all three sensors over black. Five raw snapshots: GP26
3085..3110, GP27 2466..2496, GP28 2216..2241. Compared with the prior white
maxima 187/198/191, conservative observed separations are 2898/2268/2025
counts, all above the existing 600-count contrast requirement. Black readings
are higher, consistent with IR_BLACK_IS_HIGH. The earlier GP28 stuck/low
contrast concern was not reproduced in this controlled surface comparison.
This does not establish dynamic track performance or electrical characteristics.

Sent FINISH once; accepted. Three subsequent checks showed IR READY (state 2),
failed_channels=0, acquisition faults=0, handoff_errors=0, live fault sources=0.
94488 calibration samples were accumulated. Max lateness 142 us, scan 218 us.
The last line snapshot was BOTH_BLACK (state 5), error 0, fault 0, age 7 ms:
expected while both line sensors are on black, and intentionally not a valid
starting placement for the new local line mode. Motor duty stayed 0/0,
line inactive. No reset/arm or motor request was sent; final STOP acknowledged.
Latched motion fault 0x40 remains until a separate explicit healthy reset.
IR calibration is now valid in RAM; keep power on. Next is normal track
placement and a stationary line-position check before any authorised movement.
Evidence: ignored tools/dashboard/.local/ir-black-finish-20261102-180905 JSON/log.

## On-track stationary blocker and prepared handoff correction - 2026-11-02

The next read-only h/i check found IR FAULT (state 3), acquisition stopped,
handoff_errors=1, acquisition faults=0 and first acquisition reason=0.
Raw samples froze at 499036 ms; the last line state was LOST and its age grew
from 1090 to 6039 ms. All six snapshots showed motor duty 0/0, line inactive,
live IR fault source 0x2 and latched motion fault 0x40. No reset, arm or movement
command was sent. Successful white/black calibration does not imply the runtime
remained healthy. Evidence: ignored ir-track-stationary-20261102-181101 JSON/log.

Code review found a concrete contention hazard: process_frame called the
nonblocking motion_get_status() and permanently stopped acquisition on any
negative return, including a single E_TMOUT while another core copied status.
This is a plausible cause, not a confirmed diagnosis of this hardware event:
the flashed image did not record the failing call or error. Timestamp pacing
uses consecutive scheduled milliseconds; LINE_LOST itself is not a bridge
handoff error. Barcode/line publish errors remain other possible causes.

Prepared motion_get_ir_status() for the IR task with 50 polling attempts then
one 2 ms kernel wait request (rounded to a kernel tick). The ordinary observer
API and control-task publisher remain nonblocking. Persistent failures still
stop acquisition and assert IR health failure; no stale-data bypass or automatic
restart was added. The small S3 adapter change also records last error, sample
time and stage (1 status read, 2 bridge); B1 prints these diagnostics. No B4
changes were made for this correction.

Host checks inject temporary contention, successful wait, exhausted wait and
non-timeout errors into actual motion.c. They pass along with all ten existing
motion owner-loop scenarios and the default-profile rejection checks. Firmware
build passed: text 387816, data 176, BSS 85588. Candidate UF2 SHA256:
9852A9857F067A1523A23D9A2FB30C58D06B5424531F60978C4A3A30B9E38F9F.
Build log: ignored tools/dashboard/.local/line-ir-handoff-build.log.
Previous flashed image preserved in ignored firmware-backups/
line-before-ir-handoff-retry-20261102.uf2.

Candidate NOT flashed or hardware-tested. The car still runs the prior image
with a latched IR fault. Flash approval is pending; a flash will require a new
white/black IR setup because calibration is RAM-only. Recheck stationary
runtime stability and usable line position before any separate motor trial.

## Handoff candidate flashed; stationary startup checked - 2026-11-02 18:18

After explicit user flash approval and manual BOOTSEL, verified E:\INFO_UF2.TXT
Board-ID RPI-RP2 and candidate SHA256
9852A9857F067A1523A23D9A2FB30C58D06B5424531F60978C4A3A30B9E38F9F before copying
UF2. Pico returned as COM6 / MTK3PICO. Updated handoff diagnostics were observed.
Stationary monitor sent only h/x/i (no reset/arm, calibration or motor request).
All ten snapshots showed duty 0/0, counts 0/0 and line inactive; final STOP
acknowledged. Acquisition ran without faults, samples 13168..40340, maximum
lateness 114 us and scan 146 us. No handoff errors; stage/error/time all zero.
USB recorded one dropped record (10 bytes), no truncation.

IR is UNCALIBRATED after flashing (failed_channels 0x7, live IR source 0x2,
latched motion fault 0x40), as expected. This startup check does NOT validate
the calibrated handoff correction: READY processing is not entered until
white/black setup completes. Next user action is all three sensors over white,
then BEGIN, black exposure, FINISH, and a calibrated stationary stability check.
No line-tracing motor run has occurred. Evidence: ignored
 tools/dashboard/.local/line-stationary-20261102-181829.{json,log}.

### White setup after handoff firmware flash - 2026-11-02 18:20

User confirmed all three sensors over white. Five stationary snapshots measured
GP26 177..200, GP27 171..196, GP28 164..192. Sent only h/x/i/c; BEGIN once,
accepted. Three subsequent snapshots confirmed CALIBRATING, acquisition running
without faults, progressing samples (3520 at last snapshot), handoff errors 0,
stage/error/time 0. Motor duty remained 0/0 and line inactive throughout.
Calibration is accumulating in RAM; do not reset. Next: all three sensors over
black, then FINISH and a calibrated stationary stability check. No motion sent.
Evidence: ignored tools/dashboard/.local/ir-white-handoff-20261102-182018 JSON/log.

### Black setup after handoff firmware flash - 2026-11-02 18:21

User confirmed all three sensors over black. Five snapshots: GP26 3036..3060,
GP27 864..889, GP28 1072..1099. Conservative separation from the preceding white
maxima: 2836/668/880 counts, above the 600 minimum but appreciably lower on the
line sensors than the preceding firmware's black placement. FINISH sent once
and accepted, READY with failed_channels=0, 76778 calibration samples.

The initial extended check stopped on an incomplete USB diagnostic response
(missing motor-status line); the available same-response IR data still showed
READY, running, no acquisition or handoff fault, line age 8 ms. A STOP was sent
on monitor failure. This is a host-observation failure, not evidence of a motor
or IR runtime fault. USB drop counter incremented by one record / 10 bytes.
Read-only stability monitoring was restarted with at most three retries for
incomplete records, without sending BEGIN or FINISH again. Missing records
are retained separately; explicit IR faults and nonzero duties abort monitoring.

Current black placement yields LINE_ON with error around -5 rather than
BOTH_BLACK. The min/max calibration includes the full manual transition, so
passing contrast alone does not prove the stationary black placement reaches
the normalized 75-percent black threshold. Verify coverage/operating height
before any motor trial; do not claim line tracing verified.
Evidence: ignored ir-black-handoff-20261102-182135 JSON/log.

### Calibrated stationary follow-up result - 2026-11-02 18:25

No acquisition/handoff fault was observed in the calibrated follow-up. Last
snapshot: READY, running=1, samples=308769 at raw_ms=427186, failed_channels=0,
handoff_errors=0, handoff_stage/error/time=0, max lateness 156 us, scan 182 us,
live sources=0, duty 0/0, counts 0/0, line inactive. Line data was fresh (8 ms),
state LINE_ON with error -4 at raw 3045/872/1082. Latched motor fault 0x40 remains
until a future explicit reset; no reset/arm or movement command was sent.

The first extended monitor aborted on a missing motor-status line. The second
aborted on a missing line-status line despite healthy acquisition diagnostics
(its generic message said faulty/stale, but no faulty/stale line was actually
reported). Host monitor now distinguishes missing line records and retries up
to three times, preserving incomplete replies and still stopping on explicit
faults/nonzero duty. A final 30-second read-only continuation completed. These
observations span over 150 seconds since FINISH, but are NOT an uninterrupted
150-second complete-record test; USB loss remains a separate issue (939 total
dropped records / 31998 bytes since boot at last snapshot). Persistent zero
handoff count is evidence the original runtime stop did not recur in this
interval, not proof every concurrency path is fixed.

Evidence: ignored ir-ready-stability-20261102-182306 (incomplete monitor),
ir-ready-stability-20261102-182452 (completed continuation) JSON/log.
Before any driving, verify solid black covers both line-sensor eyes at normal
operating height: the current reported black placement remains LINE_ON instead
of BOTH_BLACK. Keep calibration powered in RAM; no further flash performed.

### Solid-black placement verified - 2026-11-02 18:27

After the user repositioned solid black under the line sensors at operating
height, a 30-second read-only check completed with eight complete snapshots,
no incomplete replies/retries, and h/i only. GP26 3051..3076, GP27 2396..2418,
GP28 2368..2392. All line snapshots reported BOTH_BLACK (state 5), error 0,
fault 0, maximum age 11 ms. IR READY/running, no acquisition or handoff errors,
failed_channels=0, live sources=0. Same 76778-sample calibration retained;
no BEGIN/FINISH, reset, flash, arm or motor command issued.

Motor duty stayed 0/0 and line inactive. Left raw count was 0xffffffff (-1)
at the last snapshot, compatible with a small wheel displacement during manual
positioning; no driven-motion inference is justified. USB cumulative dropped
records still increased (1544 at last snapshot), although all requested
snapshots in this check were complete. Calibration/black detection now agrees
with the stated placement. No integrated line-driving result exists yet.
Next is a stationary check at the normal track starting position before a
separately authorised bounded motor trial. Existing OUTSIDE-mode unseen-line
2-second timeout remains relevant if both eyes remain entirely over white.
Evidence: ignored ir-ready-stability-20261102-182705 JSON/log.

### Normal track starting position checked - 2026-11-02 18:28

User reported ready on track. Read-only h/i monitoring completed for 30 seconds,
eight full snapshots without incomplete replies. All were LINE_ON (state 1),
no line fault, maximum age 9 ms. Final error -19: right line sensor currently
sees more dark surface than left. Raw ranges GP26 154..181, GP27 169..204,
GP28 582..631. IR READY/running, failed_channels=0, handoff_errors=0, acquisition
faults=0, live sources=0. Calibration retained at 76778 samples. Motors duty
0/0 and line inactive throughout. Last raw counts L=0/R=106 include manual
positioning; no commanded motion. Latched motion fault 0x40 still needs the
normal healthy RESET before a trial. USB total drops 1989, despite complete
requested snapshots; do not claim loss-free console.

Starting position satisfies the observed line-state prerequisite. No RESET,
arm, TRACE, or movement command sent. Next proposed motor trial is the existing
local one-shot LINE_TRACE, 2000 ms maximum, 20-percent duty cap and automatic
stop, after explicit motor-test approval and a fresh preflight. Physical
line-following performance remains unverified.
Evidence: ignored ir-ready-stability-20261102-182849 JSON/log.

## First integrated line-trace motor trial - 2026-11-02 18:31

User explicitly authorised `run`. Fresh stopped preflight passed with IR READY,
no live faults, usable LINE_ON and error -19. One RESET completed (epoch 10->11,
id 1, IDLE/COMPLETED, faults cleared). Sent exactly one g: local 2000 ms trace,
id 2, queued and observed TRACING/ACCEPTED. No retry, renewal or second run.

Six active snapshots showed encoder motion, fresh IR data and bounded electrical
outputs (observed left -139..-175 permille, right +177..+190; not peak bounds).
One control snapshot showed line error +44 and target speeds 672/1200 counts/s,
with left duty -139 versus right +180: the integrated line controller did change
wheel targets to steer. Other snapshots showed equal 1200/1200 targets.
Diagnostic fields are separate snapshots, so control error and newest IR error
need not be identical in the same console response.

Terminal status BEFORE host STOP was IDLE/COMPLETED, faults=0, duty=0/0,
line inactive: firmware completed its bounded run. Forward encoder deltas from
preflight were left 2347/right 2320, uncalibrated counts only. IR READY and fresh,
no handoff/acquisition fault; final line error +46. Host then sent x to disarm;
STOP acknowledged, three post-stop checks showed zero duty, final two encoder
counts stable. No physical path/straightness claim yet: awaiting user observation.
USB drops remain a separate existing issue; do not infer continuous telemetry
from sampled console records. Buddy4 metric validity remains false.
Evidence: ignored tools/dashboard/.local/line-run-20261102-183136 JSON/log.

## User-confirmed line following and requested GP20 circuit profile - 2026-11-02

User observed that the first 2-second integrated run followed the line. This is
user-observed path success; software telemetry alone did not establish it.
User then requested a 30-second circuit test, selected battery/untethered use,
a physical button instead of countdown, and confirmed the onboard GP20 button.
Prepared separate LINE_CIRCUIT=1 profile with a single USB-armed GP20 press-release
start and press-to-stop; 30-second cap, same 20% duty, scaled count guard, all
sensor/encoder guards retained, no automatic startup or replay. GP20 ownership
recorded in TEAM.md. No B3/B4 source changed for this extension.

Implementation/build/host checks completed; no new hardware run. Details,
workflow, limitations and candidate hash are in LINE_CIRCUIT_30S.md.
Awaiting manual BOOTSEL before the already-authorised flash. Current 2-second
firmware remains on the robot until that step completes.

## Circuit firmware flashed; stationary startup verified - 2026-11-02 18:41 SGT

User confirmed manual BOOTSEL. Verified E:\INFO_UF2.TXT Board-ID RPI-RP2 and
candidate SHA256 C25630737338A3B430DD7D186FE01661F87B7A2ADEC8EB6EEED21E312D3295F8,
then copied the 30-second GP20 image. COM6 / MTK3PICO returned and its CIRCUIT
PROFILE help banner confirmed 30-second maximum, 20-percent duty and GP20 mode.

Stationary monitor used h/x/i only. Ten snapshots showed motor duty 0/0, counts
0/0, line inactive, GP20 released/unarmed/unconsumed. Final STOP acknowledged.
IR UNCALIBRATED (expected after flash), running=1, acquisition faults=0,
handoff_errors=0, handoff stage/error/time all zero. Samples progressed from
13389 to 40448; max lateness 152 us, scan 192 us, motion loop 20 ms. Live source
0x2 and latched motor fault 0x40 reflect missing calibration, not a new runtime
handoff failure. USB one dropped record / 8 bytes, no truncation.

No r, g, calibration command or motor run sent. Physical GP20 press/release still
needs verification while unarmed. Next: white/black IR setup, unarmed GP20 check,
track preflight, then separately authorised arming before the physical run.
Keep battery power uninterrupted when eventually removing USB; calibration and
permission remain RAM-only. Evidence: ignored
 tools/dashboard/.local/line30-stationary-20261102-184132.{json,log}.

### Circuit-profile white calibration started - 2026-11-02 18:45 SGT

User confirmed all three IR sensors over white at normal height. Verified circuit
banner; h/x/i/c only, BEGIN once accepted. White raw ranges: GP26 162..183,
GP27 165..196, GP28 160..192 across five snapshots. Three post-BEGIN snapshots
confirmed CALIBRATING, acquisition running with no faults, progressing samples
(3411 at last snapshot), no handoff errors, GP20 released/unarmed/unconsumed,
line inactive and motor duty 0/0. Right encoder raw count 1 at last snapshot;
no movement command sent. Calibration remains in RAM and is accumulating.
Next: solid black covering all three eyes at unchanged operating height, then
FINISH and verification. Do not power-cycle or arm GP20 yet.
Evidence: ignored tools/dashboard/.local/ir-white-circuit-20261102-184555 JSON/log.

### Circuit-profile black setup completed - 2026-11-02 18:47 SGT

User confirmed solid black beneath all three sensors at operating height.
Five black snapshots measured GP26 2985..3011, GP27 2575..2601, GP28 2345..2373.
Conservative contrast against white maxima was 2802/2379/2153 counts; all exceed
600. Sent FINISH once, accepted. Calibration committed from 81838 samples.
Post-FINISH stationary monitoring completed for approximately 30 seconds:
IR READY/running, failed_channels=0, acquisition faults=0, handoff_errors=0,
live source faults=0. Both line sensors reported BOTH_BLACK, fault=0, fresh data.
Last max acquisition lateness 123 us, scan 205 us. GP20 remained released,
unarmed and unconsumed; duty 0/0 and line inactive throughout. No RESET, arm,
USB start or motor command sent. Latched motor fault 0x40 remains for the normal
healthy RESET at eventual arming. USB log-drop counters remain nonzero; no claim
of lossless continuous telemetry is made.

Next: verify GP20 press and release while unarmed, then circuit starting-position
preflight. Keep USB connected and power uninterrupted to preserve RAM calibration.
Evidence: ignored tools/dashboard/.local/ir-black-circuit-20261102-184717 JSON/log.

### GP20 held input verified while unarmed - 2026-11-02 18:49 SGT

User held GP20 as requested. Four complete read-only h/i snapshots reported
pressed=1, armed=0, consumed=0, duty=0/0 and line inactive. IR remained READY,
running with no acquisition/handoff faults and live sources=0. The last line
state was BOTH_BLACK with age 9 ms. No RESET, arm or motor command sent.
The pressed state is verified; release must still be checked before arming.
Next: release GP20, place car at the circuit starting position, battery on with
USB still connected, and obtain readiness to arm the single physical run.
Evidence: ignored tools/dashboard/.local/gp20-held-20261102-184906 JSON/log.

### Circuit arm preflight deferred: both sensors white - 2026-11-02 18:50 SGT

User reported ready to arm. GP20 release confirmed (pressed=0, armed=0,
consumed=0), IR READY/running with no acquisition or handoff error and no live
source fault. However, the stationary line state was LOST (4), error=0, age=8 ms,
raw GP26/27/28=165/182/210: both line eyes currently see white. Existing OUTSIDE
mode declares LOST after two seconds without either sensor seeing the line.
This can happen with the track between the eyes while stationary; the sensor
runtime itself remains healthy. Preflight refused to arm. Sent h/i/x only;
no r or movement command. STOP acknowledged, motors were duty 0/0 and inactive.

Next: slightly shift the car sideways so one line eye overlaps the black edge,
keeping normal height and USB connected, then repeat preflight. No bypass or
line-loss policy change made. Calibration remains valid in RAM.
Evidence: ignored tools/dashboard/.local/line30-arm-20261102-185023 JSON/log.

### One GP20 circuit run armed - 2026-11-02 18:52 SGT

After user repositioned and reported ready, track showed fresh EDGE_ONLY,
error +75..77, GP20 released. The first RESET attempt was disarmed with x
because three post-reset replies omitted the motor-status line; this was an
incomplete observation, not an IR fault. Evidence: line30-arm-20261102-185133.
Retried stationary preflight under the same user readiness, using up to six
spaced status requests. One further RESET completed, epoch=9/id=2, IDLE and
COMPLETED, faults=0. Three complete post-reset snapshots confirmed pressed=0,
armed=1, consumed=0, duty=0/0, line inactive, fresh usable line, IR READY/running,
no handoff/acquisition/live-source faults. Last line age 8 ms, error +76.

The serial port was closed without STOP, preserving the verified one-run GP20
permission. No g or movement command sent. Robot is NOW ARMED: user may maintain
battery power, unplug USB, and press/release GP20 to initiate one up-to-30-second
run. Another GP20 press stops; no repeat until explicit rearming. Actual circuit
run outcome still pending user observation/reconnected diagnostics.
Evidence: ignored tools/dashboard/.local/line30-arm-20261102-185207 JSON/log.

## Button start replaced with five-second power-on start - 2026-11-02 19:04 SGT

User explicitly requested automatic start after five seconds of power-on. Old
button permission was stopped/disarmed. New LINE_AUTOSTART profile retains the
30-second/20-percent bounded run and all line/encoder/source fault stops, with
no repeated or delayed retry. Loads measured static white/black references at
boot; these are captured endpoints, not the exact former RAM accumulator.
Details, tests and limitations: testing/integration/LINE_AUTOSTART_5S.md.

User confirmed BOOTSEL and wheels lifted. Auto5 image hash
FAEA157195BD045C9CA895C42BFD1D61BD3F851F9474F1A71D70BDDE808A69C1 flashed.
Initial monitor expected the banner too early, then sent STOP before 5 s.
Later output and an independent h/i check confirmed the correct auto firmware,
IR READY from saved references, no acquisition/handoff faults, motors stopped,
AUTO phase CANCELLED. Thus hardware startup/cancellation is observed, but actual
5-second automatic motor initiation and full circuit run remain unverified.
Next full power cycle will permit one new auto attempt; no USB/button arm needed.
Evidence: ignored auto5-boot-20261102-190252 and auto5-confirm-20261102-190411.

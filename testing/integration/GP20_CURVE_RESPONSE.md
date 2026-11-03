# Earlier, tighter curve response - 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Status: **bounded-search image 82ED61... flashed and stationary-verified; track trial pending.**
See [GP20_BOUNDED_SEARCH.md](GP20_BOUNDED_SEARCH.md). Previous 3E2F79... stopped
on LINE_LOST at 2.596 seconds; the centered stationary check confirmed false LOST.
Previous tight-turn image 4ACFBE... stopped on an encoder fault about 40 ms
after left inside-wheel pause began; exact rejection cause pending.
See [GP20_TIGHT_TURN.md](GP20_TIGHT_TURN.md). Previous E5EC97... recorded the
repeated curve failure: strong right-turn saturation precedes line loss; the
fading steering hold activates but does not reacquire. Fresh LINE_LOST at 13.386 s.
See the final sections.
The superseded 1A4342...
150/550 image produced a confirmed loaded left-wheel stall. Its original profile,
artifact and validation history are retained below; see the follow-up sections.
Historical first curve-response image:
`build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`, SHA256
`1A4342648AFB4BF987DAA869EDFB133392E563C02D3446DE34B259545CF9AEBB`.
Backup of the installed image:
`tools/dashboard/.local/gp20-ir-handoff-installed-fd23c3.uf2`.

## Problem and expected behavior

The powerbank track video `video_2026-11-03_04-24-00.mp4` showed the car beginning
a left turn, then running wide with its front sensor area outside the black arc.
The user confirmed an automatic stop but had already powered off; its exact
fault reason could not be recovered. See
[IR_HANDOFF_EPOCH_FIX.md](IR_HANDOFF_EPOCH_FIX.md) for video observations and
prior hardware evidence. Fixed, lifted-wheel speed tracking passed previously;
it did not establish adequate loaded curve response.

The new line profile approaches more slowly, commands a tighter wheel-speed
ratio earlier, and responds faster to a changed speed target. It keeps both
wheel targets positive to avoid the earlier zero-target/restart discontinuity.
When leaving a significant curve it holds a slower speed envelope for 400 ms,
helping with closely spaced curves. Current IR direction is always used: the
previous turn direction is not held through a new opposite error.

| Line setting | Previous | Candidate |
|---|---:|---:|
| Straight target, each wheel | 900 | 700 counts/s |
| Sharp-curve outside wheel | 750 | 550 counts/s |
| Sharp-curve inside wheel | 300 | 150 counts/s |
| Error magnitude reaching full turn | 100 | 60 |
| Speed feedback Kp/Ki, milli units | 30 / 240 | 60 / 480 |
| PWM increase per 20 ms | 5 | 5 permille |
| PWM decrease per 20 ms | 5 | 10 permille |
| Target slew | 4000 | 4000 counts/s per second |

After error magnitude >=30, inputs below that threshold for less than 400 ms
use a 500-count/s maximum-wheel envelope. Both desired targets are scaled by
the same factor, preserving gentle/opposite turn direction and ratio. Persistent
strong curves continue using the normal curve targets. Once the hold expires,
cruise recovers through the existing ramps. New run/reset clears curve memory;
unsigned elapsed time handles clock wrap.

Per-wheel optional speed gains were added to the shared controller; zero selects
the previous defaults. Only line_follow_begin configures the higher gains.
Straight-bench limits/gains remain the same. Starting feed-forward remains
155/185 permille to retain existing breakaway behavior under load; startup is
not a calibrated zero-overshoot launch. The higher line-only feedback and bounded
deceleration reduce subsequent response delay, without an unrestricted PWM jump.

No S3 line interpretation or Buddy 4 code changed. The OUTSIDE-mode two-second
both-white ambiguity is unchanged. The 20-percent maximum power, 200 ms stall
stop, input freshness, line-loss/junction stop, encoder guards, immediate STOP,
GP20 start and 30-second firmware maximum remain. The handoff and timestamp
fixes are included. Lower inner-wheel speed can expose breakaway/stall limits
under load; this profile requires hardware validation and does not guarantee
that every curve radius is achievable.

## Validation

All 22 host executables passed. Existing owner-loop, IR epoch-handoff, saturation,
STOP, stall, time/encoder, button and indicator regressions remain passing.
Tests for line targets/rate limits were updated to the explicit new profile.

New `testing/subsystem2/line_curve_response_test.c` checks earlier full steering,
positive symmetric targets, brief exit slowdown, opposite-direction priority,
expiry/reset/wrap, and eight alternating bends with short centered gaps in each
of three illustrative motor/load models. Actual line_follow_step handles these
sequences with 20/40/60 ms sample intervals; tests enforce PWM/target slew,
maximum output, substantial wheel differential and line-loss rejection.

A separate same-target slowdown comparison in these models (700 to 150 counts/s)
compares the old and new speed-loop gains/slew. At 600 ms, absolute speed error
was 70..107 counts/s with the prior settings versus 7..27 with the candidate.
These are synthetic response checks, not calibrated motor models, a physical
turn-radius estimate, or proof that the filmed curve will succeed.

Firmware builds without warnings/errors, text/data/BSS 391504/184/85684 bytes.
Logs are `tools/dashboard/.local/gp20-curve-response-validation.log` and
`tools/dashboard/.local/gp20-curve-response-build.log`.

Standalone curve regression from repo root:

```sh
gcc -std=c11 -Wall -Wextra -Werror -DMOTION_LINE_DEMO=1 -DMOTION_LINE_CIRCUIT=1 testing/subsystem2/line_curve_response_test.c app_program/subsystem2/motion_logic.c -lm -o line_curve_response_test
./line_curve_response_test
```

No hardware commands or flash were performed while implementing this change.
Prompt before flashing/testing, as requested. After approval verify stationary
startup, then validate the lower inside-wheel speed and direction changes with
wheels lifted before a short loaded trial on the filmed curve. Keep the current
powerbank/GP20 workflow; no automatic startup was added.

## Authorized flash and stationary verification

User replied ready to flash. Verified the sole RPI-RP2 drive E:, board identity
and exact 1A4342... SHA256 before copying `gp20_curve_response.uf2`. Evidence:
`tools/dashboard/.local/gp20-curve-response-20261103-043635.{log,json}`.
Commands were h and six i requests only; no motor-start, reset, calibration,
STOP or software-BOOTSEL command was sent during verification.

All six motion records showed IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero counts and zero motor duties. GP20 was released and armed throughout.
IR was READY/running, acquisition faults 0, with advancing samples. Five LED
records showed armed mode 1, one showed the previously documented transient
observer error mode 4; this indicator issue was not changed by the curve update.
No powered trial of the new lower inside-wheel speed or curve profile has been
performed. GP20 still starts a run up to 30 seconds on powerbank; hardware
validation requires a separate ready-to-test instruction.

## Loaded stall captured; revised rolling targets prepared

On the next user-authorized test preflight, saved status already contained a
prior GP20 run and a latched stall. The logger did not start another run and
sent x, confirming zero duties and disarmed GP20 in three final snapshots.
Evidence: `tools/dashboard/.local/gp20-right-response-20261103-044105.{log,json}`.
User confirmed this prior run was on the track and the left wheel stopped while
the right continued. Motion fault was STALL (0x2), line step STALL (6), last
control error +100, targets 150/550 counts/s, filtered speeds 3/502. The stop
record is a stall, not an IR handoff or timeout fault. Live IR remained healthy;
STOP caused one successful epoch resync and no handoff error.

The 150-count/s rolling target failed under this load; the precise breakaway
threshold has not been measured. The provisional replacement uses 250 inside /
750 outside at full turn, keeping straight speed 700, full response at error
60, faster line-only PI, bounded PWM/target ramps, and the 400 ms exit slowdown.
The outer target now rises slightly above straight cruise, but average target
speed falls from 700 straight to 500 in a sharp curve. The inside/outside ratio
is 1/3 versus 0.4 in the earlier FD23C3... profile. This retains tighter requested
steering than that earlier profile while avoiding the failed 150 target; 250 is
not a proven minimum and may still need loaded adjustment. Stall protection was
not relaxed or bypassed. No automatic pivot/reverse behavior was introduced.

Tests now check decreasing mean target speed and increasing wheel-speed ratio
difference, since the outside wheel no longer slows monotonically with error.
Repeated opposite-curve models use 250/750 targets; the feedback response test
compares a 700-to-250 transition. All 22 host executables pass; firmware builds
without warnings/errors. No new hardware run or flash occurred while revising.

Candidate SHA256:
`E621D51EC1167F0BCB97F91B2339DE41BEEFD870363493C66F644B494221F437`.
Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`.
Build text/data/BSS: 391500/184/85684 bytes.
Logs: `tools/dashboard/.local/gp20-loaded-curve-validation.log` and
`tools/dashboard/.local/gp20-loaded-curve-build.log`.
Installed 1A4342... artifact backup:
`tools/dashboard/.local/gp20-curve-response-installed-1a4342.uf2`.
Prompt before flashing or running any further hardware test.

## Revised 250/750 profile flashed

User replied ready to flash. Verified E: RPI-RP2 board identity and exact
E621D5... SHA256, then copied `gp20_loaded_curve.uf2`. Installed firmware is now
E621D5..., superseding the earlier not-flashed status. Evidence:
`tools/dashboard/.local/gp20-loaded-curve-20261103-044425.{log,json}`.
Only h and six i requests were sent after flashing; no motor start, RESET,
calibration, STOP or software-BOOTSEL command was sent during verification.
All six motion records showed IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero counts and zero motor duties. GP20 released and armed, LED mode 1 in all
six records. IR READY/running with no acquisition or handoff errors. Raw line
channels were near white (GP27 about 227..254, GP28 about 256..291), whereas GP26
barcode read black (about 3706..3729). Healthy acquisition does not mean the
steering sensors see the line; position using GP27/GP28 before a powered trial.
No powered test of this 250/750 revision has occurred yet.

## Revised loaded trial: near-success, departure at curve exit

User reported tested, then clarified that the car almost followed perfectly,
left the line and stopped, specifically near the exit onto the straight.
Captured existing serial status with four i-only requests; no motor-start,
STOP, reset, calibration or flash command was sent. Evidence:
`tools/dashboard/.local/gp20-loaded-track-result-20261103-044640.{log,json}`.

Saved motion result: ERROR/FAILED, IR fault 0x8, LINE_STEP_IR (3), epoch 4/id 2,
zero duties, stable raw counts 4294962113/7922, GP20 consumed and not armed.
Retained last-successful-control diagnostics: error 0, target 700/700, speeds
703/712 counts/s. These values predate the stop; they do not identify the exact
rejected line message or prove its classification at the fault instant. Live
line status initially reported LOST, then ON while stationary; acquisition
remained READY/running with fresh samples, no handoff/acquisition errors or
fault source bits. No stall was recorded in this trial. The startup fault in
buffered boot text is not the running fault.

User observation plus IR stop is consistent with losing the line near exit.
The existing OUTSIDE both-white interpretation returns zero steering and the
curve exit speed cap expires after 400 ms, so straight-ahead cruise can resume
before a departing car is declared LOST. This is a candidate mechanism, not a
proven trajectory diagnosis. Asked whether it straightened early/runs wide or
continues turning/cuts inside before selecting another steering change.
No further run or firmware change was made during this result capture.

## Longer, slower exit hold and frozen failing input

The user could not distinguish cutting inside from running wide at the exit.
Without that evidence, the next candidate retains sharp-turn targets 250/750
and current-sample steering direction. It extends the exit hold from 400 to
1000 ms since the last error magnitude >=30, and lowers the maximum desired
wheel target during that hold from 500 to 400 counts/s. Both targets are scaled
equally; current opposite steering remains effective. Targets and PWM retain
their existing ramps, so actual wheel speed does not change instantaneously.
This is a conservative exit-speed experiment, not a proven trajectory fix.

Motion diagnostics now freeze the failing line input on a line-controller
failure: line state, error, sample age, elapsed run time, and flags for captured
(1), input present (2), matching epoch (4), and asserted input fault (8).
The console i command prints this as a separate `[line] failure ...` record.
It is distinct from both the retained last-successful-control speeds/targets
and the live IR input. Missing input uses state 255 and age UINT32_MAX.
Subsequent STOP preserves the snapshot; accepted RESET or a new accepted line
run clears its valid flag. It is held in RAM and is lost on power-off/reboot.
External source faults handled before the line controller do not create this
snapshot; the ordinary fault/source records still apply to those failures.

All 22 host executables pass. Owner-loop regression checks failing LOST,
BOTH_BLACK and stale inputs, saved-input survival across STOP, and clearing on
accepted reset/new run. Curve regressions check the new hold duration/cap,
opposite-error priority, wrap/reset and repeated modeled bends. Firmware builds
without compiler warnings/errors; text/data/BSS 391716/184/85708 bytes.
Software/model checks do not establish success on the physical curve.

Candidate: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`.
SHA256: `CD96C9BA44595C5EC3549CDCCFA04741987846667E59C3CE7B080922EEBB1A59`.
Installed E621D5... backup:
`tools/dashboard/.local/gp20-loaded-curve-installed-e621d5.uf2`.
Logs: `tools/dashboard/.local/gp20-curve-exit-validation.log` and
`tools/dashboard/.local/gp20-curve-exit-build.log`.

No flash or hardware command was performed while implementing this candidate.
GP20 press/release, the 30-second maximum, and existing stop protections remain.
Prompt for flash readiness before installing, then assess the same curve.

## Exit-slowdown candidate flashed

User replied ready to flash. Verified the sole RPI-RP2 volume E:, its bootloader
board identity and exact CD96C9... image hash, then copied gp20_curve_exit.uf2.
Evidence: `tools/dashboard/.local/gp20-curve-exit-20261103-045341.{log,json}`.
Only h and six i requests were sent for stationary verification; no motor-start,
reset, STOP, calibration or software-BOOTSEL command was sent.

All six motion snapshots were IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero counts and zero duties. GP20 was released and armed throughout. IR was
READY/running with advancing samples and no acquisition faults. Five LED
records showed armed mode 1, one the known transient observer mode 4 despite
fault-free stationary motion. This does not establish loaded curve performance.
The next physical trial remains GP20 press/release on the track, up to 30 seconds.

## Exit-slowdown video review

User supplied `video_2026-11-03_04-56-41.mp4` after the CD96C9... flash.
The clip is about 15.38 seconds. Reviewed overview frames and closer frames
at 10.5, 11.5, 12.5, 13.5, 14.5 and 15.2 seconds. Derived images are in
`tools/dashboard/.local/video-20261103-045641/`.

The car travels up the left straight and turns clockwise around the top arc.
In the latter part of the bend, approximately 12..14 seconds, its front moves
toward the outside/right of the arc while its heading remains diagonal rather
than aligning with the downward exit straight. It appears stationary in the
last frames. This suggests running wide rather than cutting inside, but the
sensor contact patches are obscured and the camera moves. The video cannot
establish the exact sensor sequence, wheel targets, or stop reason. No claim
is made that the one-second exit cap engaged at a particular moment.

No Pico serial device was visible during review. Asked whether the stop was
automatic and whether power was retained before requesting a connection;
do not remove the sole power supply merely to retrieve RAM diagnostics.
No firmware edit, flash or hardware command was performed for this review.

## Preserved stop details from exit-slowdown track run

User confirmed power was retained and USB reconnected. Read four i requests
only; no STOP, RESET, start or flash. Evidence:
`tools/dashboard/.local/gp20-curve-exit-result-20261103-045740.{log,json}`.
The current saved run is epoch 4/id 2, ERROR/FAILED, IR fault 0x8, line step 3,
raw counts 4294959165/5070, duties 0/0; GP20 consumed and disarmed.
Frozen failure: state 4 (LINE_LOST), error 0, age 3 ms, elapsed 12559 ms,
flags 0x7 (captured, present, matching epoch, no asserted input-fault flag).
This confirms a fresh line-loss classification triggered the automatic stop,
not the 30-second limit, a stale message, stall, or epoch handoff error.
IR acquisition remains READY/running, no faults, handoff errors or resyncs,
and no asserted external fault sources. Startup 0x40 text precedes the saved
run in buffered console history and is not its running fault.

Last-successful-control values retained separately are error 0, targets
700/700 and filtered speeds 679/674 counts/s. These are preceding control
values, not actual motion after the stop. With OUTSIDE-mode both-white logic,
zero steering is returned before the unseen timeout declares LINE_LOST; by
the preceding control the temporary exit cap was no longer active. This
supports investigating bounded steering continuity during ambiguous both-white
input after a bend rather than extending the run duration or bypassing LOST.
It still does not reconstruct every input or wheel response during departure.
No firmware change or powered test was performed during this capture.

## Bounded steering continuity after a curve

User requested the fix. Buddy 2's local line controller now remembers the
direction of an actual significant error (magnitude >=30). For LINE_ON with
zero error, it applies a correction in that direction of at most 20 error
units, fading linearly to zero within 500 ms of that significant observation.
Integer rounding can reach zero slightly earlier. Only actual significant
input renews the timer. Every nonzero input takes immediate priority in desired
steering; even a weak opposite input clears the old direction, while a strong
opposite input establishes the new direction. Existing target/PWM ramps still
bound physical response. New runs/reset clear the memory, and unsigned elapsed
time handles timer wrap.

The existing one-second, 400-count/s exit envelope uses the actual IR error,
so synthetic steering cannot renew it. Near the beginning of the memory period,
the desired wheel targets are approximately 313/400 (or the reverse), gradually
returning to 400/400. Sharp-curve targets remain 250/750. The speed controller
continues to enforce rolling targets, output/slew limits and independent stall
checks. LOST, stale/epoch-invalid input, junctions and faults are checked before
steering memory can apply. The S3 two-second LOST classification is unchanged.
GP20 start/stop and the 30-second maximum remain.

The S3 message does not distinguish both-white from another balanced zero-error
LINE_ON input. This is deliberately bounded curve-history assistance, not a
reliable detection of where the line lies. It can briefly steer on a genuine
straight exit and requires physical validation. No pivot/reverse or unbounded
search was added. S3's wire format and Buddy 4 implementation are unchanged.

Console control diagnostics append `steer=` for the applied error, keeping
`err=` as the actual sensor error. These remain last-successful-control values;
the separate frozen failure record identifies the rejected input at the stop.

All 22 host executables pass. New tests exercise both signs, fading/no renewal,
weak and strong opposite readings, wrap/reset, the real S3 curve-to-both-white
transition and immediate LOST/junction/fault/stale/epoch rejection while memory
is armed. Existing repeated-curve models and motor/IR owner regressions pass.
Firmware builds without compiler warnings/errors, text/data/BSS
391952/184/85716 bytes. Tests establish software behavior, not track success.

Candidate: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`.
SHA256: `4FE887A02EF54A283E981940A6EB742558F89F9D9660CA56DB788AEC4DD364EE`.
Installed-image backup: `tools/dashboard/.local/gp20-curve-exit-installed-cd96c9.uf2`.
Logs: `tools/dashboard/.local/gp20-curve-memory-validation.log` and
`tools/dashboard/.local/gp20-curve-memory-build.log`.
No hardware command or flash was performed while implementing this change.
Prompt for hardware readiness before flashing or running motors.

## Steering-memory candidate flashed

User confirmed ready to flash with both wheels lifted. Verified the sole
RPI-RP2 bootloader volume, Board-ID RPI-RP2, and exact 4FE887... image hash,
then copied gp20_curve_memory.uf2. Evidence:
`tools/dashboard/.local/gp20-curve-memory-20261103-050050.{log,json}`.
Only h and six i requests were sent after flashing; no motor-start, RESET,
STOP, calibration or software-BOOTSEL command was sent.

All six motion snapshots were IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero counts and zero duties. GP20 released and armed throughout. IR READY and
running with advancing samples and no acquisition faults. Five LED records
showed armed mode 1; one showed the previously documented transient observer
mode 4 despite fault-free motion. Line channels read high during the lifted
check, so position the sensors appropriately on the track before pressing GP20.
No powered curve test of this revision has yet been observed.

## Steering-memory track result: same departure area

User supplied `video_2026-11-03_05-03-51.mp4` and reported the same departure
area, then confirmed retained power/USB connection. The 15.68-second clip was
reviewed in 12 evenly spaced frames, saved as
`tools/dashboard/.local/video-20261103-050351/overview.jpg`.
The car again turns clockwise from the left straight around the upper arc,
then runs outward near the right side before aligning with the exit straight.
The video does not expose the exact sensor contact points or commanded speeds.

Four i-only queries captured
`tools/dashboard/.local/gp20-curve-memory-result-20261103-050426.{log,json}`.
No start/STOP/reset/flash command was sent. Saved run: epoch 4/id 2,
ERROR/FAILED, fault 0x8, step 3, raw 4294958977/5200, duties 0/0, GP20 consumed.
Frozen failure: LINE_LOST (4), error 0, age 3 ms, elapsed 12839 ms, flags 0x7.
IR acquisition remains READY/running, no acquisition/handoff faults or resyncs,
and no asserted external fault sources. Buffered startup fault 0x40 is separate.
Last-successful-control values are raw error 0, applied steering 0, targets
700/700 and filtered speeds 750/689 counts/s; these predate the stop.

The bounded memory did not resolve this loaded failure. Its applied error is
expected to be zero by a two-second unseen stop, so the endpoint alone cannot
show whether the correction activated, which direction it used, or how the
wheels tracked the earlier curve targets. Do not infer those facts from the
final zero value or from live sensors after the car has been repositioned.

## Stopped-run history for the next diagnosis

Added a 64-entry rolling RAM history to Buddy 2, sampled at most once per
100 ms during successful local line control (approximately the last 6.4 s at
the nominal loop rate). A line-controller failure appends its input immediately
even if the sampling interval has not elapsed. Each row contains elapsed time,
line state/error, applied steering, sample age/validity flags, wheel targets,
filtered speeds and signed motor duties. The failing row's speeds/targets/
steering/duties are the retained pre-stop control values, identified by its
non-OK step; they are not a command to continue after the rejected input.
Other terminal events such as external STOP freeze the preceding samples
without inventing a failing line input. Existing status/fault fields identify
those events. Samples are not a complete 20-ms control log or raw ADC capture.

The motion owner records without allocation, locks or USB output. It publishes
one observer copy of changed history after motion stops, under the existing
nonblocking observer lock. Readout is rejected while the published owner state
is active. Console `t` reads the next eight oldest-to-newest rows, wrapping to
the first page after the final page. Epoch/request ID checks prevent mixing
runs across pages. Pagination bounds output below the USB queue size in normal
use; no printing was added to the driving loop. Existing console contention
can still require retrying a read.

STOP preserves the record; accepted reset/new line run clears it. Power loss,
RESET-button reboot or flashing erases it. Steering, curve targets, the 500-ms
memory, one-second exit cap, GP20 behavior and all stop guards remain unchanged.
This image is diagnostic instrumentation, not a claim that the curve is fixed.

All 22 host executables pass, including new chronological wrap/overwrite,
100-ms throttle, forced final input, active-read rejection, lock contention,
STOP preservation, reset clearing and paged-console read-only checks. Firmware
builds without compiler warnings/errors: text/data/BSS 393308/184/89876 bytes.
The stopped observer copy adds about 4 KiB total static storage including the
owner history. No hardware performance claim is made before flashing/testing.

Candidate: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`.
SHA256: `E5EC97ABE02C527663115611BEF66A4765B27C74D29C32D3665B2048A97A8618`.
Installed backup: `tools/dashboard/.local/gp20-curve-memory-installed-4fe887.uf2`.
Logs: `tools/dashboard/.local/gp20-curve-history-validation.log` and
`tools/dashboard/.local/gp20-curve-history-build.log`.
Not flashed. Prompt for readiness, then one powerbank curve trial; preserve
power afterward and read i plus successive t pages before any reset/new run.

## Diagnostic-history image flashed

User replied ready to flash. Verified the sole RPI-RP2 volume, Board-ID RPI-RP2
and exact E5EC97... hash, then copied gp20_curve_history.uf2. Evidence:
`tools/dashboard/.local/gp20-curve-history-20261103-050823.{log,json}`.
Post-flash h and six i requests confirmed IDLE/COMPLETED, faults 0, epoch 3/id 1,
raw valid, zero counts/duties; GP20 released/armed and LED mode 1 throughout.
IR READY/running with advancing samples and no acquisition/handoff faults.
No start, STOP, reset, calibration or software-BOOTSEL command was sent.
The history remains empty until a physical run; the subsequent t-only read
checks the new console command without creating motion. Next step is one
GP20 track trial, preserving power afterward for i/t history capture.

## Complete recorded curve run

User reconnected after the track trial. Only i and t queries were sent; no
start, STOP, RESET, calibration or flash. Evidence:
`tools/dashboard/.local/gp20-history-result-20261103-051112.{log,json,csv}`.
Observer contention caused several t reads to return -50 and restart paging;
retries recovered all 64 unique rows with identical duplicates, all from run
epoch 3/id 2. This is a diagnostic-read contention, not a recorded motor or IR
handoff fault. Motion's saved fault increments its current epoch to 4.

Saved stop: ERROR/FAILED, IR fault 0x8, step 3, frozen LINE_LOST/error 0,
age 0 ms, elapsed 13386 ms, flags 0x7. Stable raw counts 4294958855/5306 and
actual stopped duties 0/0; GP20 consumed/disarmed. IR remains READY/running,
no acquisition/handoff faults or resyncs. Startup 0x40 text is buffered history.

Recovered sequence (times relative to firmware run admission, not video time):

- 7.106..11.206 s: predominantly strong negative error and sustained 750/250
  left/right targets. Many measurements approach these targets, with transient
  inside-wheel deviations. Error reaches -100 repeatedly at 10.206..11.006 s;
  the controller is already at its configured maximum right-turn ratio.
- 11.306 s: last sampled nonzero error, -44. Desired steering still follows it.
- 11.406..11.806 s: sensor error 0 while applied steering fades -17,-13,-9,-5,-1.
  The memory is demonstrably active. Targets settle into the 400-count/s exit
  envelope. Transient speed differences remain during this transition.
- 11.906..12.306 s: zero applied steering, 400/400 targets.
- 12.406 s onward: exit hold expires and targets return to 700/700 while the
  line is still unseen. At 13.386 s, fresh LINE_LOST stops both outputs.

The terminal trace row's 713/766 speeds and -120/111 duties are retained
pre-stop values, not movement after stopping; ordinary status confirms 0/0.
The record confirms that both the main right-turn command and added memory
execute. Combined with the previous video of running wide, it supports
investigating insufficient achievable turn curvature with the current 3:1
rolling target ratio. It does not measure physical radius or rule out sensor
placement, traction or transient load effects.

Recommended next change: a deliberately bounded tighter-turn mode for sustained
large error, with a controlled outside wheel and deliberate inside-wheel pause,
plus entry/exit hysteresis to avoid repeatedly toggling stop/restart. The prior
150-count/s loaded stall argues against blindly lowering the rolling target.
Keep all LOST/stale/encoder/STOP protections; do not extend the unseen timeout
or infer reacquisition solely from error zero. No such steering change has been
implemented or flashed during this readout. Evidence is saved on the laptop;
the board no longer needs retained power for this run's diagnostics.

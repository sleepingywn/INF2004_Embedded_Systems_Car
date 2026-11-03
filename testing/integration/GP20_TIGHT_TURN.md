# Bounded tight-turn trial - 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Status: **bounded-search image 82ED61... flashed and stationary-verified; track trial pending.**
See [GP20_BOUNDED_SEARCH.md](GP20_BOUNDED_SEARCH.md). Previous 3E2F79... stopped
on LINE_LOST at 2.596 seconds; a centered stationary check confirmed false LOST.
The previous encoder fault did not recur and remains unresolved.
Previous 4ACFBE... stopped on an encoder fault shortly after inside-wheel pause
began. Its exact encoder rejection cause is not recorded.

The recorded E5EC97... track run reached sustained maximum right-turn targets
750/250 counts/s. During 27 plateau samples at 8.606..11.206 s, measured wheel
speeds averaged 748.4/254.0, while sensor error repeatedly reached -100. It then
lost the line despite the fading steering hold. See
[GP20_CURVE_RESPONSE.md](GP20_CURVE_RESPONSE.md) for the full evidence and its
limits. This suggests the current rolling turn is insufficiently tight; physical
turn radius, sensor geometry and traction have not been calibrated.

User authorized implementing a tighter-turn mode. Buddy 2 now uses the actual
sensor error to enter a deliberate inside-wheel pause with the outside wheel
targeting 500 counts/s. The state machine is in `line_tight_turn.h`; it is called
only after the existing input, epoch, encoder, duration and timing guards.

| Setting | Value |
|---|---:|
| Entry error magnitude | >=85, same direction |
| Continuous entry confirmation | 120 ms |
| Inside-wheel target/output during tight turn | 0 / 0 |
| Outside-wheel target | 500 counts/s |
| Exit error magnitude | 1..45, same direction |
| Continuous exit confirmation | 100 ms |
| Opposite nonzero error | Exit immediately |
| Cooldown before a new entry confirmation | 300 ms |
| Maximum continuous tight-turn duration | 800 ms, then stop |
| Continuous zero-error allowance while tight-turning | 200 ms, then stop |

Zero error is ambiguous and does not count as successful exit confirmation.
An opposite error takes priority immediately, subject to the existing target
ramps on resumed rolling motion. Timer arithmetic handles wrap. New run/reset
clears all mode memory. Entry confirmation, lower exit threshold, exit dwell
and cooldown prevent repeated pause/restart on threshold noise. Synthetic
steering-memory error cannot engage or prolong this mode.

The inside-wheel pause commands zero duty immediately using the existing motor
stop output. This is a deliberate exception to normal PWM/target deceleration
ramps. Actual wheel motion and braking response require hardware observation.
The outside-wheel target/PWM retains its existing ramps and 20-percent output
limit. Its stall guard remains active. The intentionally paused wheel is exempt
only while its target is zero; normal 200-ms stall checking resumes on restart.
Existing speed-controller restart uses bounded feed-forward power once on exit
(left 155/right 185 permille before that step's feedback). This transition is
not claimed to follow the ordinary PWM slew or be physically jerk-free. The
shared speed controller and its straight-bench behavior were not modified.

Failure after 800 ms returns LINE_STEP_TIGHT_TIMEOUT (9), mapped to motion
TIMEOUT fault. Zero error for 200 ms during a tight turn returns
LINE_STEP_TIGHT_UNSEEN (10), mapped to IR fault. Those can precede the ordinary
two-second S3 LOST timer and are intentional bounded trial stops. All existing
LOST/junction/stale/epoch/encoder/STOP protections remain. No reverse output,
automatic restart or longer trial duration was added. GP20 still starts one
run up to 30 seconds and another press stops it.

The console and saved history append `tight=1` for left inside-wheel pause,
`tight=-1` for right pause, or `tight=0` for normal rolling control. `steer=`
remains the proportional/memory error; tight mode overrides its wheel targets.
Stopped diagnostics retain the last mode as historical information. Failed
history rows still show pre-stop speed/target/duty values; ordinary status
shows the actual stopped outputs.

## Software verification

All 23 host executables passed. New tests cover entry confirmation/noisy
thresholds, lower exit threshold/dwell, cooldown, both signs, opposite evidence,
zero-input and hard deadlines, wrap/reset, deliberate pause and independent
outside-wheel stall. Three illustrative load models exercise two successive
opposite tight turns and bounded restart/output behavior. They are not calibrated
physical models and do not establish track success.

Actual motion-owner scenarios verify left/right entry and exit, hard timeout,
zero-input timeout, manual STOP, LOST, external fault and outside-wheel stall,
including zero final motor outputs and history capture. Existing rolling-speed
tests now use full rolling errors below the new entry threshold; sustained
errors >=85 are covered by the new bounded-mode cases rather than incorrectly
expecting uninterrupted 30-second rolling motion.

Standalone mode regression from repo root:

```sh
gcc -std=c11 -Wall -Wextra -Werror -DMOTION_LINE_DEMO=1 -DMOTION_LINE_CIRCUIT=1 testing/subsystem2/line_tight_turn_test.c app_program/subsystem2/motion_logic.c -lm -o line_tight_turn_test
./line_tight_turn_test
```

Firmware built without compiler warnings/errors. Text/data/BSS:
393932/184/89916 bytes. Whitespace check passed (existing Git line-ending notice
for saved calibration remains unrelated).

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`.
SHA256: `4ACFBEEB114FFB6A487995D4A5C03BA5C90D76B4A59E06CFA68E99A16CD7AD62`.
Installed backup: `tools/dashboard/.local/gp20-curve-history-installed-e5ec97.uf2`.
Logs: `tools/dashboard/.local/gp20-tight-turn-validation.log` and
`tools/dashboard/.local/gp20-tight-turn-build.log`.

No hardware commands or flash were performed during implementation. Prompt
before flashing/testing. After readiness and flash, verify stopped GP20 startup,
then evaluate a short controlled trial of the tighter turn. Preserve power after
the run and collect i/t records before resetting so timeout, sensor loss and
wheel tracking can be distinguished.

## Authorized flash and stationary verification

User confirmed ready to flash with both wheels lifted. Verified the sole
RPI-RP2 volume, Board-ID RPI-RP2 and exact 4ACFBE... image hash before copying
gp20_tight_turn.uf2. Evidence:
`tools/dashboard/.local/gp20-tight-turn-20261103-051710.{log,json}`.
Only h and six i requests were sent after flashing. No motor-start, STOP,
RESET, calibration or software-BOOTSEL command was sent.

All six motion snapshots were IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero counts and motor duties; GP20 released and armed. IR READY/running with
advancing samples and no acquisition or handoff errors. Five LED records were
armed mode 1, one the known transient observer mode 4 despite fault-free motion.
This confirms stationary startup only. Powered curve behavior remains untested.

## First loaded tight-turn result: encoder stop during entry

User supplied `video_2026-11-03_05-20-55.mp4`, then confirmed retained power/USB.
The approximately 10.01-second clip shows the robot starting on the right-hand
straight and turning counterclockwise toward the upper arc. This is a left turn,
opposite the preceding recorded right-turn trials. The robot stops partway into
the bend; the video does not establish completed curve/exit tracking.
Twelve overview frames are saved in
`tools/dashboard/.local/video-20261103-052055/overview.jpg`.

Read i/t only, with no motor-start, STOP, RESET, calibration or flash command.
Recovered all 64 unique history rows with consistent duplicates, run epoch 3/id 2:
`tools/dashboard/.local/gp20-tight-result-20261103-052132.{log,json,csv}`.

The saved result is ERROR/FAILED, ENCODER fault 0x1, LINE_STEP_ENCODER (4),
elapsed 6356 ms. Frozen input is LINE_ON, error +74, age 0 ms, flags 0x7.
Current motion has epoch 4/id 2, stable raw counts 4294964271/4048, raw-valid=1,
and actual duties 0/0. IR remains READY/running with no acquisition/handoff
errors or resyncs. This is not the previous LOST fault, an outside-wheel stall,
the 800-ms tight-turn deadline, or the 200-ms zero-input deadline.

At 6.216 s, error +86 still commands rolling 250/750 targets. At 6.316 s,
tight=+1, targets 0/670, duty 0/132: the left wheel is deliberately paused and
the right target is ramping toward 500. At 6.356 s the encoder guard stops the
run, about 40 ms after the first recorded pause. The retained last-successful
targets are 0/590 and filtered speeds 209/746. Those values predate stopping;
actual stopped duties are 0/0. The short activation does not validate sustained
tight-turn behavior, exit behavior or the restart transition.

Source inspection identifies several paths to LINE_STEP_ENCODER: changed
invalid-edge count, unexpected signed/receding travel, or rejected count/speed
deltas (and raw-unavailable admission to this control call). The available
history does not include per-step raw counts, invalid-edge deltas or a reason
subcode. Therefore it cannot distinguish a small backward tick while stopping
from an invalid pulse sequence or another count rejection. Do not label this a
confirmed wiring fault, assume wheel recoil, or bypass encoder checks based
on the coincident pause alone. A precise encoder-rejection snapshot is the next
diagnostic requirement before deciding whether to change the braking transition.

No firmware changes or flash were performed during this review/readout.
Evidence is saved on the laptop; retained board power is no longer needed for
this run's logs.

## Frozen encoder rejection report

User authorized the next change. The candidate records the specific encoder
rejection before the existing fault stop clears controller state. This is a
diagnostic change; the braking behavior, thresholds and encoder protections
remain the same. It does not claim to resolve the physical trigger yet.

Console i prints three additional `[line] encoder ...` lines after an encoder
failure: reason flags, speed-layer flags, raw-valid status, initial/current
invalid-edge counters, sample interval, current/previous/origin raw counts,
speed-controller previous totals and targets. Counts are unsigned modulo values;
forward raw direction is negative on the left and positive on the right. Previous
raw counts are reconstructed from the last accepted totals using unsigned modulo
arithmetic, preserving wrap. The report survives STOP and clears on an accepted
RESET/new line run. RAM diagnostics are erased by power-off/reboot/flash.

| Encoder reason bit | Meaning |
|---|---|
| 0x01 | Raw encoder snapshot unavailable |
| 0x02 | Aggregate invalid-edge counter changed |
| 0x04 / 0x08 | Left/right signed travel is negative relative to run origin |
| 0x10 / 0x20 | Left/right travel receded from the preceding accepted sample |
| 0x40 | Speed-controller input rejection; inspect speed_reason |

Several wheel bits may be set together. Earlier checks retain precedence:
invalid-edge rejection occurs before signed-travel checks. The aggregate ISR
invalid count covers ambiguous/invalid quadrature events; it does not by itself
identify which electrical channel or ISR ambiguity caused an event.

Speed-layer flags identify interval (0x01), target range (0x02), left/right
negative delta (0x04/0x08), or excessive left/right measured count rate
(0x10/0x20). The shared speed function now optionally reports its existing
rejection branch; the original bool API wraps it. All validations still run
before either wheel controller is updated. Snapshot target values are the
rejected targets for speed-layer failures and preceding targets otherwise.
Non-encoder failures cannot inherit a prior per-step rejection code.

All 24 host executables pass. New regressions cover exact reason flags, both
one-count reversals, invalid counts, impossible rates, normal count/time wrap,
non-encoder code clearing and unchanged reported/unreported speed-controller
outputs. Actual motion-owner scenarios inject each failure after tight-turn
entry, verify final zero outputs, exact frozen counts, observer publication,
STOP preservation and reset clearing. Existing turn/IR/button tests pass.

Build has no compiler warnings/errors, text/data/BSS 394400/184/90036 bytes.
Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`.
SHA256: `3E2F7927163AE4D31E05B5A47C65BF4A2547A539185AF0203B4799C6480CC38F`.
Installed backup: `tools/dashboard/.local/gp20-tight-turn-installed-4acfbe.uf2`.
Logs: `tools/dashboard/.local/gp20-encoder-reason-validation.log` and
`tools/dashboard/.local/gp20-encoder-reason-build.log`.
No hardware command or flash was performed while implementing the report.
Prompt before flashing/testing; preserve power after the next trial and read
i/t before resetting to determine the actual cause rather than weakening guards.

## Encoder-reason image flashed

User confirmed ready to flash with both wheels lifted. Verified the sole
RPI-RP2 volume, Board-ID RPI-RP2 and exact 3E2F79... image hash, then copied
gp20_encoder_reason.uf2. Evidence:
`tools/dashboard/.local/gp20-encoder-reason-20261103-053451.{log,json}`.
Only h and six i requests were sent after flashing; no start, STOP, RESET,
calibration or software-BOOTSEL command was sent.
All six motion snapshots were IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero counts/duties and GP20 released/armed. IR READY/running with advancing
samples and no acquisition/handoff errors. Five LED samples were armed mode 1;
one was the known transient observer mode 4 despite fault-free motion.
This is stationary verification only; preserve power after the next curve
trial and retrieve i/t before resetting or powering off.

## Early stop on the diagnostic image: LINE_LOST, no encoder rejection

User reported straight movement for about a second followed by stopping and
a blinking LED, then confirmed retained power/USB. Only i/t queries were sent.
Evidence: `tools/dashboard/.local/gp20-encoder-result-20261103-053653.{log,json,csv}`.
All 27 history entries were recovered with consistent duplicates.

Saved run: ERROR/FAILED, IR fault 0x8, step 3, epoch 4/id 2, actual duties 0/0.
Frozen failure: LINE_LOST, error 0, age 0 ms, elapsed 2596 ms, flags 0x7.
No encoder-rejection snapshot was emitted. IR acquisition remains READY/running,
without acquisition/handoff errors or resyncs. The previous encoder failure was
not reproduced; this does not establish that it has been fixed.

History starts at +95/+97 error (16/116 ms). At 216 ms, tight=+1 with left target
0 and right target 500; the filtered right speed is 1122 during startup. By
316 ms, actual error is -100, the mode exits on opposite evidence, and the left
wheel restarts. At 516 ms the last sampled nonzero error is -76. From 616 ms
onward all sampled errors are zero: steering memory fades, the exit cap holds
400/400, then 700/700 returns at 1616 ms. Fresh LINE_LOST stops at 2596 ms,
consistent with the S3 two-second continuous both-white rule. The early tight
entry/opposite reversal is observed; starting placement and actual trajectory
were not captured in a video for this run.

With OUTSIDE sensor spacing, both-white may mean either a centered straight
line or departure from the track. The record proves that the firmware declared
LOST, not that the car physically left the line. Asked the user whether the
line remained between GP27/GP28 at the stop before selecting a rule change.
Live readings after USB connection do not establish the original track pose.
No firmware edits, flash, RESET, STOP or motor commands were performed during
this readout. All evidence is saved on the laptop.

## Centered stationary check confirms classification ambiguity

User could not establish the stopped pose from the prior run, but suspected
both sensors were white for two seconds. Asked for a controlled stationary
placement at normal height with the black line centered between GP27 and GP28,
GP20 unpressed, held for at least three seconds. User replied positioned.
Five i-only samples were captured:
`tools/dashboard/.local/gp20-centered-ir-20261103-053938.{log,json}`.

All five live line messages were LINE_LOST (4), error 0, fault flag 0, age
0..3 ms. IR acquisition remained READY/running with advancing sample counts,
no acquisition/handoff errors. GP27 raw values were 167..200 and GP28 196..232,
consistent with white under the saved calibration. GP26 was also near white
(154..176); its current placement did not provide a black-line reference for
this centered pose. Motion retained the previous latched fault, GP20 consumed,
stable counts and zero output; no reset/start/STOP/flash was issued.

This establishes that the two-second OUTSIDE both-white timer can report LOST
while the user has deliberately centered the line. It does not prove the robot
was centered when the previous moving trial stopped. It also does not settle
the separate prior encoder fault. With the present sensor observation, an
unseen line between the sensors and an absent line are indistinguishable.
A reliable geometry change would let a sensor see the line at center (closer
edge placement or a center sensor); a software-only alternative requires
bounded active reacquisition with potential steering oscillation. Simply
extending/removing the timer is not equivalent to detecting the line.
No firmware change was made during this stationary diagnosis.

# Integrated encoder speed control and IR line tracing

Prepared 2026-11-02 at the user's request to stop wheel-calibration trials and
move on to line tracing. Implementation and software validation are complete;
the image has now been flashed and passed the stationary check below. No
integrated motor/line-following run has occurred. No new wheel calibration is required to try this
count-domain profile, but working IR black/white contrast remains essential.

## Resulting behavior

An explicit local request lets Buddy 2's existing motion task follow Buddy 3's
line-position messages for at most two seconds. Each wheel has continuous
encoder speed PI. A line to the left slows the left wheel; a line to the right
slows the right. The outer wheel's target stays at or below 1200 normalised
counts/s, and duty never exceeds 20%. A full-scale error commands the inner
wheel to stop. Both-black/junction, lost/held/invalid line, stale input, wrong
epoch, encoder faults, stalled wheel, excessive travel, or USB disconnect stops
the run. There is no automatic start, reacquisition search, or barcode turn.

The line controller supplies different wheel-speed targets directly. It does
not apply the straight bench's equal-travel correction during a turn, since
that would oppose line steering. Lower duty is permitted down to zero for
the inner wheel; retaining the bench's 14% floor would restrict steering.
Returning from a zero target uses the wheel's bounded feed-forward value,
then resumes normal slew/PI control. Physical turning behavior is unverified.

Shared implementation: `app_program/subsystem2/forward_speed.h`, also used by
the standalone feedback bench. Existing v7 tuning is retained: feed-forward
15.5% left / 18.5% right, Kp 0.03, Ki 0.24, integral limit +/-4 percentage
points, 40 ms speed filter and 0.5 percentage-point slew per 20 ms. The
provisional 2550 counts/revolution values remain estimates. Previous v7 tests
still showed about 60 counts of left lead; no successful straight calibration
or physical line-tracking accuracy is claimed by this integration.

## Boundaries and interfaces

- `MOTION_LINE_TRACE` is a new local-only, forward-only, bounded request.
  Fields for velocity/distance/angle must be zero. Duration is 40..2000 ms.
  It cannot be renewed and cannot queue behind a running operation.
- It requires coherent recent raw encoders and fresh healthy line messages.
  It does not need calibrated metric odometry. `MOTION_CALIBRATED` stays zero;
  `motion_status_t.valid/calibrated` remain false, raw counts remain available.
  Distance/angle commands and normal calibrated velocity/follow commands retain
  their existing gates. Buddy 4 cannot mistake these estimates for valid metric
  wheel data. No Buddy 3 or Buddy 4 source was changed for this integration.
- Existing IR acquisition, calibration, epoch and health handoff are reused.
  The IR fault cannot be bypassed from the new console. Local reset does not
  start movement, and succeeds only when existing source faults permit it.
- Live speed targets/results appear under `i` diagnostics as normalised counts/s.
  MQTT retains the existing schema and honest metric validity flags. Wi-Fi,
  telemetry and terrain tasks remain in the combined firmware.
- No new task, IRQ, timer or kernel object was added. Motor PWM/encoder ownership
  remains with Buddy 2; ADC and line detection remain with Buddy 3.
- Stops are checked by the existing motion task. The standalone bench's global
  hardware watchdog is not enabled in the RTOS image: doing so would reset the
  entire Wi-Fi/IR/kernel system. Hardware scheduling and stop timing require
  validation; software tests do not establish a hard real-time stop bound.

## Profiles and prepared image

Default `LINE_DEMO=0` leaves the USB console diagnostic-only and rejects the
new line request. `LINE_DEMO=1` explicitly enables the limited local commands,
requires USB CDC, adds `_line` to the image filename, and participates in the
build's stale-object guard. The broad development console remains disabled.

Build from `build_make` with the existing toolchain/SDK configured:

```text
make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 LINE_DEMO=1 PICO_SDK_PATH=<sdk-path>
```

On this Windows setup the converter is selected with `E2U=tools/elf2uf2.exe`.
Prepared file: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line.uf2`.
SHA-256: `9E1051B253EA18E5FD2838A940E568EBDB62E8624E801CB58B03D7E33F8B3F28`.
Build sizes: text 387624, data 176, BSS 85564 bytes. Default and line profiles
both built successfully; neither build log reported a warning or error.
Earlier main UF2 backed up in ignored
`tools/dashboard/.local/firmware-backups/before-line-integration.uf2`.

## Operator flow after explicit flash approval

1. Flash the `_line` image for a stationary sensor check first. Boot does not
   drive. Open USB with DTR asserted; `h` identifies the local line profile.
2. `i` reports raw IR, calibration state, health-related faults, line position,
   and motion status. First verify GP27/GP28 respond to the track/background.
   Prior documents report an unresolved GP28 contrast/wiring issue; do not
   assume either that it persists or that this motor change fixes it.
3. The existing IR calibration is RAM-only and needs black/white exposure after
   reboot: `c` begins, expose all three sensors to both surfaces while stationary,
   `f` finishes, `i` verifies READY/healthy and no failed channel. This is sensor
   setup, not another wheel-count/straight-drive calibration. No automatic
   motion is performed by calibration commands.
4. With a clear track, correct sensor geometry and cable slack, `r` requests
   reset/arm. Inspect `i` for accepted reset, cleared faults and a fresh line
   snapshot in the new epoch. Neither `r` nor `g` clears live source faults.
5. After approval/readiness for physical line tracing, `g` requests one run.
   It consumes the arm even if refused. Held/queued `g` cannot replay it;
   every further run requires a new `r`. `x` or space stops and disarms.
6. Read `i` after stop; record physical path, line error, speeds/targets, faults
   and counts. No automatic duration extension or retry is implemented.

Sensor geometry is the existing OUTSIDE mode: GP27 is the left-hand line
sensor, GP28 the right-hand line sensor; GP26 is barcode only. Both-white can
mean the line is between the eyes, so it does not prove centering. Buddy 3
declares line lost after two seconds without either eye seeing it. That behavior
is unchanged; long straight segments may stop until placement is validated.
IR line tracing can correct deviation from a visible track, not guarantee a
straight heading on an unmarked floor. Tight bends/junction missions are outside
this initial profile. Barcode capture may continue but cannot command turns.

## Software evidence

All tests below are host-only and do not operate hardware:

| Check | Result |
|---|---|
| Shared speed-controller regression | Actual standalone main-loop checks and five 30-second illustrative motor models passed |
| IR-to-wheel direction | Uses Buddy 3's actual normalisation/line logic; positive/negative errors produce correct slower wheel |
| Line input guards | Uncalibrated, lost, held, junction, fault, stale/future time, wrong epoch, bad range rejected |
| Line dynamics/limits | Inner-wheel stop/restart, no false zero-target stall, no competing equal-travel correction, count/time wrap and 2 s deadline passed |
| Actual RTOS motion owner with mock kernel/hardware | Ten scenarios passed: normal expiry, external fault, lost/stale line, disconnect, stall, encoder fault, stop, left/right steering |
| Admission and validity | Local-only, no renewal/queue, no metric calibration bypass for distance/follow; default build rejects line mode |
| Local command surface | Healthy/fresh IR required, reset/arm then one run, stop disarms, no IR bypass; default has no motor keys |
| Regressions | Six existing motion-logic cases and diagnostic allowlist/clock checks passed |

Source runners: `testing/subsystem2/line_follow_test.c`,
`motion_line_integration_test.c`, `line_console_test.c`, and the existing
`feedback_bench/host_test.c` / `controller_test.c`. Build them with host GCC
`-std=c11 -Wall -Wextra -Werror` and the relevant motion/sensor logic sources.
The actual-motion host runner includes `motion_host_shim.h` and uses
`-Iinclude -Wno-type-limits -Wno-sign-compare` for two pre-existing warnings in
legacy motion code. Run it and the console runner with `-DMOTION_LINE_DEMO=0`
as well to verify the default profile. No hardware timing, sensor contrast,
tracking stability or stopping distance has been established for this image.

## Stationary flash/IR check - 2026-11-02

User authorised flashing and a stationary IR check only. Verified prepared hash,
rebooted the identified Pico into BOOTSEL, checked RP2 drive identity and copied
the integrated image. It enumerated as COM6 with firmware serial MTK3PICO (the
standalone SDK image used E66480454F7FA236 on COM5). The first monitor attempt
matched the old serial and timed out without opening a port or sending commands.
After checking the descriptor source and inventory, selected MTK3PICO and verified
the LOCAL LINE PROFILE banner. Sent only h/i/x: help, diagnostics and stop.
No reset/arm, movement or IR calibration command was sent.

Ten snapshots over about 30 seconds: motor duty 0/0, encoder counts 0/0,
line active=0 throughout. Initial and final stop acknowledged. IR acquisition
running with no faults or first-fault reason, no handoff errors. Samples advanced
72241 -> 99521; max lateness 126 us, max scan 170 us. Motion max loop 39 ms,
no timeout; application/radio release complete. Only live source bit 0x2 (IR)
was asserted, with latched motion fault 0x40: expected while IR state is
UNCALIBRATED, calibration_samples=0 and failed_channels=0x7.

Observed raw ranges: GP26 152..179, GP27 180..210, GP28 291..1833. GP28 changed
substantially, but the viewed surfaces/placement were not controlled or recorded;
this does not establish black-white contrast or prove the earlier issue fixed.
No line snapshot is available before IR setup. Calibration remains blocked,
metric validity remains false, and line following has not been physically tested.

Evidence: ignored tools/dashboard/.local/line-stationary-20261102-180347 JSON/log.
Next step is stationary, controlled black/white exposure using the existing IR
setup workflow. This is separate from the wheel calibration the user shelved.

USB diagnostics nuance: dropped-record counter increased 443 -> 444 (40 bytes)
during the snapshot window, with one contention and no truncation. Earlier drops
were already present at the first snapshot. All ten requested diagnostic sets
were captured, and no IR timing fault occurred; this is not a zero-drop console
validation. Do not infer missing event details from the log.

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

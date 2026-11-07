# IR handoff across STOP and RESET - 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Status: **flashed; eight stationary STOP/re-arm cycles and a two-second lifted
run with powered STOP passed.** The recovery path was exercised during the
stationary cycles. Loaded track and steering-transition validation remain pending.
Installed firmware is now the following IR handoff correction image:
`build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`, SHA256
`FD23C3C27C425B3DB8F765923A25E5EB431317A23A65D7FDD1608B70F3CEC007`.
The previous artifact is backed up at
`tools/dashboard/.local/gp20-low-speed-installed-b6aa42.uf2`.

## Trigger and behavior

The right-turn preparation failed after USB RESET, and a later successful
right-turn run latched an IR fault after STOP. Both reported handoff stage 2,
E_OBJ (-41), stopped acquisition and motion fault 0x40/source 0x2. Evidence:
`tools/dashboard/.local/gp20-right-response-20261103-035845.{log,json}` and
`tools/dashboard/.local/gp20-right-response-20261103-040206.{log,json}`.
See [GP20_LOW_SPEED_CONTROL.md](GP20_LOW_SPEED_CONTROL.md) for the motor results.
Those captures did not include the rejected epoch/timestamp values, so the
precise cause of each hardware rejection was not independently observed.

A deterministic test reproduces a race consistent with these captures: S3 reads
a coherent motion epoch; STOP/RESET changes it before S3 publishes; B2 rejects
the old-epoch sample; the bridge/runtime treats the rejection as a real IR
failure and stops acquisition. The epoch check is necessary, but this expected
rejection must be distinguished from actual failures.

The publishers now return the explicit non-error status MOTION_IR_SUPERSEDED
when the epoch differs under the mailbox lock. E_OK still means accepted; the
new status means NOT accepted. Both in-repo callers are in motion_ir_bridge.c
and handle it explicitly. Duplicate/out-of-order samples in the same epoch,
full barcode mailbox, malformed input, and lock errors retain their failures.

On supersession the bridge discards its line tracker and partial/completed
barcode. The runtime discards queued ADC frames, marks the epoch unknown and
keeps acquisition running. A subsequent coherent status read establishes the
new epoch and a fresh timestamp fence; only frames acquired after that fence
can be published. Old samples are never relabelled with the new epoch or retried.
This path neither starts motion nor clears a real health fault. Invalid/gapped
sampling remains a fault even if an epoch change happens concurrently.

The diagnostic i output now includes epoch_resyncs alongside the existing
handoff error fields. It counts this recovery path separately from real handoff
errors. Normal epoch observations without a publication race are not counted.
The timestamp fence now reads the clock after observing the motion status.

Changes are in the B2 publisher/bridge, S3 runtime/status, and one S1 diagnostic
print. No Buddy 4 code, wheel tuning, line-loss timeout, GP20 behavior, motor
limits or stop logic changed. The intermittent LED observer issue is separate.

## Validation

The new `testing/subsystem2/ir_handoff_integration_test.c` includes the real
motion publisher, IR bridge/runtime and sensor algorithms with a mocked kernel
and ADC. It injects a coherent epoch transition at the publication lock boundary
(simulating STOP/RESET publication); the separate owner-loop suite exercises
actual owner STOP/RESET behavior. The regression failed before the fix and passes
afterward. Cases cover line and barcode races, epoch wrap, fresh-frame fencing,
no replay or motor start, preservation of existing health faults, duplicate and
out-of-order timestamps, malformed input, mailbox-full, contention/status errors,
sampling gaps including concurrent epoch changes, and stale queued frames.

All 21 host executables pass (including the new test in default and button30
profiles). Firmware builds without warnings/errors: text/data/BSS
391300/184/85660 bytes. Local logs:
`tools/dashboard/.local/gp20-ir-handoff-validation.log` and
`tools/dashboard/.local/gp20-ir-handoff-build.log`.

Standalone regression from repo root:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Wno-type-limits -Wno-sign-compare -DMOTION_LINE_DEMO=1 -DMOTION_LINE_CIRCUIT=1 -Iinclude testing/subsystem2/ir_handoff_integration_test.c app_program/subsystem2/motion_logic.c app_program/subsystem3/sensor_line.c app_program/subsystem3/barcode_sense.c -o ir_handoff_test
./ir_handoff_test
```

No hardware commands, flash or motor test was performed during implementation.
Prompt before flashing/testing, as requested. After approval, verify stationary
startup, then separately check STOP/re-arm without motion while monitoring fresh
IR, handoff_errors and epoch_resyncs. Only then repeat a bounded lifted motor
trial; loaded track behavior is still unvalidated. A recovery count of zero does
not demonstrate that a race was exercised on hardware.

## Authorized flash and stationary startup check

User replied ready to flash. Verified the sole RPI-RP2 drive E:, its board
identity and exact FD23C3... artifact SHA256 before copying
`gp20_ir_handoff.uf2`. Evidence:
`tools/dashboard/.local/gp20-ir-handoff-20261103-041312.{log,json}`.
Commands were h and six i requests only. No motor start, reset, calibration,
STOP or software-BOOTSEL command was sent during verification.

All six motion snapshots: IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero encoder counts and zero motor duties. All six GP20 records: released,
armed, not consumed, no reset pending. All six LED modes were armed (1).
IR READY/running with fresh samples, no acquisition or handoff errors. New
`epoch_resyncs` field is present and zero throughout; this verifies startup,
not that the recovery race was exercised. Right line sensor remains over black.
The earlier intermittent LED observer issue is not claimed fixed.

Next: obtain approval for stationary STOP/re-arm cycles without pressing GP20,
then verify fresh IR and no handoff faults across those transitions. A powered
run remains a separate next step. No movement test was performed with FD23C3...

## Stationary STOP/re-arm validation

User approved the check with GP20 left unpressed. Evidence:
`tools/dashboard/.local/gp20-stationary-handoff-20261103-041508.{log,json}`.
Only i, x and r commands were sent: eight STOP/RESET-arm cycles, then a final
STOP to leave the board disarmed. No motor-start, calibration, boot or flash
command was sent. Each transition was checked for expected epoch change and
RESET request ID, zero duties, released GP20, fresh IR and advancing samples.

All eight cycles passed with zero handoff errors. epoch_resyncs advanced from
0 to 7, so actual old-session rejection/recovery paths were exercised, not merely
inferred from an error-free idle check. Multiple rejected frames can occur at
one transition; the counter is recovery events, not necessarily unique resets.
All recorded motor duties and encoder counts remained 0/0.

Final status: epoch 20/id 9, IDLE, no motion faults, GP20 disarmed, duties/counts
0/0, IR READY/running, line age 1 ms, handoff_errors 0 and epoch_resyncs 7.
This supports the fix for stationary STOP/re-arm transitions. Powered STOP,
left/right transitions and loaded track performance remain separate validation.
No further motor run was started.

## Powered STOP check after the handoff fix

User authorized a two-second lifted-wheel test, then pressed GP20 once with
GP27 white / GP28 black. Evidence:
`tools/dashboard/.local/gp20-right-response-20261103-041618.{log,json}`.
The logger verified healthy preflight, issued r to re-arm, monitored the physical
button start, and sent x exactly 2.000 seconds after detecting the queue/start
message. No flash, calibration or subsequent motor run was performed.

The final running snapshot showed target 750/300 counts/s, measured filtered
751/321, electrical duties -90/+51 permille, and no motion faults. After STOP,
three snapshots showed IDLE/CANCELLED, faults 0, epoch 22/id 11, zero duties,
GP20 disarmed, and stable raw counts 4294965346/1423. IR remained READY/running,
samples advanced, line ages were 8/1/1 ms, handoff_errors stayed zero, and the
source mask remained zero. Frozen last-control speeds 756/324 in post-STOP
records are historical, not ongoing wheel movement.

The resync counter stayed at 7 from the earlier stationary test: this moving
STOP succeeded but did not record a new publication race. Combined evidence
now covers exercised stationary recovery plus a clean powered STOP. One
transient LED error-mode snapshot still occurred during fault-free running;
that separate observer-indicator issue remains unresolved. Next recommended
step is a separately authorized short USB-connected track trial, beginning on
a straight section before a gentle curve. No loaded tracking success is claimed.

## Untethered track video review

User supplied `C:/Users/joell/Downloads/Telegram Desktop/video_2026-11-03_04-24-00.mp4`
after switching to powerbank operation. Video metadata: 14.47 seconds,
2160x3840, approximately 29.79 frames/s. Reviewed extracted frames across the
clip and at half-second intervals around the bend; local inspection artifacts
are in `tools/dashboard/.local/video-20261103-042400/`.

Visible sequence: the car advances along the right straight, begins rotating
left into the bend around 9.5-11 seconds, then runs wide of the curve. By the
last frames, the front sensor area is beyond the black arc on its outer side.
This is a loaded curve-tracking failure; it is not evidence of absent steering
or reversed steering sign. Camera movement and occlusion limit precise path,
wheel-speed and sensor-footprint measurements. No LED/fault code was inferred
from the video. The user confirmed that the robot stopped by itself.

The sequence is consistent with a late/insufficient curve correction followed
by line loss, but the stop cause is not established without telemetry. The
OUTSIDE-mode two-second both-white timeout, speed response time and minimum
rolling turn radius are plausible contributors, not measured diagnoses for
this clip. Preserve power if possible and obtain stop diagnostics before
choosing another control change. Asked whether powerbank is the only supply,
since swapping its USB connection may reset the Pico and lose RAM fault state.
No firmware, calibration, flash or motor command was changed/sent during review.

The user then confirmed the robot was already powered off. The volatile stop
record from this run is therefore unavailable; do not attribute the stop to a
specific fault or claim it was the 30-second timeout. No reconnect is needed to
recover that lost record. The video supports improving loaded curve response
(slower approach/tighter correction); gains and thresholds remain unchanged.

## Subsequent curve response candidate

The user requested an improvement for repeated curves after the filmed wide
turn. A slower, earlier/tighter, faster-responding curve profile with a brief
exit-speed hold is built as 1A4342... . See
[GP20_CURVE_RESPONSE.md](GP20_CURVE_RESPONSE.md) for settings, tests and limits.
It has not been flashed; installed firmware remains FD23C3... .

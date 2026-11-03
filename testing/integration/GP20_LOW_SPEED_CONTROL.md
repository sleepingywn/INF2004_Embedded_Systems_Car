# Line-following low-speed correction - 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Current installed firmware is the subsequent FD23C3... IR handoff correction;
see [IR_HANDOFF_EPOCH_FIX.md](IR_HANDOFF_EPOCH_FIX.md). The B6AA42... tests and
results below are historical and remain the latest powered speed validation.

Status: **flashed; lifted tests show improved speed tracking in both directions.**
A STOP/re-arm IR handoff fault remains unresolved. Transition and loaded track
validation remain pending.
Installed firmware is now the low-speed correction below. The image is
`build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`, SHA256
`B6AA42FC216845E47A9691573E1F070DA17F7CBF5CCA8EBF613C3F0BF82D96A3`.
The installed image was backed up as
`tools/dashboard/.local/gp20-time-fix-installed-43e175.uf2` before building.

## Problem and change

The lifted GP27-black/GP28-white trial requested about 400/780 counts/s, but
measured about 865/1200 and held PWM near 101/132 permille. Its two-second host
window completed without a timing fault. Evidence and limitations are in
[GP20_SMOOTH_STEERING.md](GP20_SMOOTH_STEERING.md), with raw capture at
`tools/dashboard/.local/gp20-wheel-response-20261103-034519.{log,json}`.

The fixed 155/185-permille feed-forward bias could only be reduced by 40 permille
of integral correction. The observed plateau matches that lower bound. The
line profile now sets a per-wheel integral floor equal to the negative of its
full feed-forward bias: -155000/-185000 milli-permille. This lets the regulator
reduce power sufficiently for low positive speed targets. Positive integral
limit remains +40000. Straight-bench initialization retains its original
+/-40000 integral limits and minimum PWM.

Recovery tests also reproduced a shared anti-windup rounding defect. Rejecting
an entire integral increment that barely exceeded the PWM slew window could
freeze duty permanently (reproduction: duty 52, integral -129130, feed-forward
155, zero measured speed and target 900). The controller now accepts the part
of the integral increment that fits the output window. It still rejects
accumulation farther into saturation and permits unwinding. This arithmetic
correction applies to the shared speed controller, including the straight bench;
its existing gain, speed target, PWM and integral limits are unchanged.

Implementation: `app_program/subsystem2/forward_speed.h` and `line_follow.h`.
No Buddy 4 code changed. Steering targets remain 300..900 counts/s; target slew,
PWM slew, 20-percent maximum output, immediate STOP/fault handling, 200 ms stall
limit, input freshness, GP20 start and 30-second device maximum are preserved.
The earlier timestamp-ordering fix remains included. Indicator transient-error
behavior and the OUTSIDE-mode two-second both-white ambiguity are not changed.

## Verification

- The overspeed regression failed with the original controller.
- A larger negative bound alone failed recovery; the rounding correction makes
  it pass. The focused reproduction now advances PWM within its slew limit.
- New `testing/subsystem2/line_speed_control_test.c` covers overspeed correction,
  unchanged straight integral bounds, prolonged saturation and recovery, zero
  target/reset, both turn directions, full turns, return to center, and 20/40/60
  ms intervals. Three illustrative motor/load models reach requested speeds;
  they are not calibrated hardware models or evidence of floor performance.
- All 19 host executables passed, including owner-loop timing, stale IR,
  line loss/junction, encoder/stall, STOP, expiry and button/indicator checks.
  An owner-test deadline assertion now observes actual request completion,
  rather than the last nonzero PWM write: a regulator can already coast at zero
  before its request expires. Completion bounds and fault checks remain.
- Firmware compiled successfully. Build text/data/BSS: 391108/184/85652 bytes.
  Local logs: `.local/gp20-low-speed-validation.log` and
  `.local/gp20-low-speed-build.log` under `tools/dashboard/`.

Standalone new regression (from repository root, GCC):

```sh
gcc -std=c11 -Wall -Wextra -Werror -DMOTION_LINE_DEMO=1 -DMOTION_LINE_CIRCUIT=1 testing/subsystem2/line_speed_control_test.c app_program/subsystem2/motion_logic.c -lm -o line_speed_control_test
./line_speed_control_test
```

## Next hardware validation

Prompt before flashing or motor tests, as requested by the user. After an
approved flash, verify stationary startup first. Then separately authorize a
short lifted-wheel trial and compare requested/measured speeds for fixed left
and right error, followed by return-to-center. A host two-second STOP is not a
two-second firmware limit: keep USB connected and wheels secured. Greater
low-speed authority may expose real motor breakaway/stall behavior under load;
the existing stall stop remains active. No floor-run success is claimed.

## Authorized flash and stationary verification

User replied "ready to flash". Verified the sole RPI-RP2 drive (E:), its
INFO_UF2.TXT board identity, and exact B6AA42... SHA256 before copying
`gp20_low_speed.uf2`. Evidence:
`tools/dashboard/.local/gp20-low-speed-20261103-035515.{log,json}`.
Commands were h and six i requests only; no motor-start, reset, calibration,
STOP or software-BOOTSEL command was sent.

All six motion snapshots: IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero encoder counts and zero motor duties. All six GP20 snapshots: released,
armed, not consumed, no reset pending. IR READY/running, no acquisition faults.
The pre-existing intermittent indicator issue remains: four records show armed
mode 1, two show transient mode 4 despite fault-free motion records. Do not
claim continuous slow blinking. This flash did not include a wheel test; the
low-speed improvement is not yet physically validated.

## Lifted left-turn validation with low-speed correction

User authorized the two-second test and pressed GP20 with wheels lifted,
GP27 black and GP28 white. Evidence:
`tools/dashboard/.local/gp20-wheel-response-20261103-035612.{log,json}`.
Host STOP was sent 2.000 seconds after detecting start; the device limit remains
30 seconds. No timing, IR, stall or encoder fault occurred. Three post-STOP
records confirm IDLE/CANCELLED, no faults, GP20 disarmed, zero duties and stable
raw counts left 4294965914/right 2190. No second run was started.

Error stayed +79..80. Speeds decreased through 1273/1595, 832/1177, 664/1041,
544/941, 472/853, 445/827 and 426/801 counts/s. The final running snapshot in
the complete log was 406/792 against requested 396/780, at electrical duty
-59/+93 permille. Frozen last-control diagnostics after STOP were 405/794
against 402/782; those are not post-stop physical wheel speeds. Compared with
the prior approximately 865/1200 plateau, the requested differential speed is
now closely approached under this fixed, lifted-wheel condition.

This supports the low-speed correction but does not prove loaded curve tracking
or transient response between left/right/center. A mirrored right-turn test was
requested next; no flash, calibration or code changes were made during this trial.

## First right-turn attempt blocked before movement

User confirmed right placement and wheels lifted. Logger preflight initially
showed IDLE/no faults, fresh error -100, GP27 raw 193 and GP28 raw 3141. After
USB r, IR stopped with handoff stage 2, E_OBJ (-41), handoff_errors 1, runtime
state FAULT/running 0; motion had external fault 0x40, source mask 0x2. GP20
never armed and no TRACE was submitted. Logger sent x; three subsequent records
confirmed zero duties and GP20 disarmed. Evidence:
`tools/dashboard/.local/gp20-right-response-20261103-035845.{log,json}`.

The failure coincided with re-arm epoch transition. motion_publish_line rejects
an epoch mismatch or repeated/out-of-order timestamp with E_OBJ, and IR runtime
latches a handoff error; exact rejection cause was not separately logged.
Do not label this as a low-speed controller or right-wheel failure. User asked
to tap RESET without BOOTSEL, keeping wheels lifted. Right-test logger now uses
an already-armed fresh boot without redundant r, verifies fresh rightward line
input and healthy IR first, and retains STOP/zero-output verification. This is
a test-workflow adjustment, not a firmware repair of the handoff issue.

### Restart recovery pending

After the user replied restarted, right-test preflight could not find MTK3PICO;
no serial commands or motor commands were sent. Evidence:
`tools/dashboard/.local/gp20-right-response-20261103-040020.{log,json}`.
A further 15-second enumeration check found no Pico serial port. E: instead
contained valid RPI-RP2 INFO_UF2.TXT; present USB identity was VID_2E8A/PID_0003
with working mass-storage interface (RP2 Boot interface driver error is the
previously known separate picotool-interface issue). Thus the board was in ROM
BOOTSEL mode. Requested RESET with BOOTSEL released, USB connected, main switch
ON and wheels lifted. No image was copied and no driver changes were made.

## Right-turn retry after normal restart

Normal restart restored MTK3PICO. Logger verified idle, fault-free startup,
GP20 already armed and fresh right-black error -100. It reused the boot arm;
no r command was sent. User pressed GP20 once with wheels lifted and placement
fixed. Evidence:
`tools/dashboard/.local/gp20-right-response-20261103-040206.{log,json}`.
The host sent x 2.000 seconds after detecting the start; no further run occurred.

During TRACE, targets were 750/300 counts/s. Measured filtered speeds reduced
through 1194/1424, 1022/975, 909/698, 824/493, 800/399, 785/352 and 760/343.
The last running snapshot had PWM -91/+52 permille and no motion faults.
Post-STOP frozen last-control diagnostics were 753/309 against 750/300; these
are retained running measurements, not wheel speeds after STOP. Both directions
now show substantially improved target tracking in fixed lifted tests. This
is not a loaded track validation or a left/right transition test.

After STOP, the IR handoff fault recurred: runtime state FAULT, running 0,
handoff_errors 1, stage 2, E_OBJ -41 at sample_ms 50002, motion external fault
0x40 with source mask 0x2. The final running sample at 49779 was healthy; STOP
advanced the epoch, then the fault was observed. Exact rejected epoch/timestamp
values are not logged, so this is consistent with the known epoch-transition
handoff path, not a proven exclusive cause. Three post-STOP snapshots confirmed
zero duties, disarmed GP20 and stable raw counts 4294965424/1390. Motors are
stopped; do not call the full end-to-end test fault-free.

Transient LED mode 4 occurred twice while TRACE snapshots still had no faults,
consistent with the already documented unavailable/stale observer issue. No
firmware edits, flash, reset or calibration command was sent during this retry.
Investigate/fix STOP/re-arm handoff handling before another floor trial.

## IR handoff correction prepared

A software fix and regression for STOP/RESET epoch supersession are now built
as FD23C3...; see [IR_HANDOFF_EPOCH_FIX.md](IR_HANDOFF_EPOCH_FIX.md). It has not
been flashed or hardware-tested. Installed firmware remains B6AA42... and its
recorded STOP/re-arm handoff issue remains relevant until replacement/validation.

### Subsequent handoff hardware validation

Installed FD23C3... passed eight stationary STOP/re-arm cycles, with seven
counted epoch resynchronizations and zero handoff faults. See
[IR_HANDOFF_EPOCH_FIX.md](IR_HANDOFF_EPOCH_FIX.md) for evidence and scope.
No powered run of that handoff-fix image has been performed yet.

### Powered STOP validation on subsequent handoff-fix image

FD23C3... subsequently completed a two-second lifted right-turn run and STOP
with fresh continuing IR, zero handoff/motion faults, and final running speeds
751/321 versus targets 750/300. See
[IR_HANDOFF_EPOCH_FIX.md](IR_HANDOFF_EPOCH_FIX.md) for the full scope. Loaded
track and changing steering-error behavior still require validation.

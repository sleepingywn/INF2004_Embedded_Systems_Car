# Smooth rolling turns — 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Current installed image: low-speed correction B6AA42..., flashed with stationary
checks only. See [GP20_LOW_SPEED_CONTROL.md](GP20_LOW_SPEED_CONTROL.md).
The timestamp-fix and smooth-steering details below are historical.

Prepared after severe jerking and an early stop. The smooth-steering revision
CB31CCD9... subsequently failed the physical track trial (left veering), and a
lifted trial captured a timing fault. The timestamp correction was installed
as `43E175E826A05A74E80481F73213A1686BC573F36D647675012ED8065D9A3683`.
Its subsequent lifted trial completed without a timing fault, as recorded below.
The steering failure remains unresolved. Prior image backups are retained in
`tools/dashboard/.local/`. The sections below retain the investigation history.

## Findings and uncertainty

The preceding curve tuning commanded the inner wheel to zero for error
magnitude >=50. The shared speed controller applies zero immediately and
restarts a stopped wheel at its feed-forward duty (15.5% left / 18.5% right).
Repeated edge/center or threshold transitions therefore can bypass normal
PWM slew limits. The four-times-faster deceleration added for curves also
made steering more abrupt. This is a concrete software path consistent with
the reported jerking; no recorded moving trace proves it was the sole cause.

The maximum run duration is 30 seconds. Line loss/junction, stale sensor data,
stall, encoder, timing and other faults can end it earlier. A fast LED is a
rejection/fault/status-unavailable indication, not a normal time-limit indication.
The exact reason for this reported stop is **unknown**: the Pico was not visible
to serial enumeration, including after the user reported reconnecting USB while
keeping power on. No diagnostic, reset, rearm or motor command reached the robot
in this investigation. Do not label the stop as confirmed line loss or stall.

## Change

- Straight wheel target reduced from 1000 to 900 encoder counts/s.
- Outer target decreases gradually to 750 counts/s at full steering error.
- Inner target decreases gradually to a positive floor of 300 counts/s. Normal
  tracking no longer deliberately stops/restarts a wheel or requests reverse.
- Full steering range is spread across error magnitudes 0..100, rather than
  reaching a hard inner-wheel stop at 50.
- Each target changes by at most 4000 counts/s per second (80 counts/s per
  20 ms step). Uses measured control interval, including 20..60 ms jitter.
- Normal PWM increases AND decreases are limited to 250 permille/s (0.5
  percentage points per 20 ms), using the existing speed-controller limits.
- All immediate STOP/fault/expiry actions, 20% PWM cap, 200 ms encoder-progress
  stall check, fresh-line requirements and 30-second maximum remain intact.

This is provisional tuning, not a physical guarantee of smooth tracking. A
positive target is not proof a loaded wheel keeps turning. Rolling turns have
a wider minimum radius than stopping the inner wheel; very tight curves may
still be missed. The speed PI/feed-forward calibration has not been expanded
or claimed valid at every lower target. Motor stall protection is unchanged.
Buddy 3's both-white/center ambiguity and two-second unseen-line timeout remain.
No Buddy 4, IR acquisition, LED or radio changes were made for this adjustment.

## Validation and artifact

18 host executables passed. Updated line tests cover left/right sign, symmetric
monotonic positive targets, sustained curves, return to center, 49/51 threshold
chatter, full +/-100 and zero jumps, timing jitter and bounded target/PWM changes.
A held stationary inner wheel at full curve still produces STALL after 200 ms.
Existing actual-owner tests retain local-only admission, fault/STOP/expiry,
USB policies, no replay/renewal and uncalibrated-motion guards. LED and BOOTSEL
regressions also pass. Logs: `.local/gp20-smooth-validation.log` and
`.local/gp20-smooth-build.log` under `tools/dashboard` (ignored).

Full GP20 build passed without compiler warnings/errors: text 390752, data 184,
BSS 85644 bytes. Flags: `SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1
LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0`.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`

SHA256: `CB31CCD9E10EB762A1C3B1E03F73B2640C8FD172954BC4773205A7C4CCC3E65B`.

Prompt before flashing/physical testing. Use wheels lifted, Robo Pico main switch
ON and USB connected; hold Pico BOOTSEL, tap carrier RESET, release BOOTSEL.
Entering BOOTSEL clears RAM fault history, so do not claim to have recovered the
prior stop reason afterward. Start with stationary checks; the next powered
trial should be short and preferably USB-connected for actual fault diagnostics.

## Authorized flash and stationary verification

User replied "ready to flash" after the wheels-lifted/BOOTSEL+RESET instructions.
Verified E: INFO_UF2.TXT Board-ID RPI-RP2 and exact SHA256 above before copying
`gp20_smooth.uf2`. Then used the identified MTK3PICO serial console with only h
and six i requests. No start, reset, calibration, STOP or software-BOOTSEL
command was sent. Evidence:
`tools/dashboard/.local/gp20-smooth-20261103-032401.{json,log}`.

All six motion records: IDLE/COMPLETED, faults=0, epoch=3/id=1, raw_valid=1,
counts=0/0, electrical duties=0/0. All six GP20 records: released, armed=1,
consumed=0, reset_pending=0. LED mode=1 (armed) on all six records. IR READY,
running, acquisition faults=0 on all six records. This verifies stationary
startup; it does not validate the new turn dynamics or identify the previous
run's lost RAM fault record. Next powered validation should use a short section
and retain USB/power where practical for reading the reason if another stop occurs.

## Subsequent physical trial: unsuccessful

User reports this image veered left off course without apparent correction or
stopping. It has NOT passed track validation.

Evidence: `tools/dashboard/.local/gp20-offcourse-20261103-033252.{log,json}`.
After USB connection, commands were i, x, i, i, i (inspect, STOP/disarm,
verify). No motor start, reset, calibration or BOOTSEL was sent. The stream
contained startup records, current uptime about 28 seconds, zero wheel counts,
reset request id 1 and no active line trial. This is consistent with a fresh
boot, not preserved failed-run evidence; the restart cause is unknown. The
startup fault record must not be attributed to the failed run. All three
post-STOP snapshots showed IDLE, zero duties, no motion faults and GP20 disarmed.
Both line sensors read near white; stationary LINE_LOST was observed.

Code review: OUTSIDE mode returns LINE_ON/error 0 for both-white input until
2000 ms continuously white. A reading above the white threshold resets this
timer, so intermediate readings could prevent LOST. Rolling targets and slew
also reduced turn response compared with the previous inner-wheel-stop version.
These are possible contributors, not a confirmed explanation of the failure.
Next: stationary GP27-black/GP28-white and reverse checks to verify contrast and
error sign before selecting another steering change. No further flash or run.

### Stationary left-black / right-white check

User confirmed GP27 over black, GP28 over white. Five i-only samples saved in
`tools/dashboard/.local/gp20-left-black-20261103-033417.{log,json}`:
GP27 raw 2405..2433, GP28 raw 161..190; LINE_EDGE_ONLY, error +92..+93,
fault 0 and age 1..8 ms. All five samples showed motors at zero duty, counts
zero, GP20 disarmed and no motion/IR faults. This confirms the expected sensor
contrast and positive-left error for this placement, not physical steering.
Right-black / left-white verification requested next. No motor commands sent.

### Stationary right-black / left-white check

User confirmed GP27 white / GP28 black. Five i-only samples saved in
`tools/dashboard/.local/gp20-right-black-20261103-033453.{log,json}`:
GP27 raw 159..190, GP28 raw 3046..3072; LINE_EDGE_ONLY, error -100,
fault 0, age 1..9 ms. All available motion records showed zero duties,
IDLE and no motion faults, GP20 disarmed. Left raw count was UINT32_MAX
(one negative count relative to boot); no powered movement was commanded.
Both static directional placements now produce the expected signs. Right black
is substantially higher than the saved black reference 2345 and clamps to 100;
this is evidence that the saved reference differs from present conditions, not
proof that calibration caused the track failure. A both-white check requested
next to verify LOST under the current placement/lighting. No code/flash changes.

### Stationary both-white check

User confirmed both line sensors over white after waiting three seconds.
Five i-only samples in
`tools/dashboard/.local/gp20-both-white-20261103-033533.{log,json}` all showed
LINE_LOST (4), error 0, fault 0, fresh age 1..8 ms. GP27 raw 197..233;
GP28 raw 178..206. All five motion records: IDLE, zero duties, no faults,
GP20 disarmed. No motor, reset, calibration or BOOTSEL command sent.

Together these static checks demonstrate correct left/right sensor error signs
and detection of sustained both-white input in the present conditions. They do
not establish that LOST was reached during the failed moving run, or prove
physical motor stop response to LOST. The current OUTSIDE-mode two-second
ambiguity and reduced turn response remain limitations. Next useful validation
is a separately authorized short wheel-response test with wheels secured clear
of the surface, capturing sensor error, requested wheel speeds, actual encoder
speeds and PWM. Avoid another unlogged 30-second track run or blind gain change.

## Lifted wheel-response trial and timing correction (not flashed)

User confirmed both wheels lifted, then GP27 black / GP28 white and pressed
GP20 once. Logger sent RESET to arm, polled i, and sent STOP on the observed
fault before its planned two-second host deadline. Installed firmware retained
a 30-second device limit. Evidence:
`tools/dashboard/.local/gp20-wheel-response-20261103-033840.{log,json}`.

Actual TRACE was accepted. A running snapshot showed error +94, target speeds
500/759 count/s (left target still ramping), measured filtered speeds 721/853,
and electrical duties -158/+188. The next snapshot already showed ERROR,
MOTION_FAULT_TIMEOUT (0x4), LINE_STEP_TIME (5), zero duties. Last successful
control diagnostics were error +93, targets 333/761, measured speeds 1168/1502.
Those diagnostics are historical, not a sample of the failing interval.
Firmware stopped before the host STOP. Three subsequent snapshots confirmed
zero duties and GP20 disarmed. No further powered test was performed.

This demonstrates that sensor error reached the steering controller and that
requested differential speeds were produced; it does not establish loaded-wheel
tracking performance. This timeout does not prove the cause of earlier runs.

Code review found a definite timestamp-ordering bug: motion_task captured now
before update_odometry acquired its encoder timestamp. A millisecond tick during
acquisition left state.time_ms later than now; the unsigned freshness check in
line_follow_step could interpret the fresh sample as billions of milliseconds
old and return LINE_STEP_TIME. Captured telemetry does not include the exact
failing age/delta, so this is a reproduced plausible cause, not proven exclusive
cause of the physical timeout.

Added actual-owner integration scenario 14: advance the fake clock by one
millisecond during encoder acquisition after admission. It failed on the old
code with the assertion requiring fault-free completion. The correction refreshes
now after mailbox/encoder acquisition and before admission/control; loop dt and
last_update remain on their original schedule. No timeout thresholds, stop paths,
steering gains or Buddy 4 code changed. All 18 host executables pass, including
the regression in USB2/button30/auto30 and existing stale-input/fault tests.
Firmware build succeeded. Build log: `.local/gp20-time-boundary-build.log`.

New built UF2 SHA256:
`43E175E826A05A74E80481F73213A1686BC573F36D647675012ED8065D9A3683`.
NOT FLASHED. Installed image remains CB31...; saved a matching backup as
`tools/dashboard/.local/gp20-smooth-installed-cb31.uf2` before replacing the build
output. Obtain flash approval before installing; repeat a short lifted trial
before attempting another floor run.

## Authorized timestamp-fix flash and stationary verification

User replied "ready to flash" with wheels lifted. Verified RPI-RP2 on E: and
exact 43E175... SHA256 before copying `gp20_time_fix.uf2`. Installed image is now
43E175..., superseding the earlier NOT FLASHED status above.

Evidence: `tools/dashboard/.local/gp20-time-fix-20261103-034330.{log,json}`;
additional `tools/dashboard/.local/gp20-time-fix-followup.log`.
Commands were h and nine i requests only. No start, reset, calibration, STOP or
software-BOOTSEL command. All nine motion snapshots showed IDLE/COMPLETED,
no faults, zero counts and duties, epoch 3/id 1. GP20 released and armed in all
nine. IR READY/running, no acquisition faults; left-black error around +79..80
in the follow-up. No powered trial performed.

Indicator caveat: seven of nine snapshots showed armed mode 1; two showed
transient error mode 4 despite adjacent fault-free, armed motion snapshots.
line_indicator_observe uses mode 4 when its nonblocking motion-status read is
unavailable or stale; the later status print is a separate read. Return to
mode 1 without reset is consistent with this non-latched path, not a confirmed
motor fault. Exact read failure/stale cause was not instrumented. Do not claim
the LED stayed continuously in armed mode. This indicator issue remains open.

## Timestamp-fixed lifted trial: completes host window, weak speed tracking

User authorized next test and pressed GP20 with wheels lifted, GP27 black and
GP28 white. Evidence:
`tools/dashboard/.local/gp20-wheel-response-20261103-034519.{log,json}`.
Installed SHA256 43E175... . Host issued x exactly 2.000 seconds after detecting
the GP20 queue/start log. The device itself still has a 30-second maximum.
No early timing, IR, stall or encoder fault was reported. Three post-STOP
records confirm IDLE/CANCELLED, faults 0, zero duties, GP20 disarmed; final two
counts stable at raw left 4294965376/right 2625. This is one successful timing
regression trial, not proof against all possible scheduling failures.

Sensor error remained +79..80. After startup, requested wheel speeds were about
396..402 left / 780..782 right count/s. Measured filtered speeds settled near
860..870 / 1198..1219, with electrical PWM around -100..-101 / +131..+132 permille.
The left wheel is correctly commanded slower, but the measured left/right ratio
is about 0.72 rather than the requested 0.51. Lifted-wheel results are not loaded
floor-turn measurements and do not prove the cause of all earlier veering.

A concrete controller limitation matches the plateau: fixed feed-forward is
155/185 permille, but integral correction is capped at +/-40 permille.
At target 402/782 and measured 863/1198, the lower integral bound gives
155 + 0.03*(402-863) - 40 = 101.17 and
185 + 0.03*(782-1198) - 40 = 132.52 permille, matching observed outputs.
The integral state was not logged directly, so saturation is inferred from
code plus telemetry. At zero speed error this bound still leaves 115/145
permille of power; the controller cannot cancel its fixed bias at low targets.
This is the next control issue to address within the line profile, retaining
slew, 20-percent cap, stall and STOP protections. No additional flash or motor
run was performed; gains remain unchanged after this diagnostic trial.

## Low-speed correction prepared

See [GP20_LOW_SPEED_CONTROL.md](GP20_LOW_SPEED_CONTROL.md) for the completed
low-speed authority and anti-windup rounding correction, regression evidence,
and new B6AA42... image. It is not flashed; installed firmware remains 43E175...
No motor commands were sent while implementing this change.

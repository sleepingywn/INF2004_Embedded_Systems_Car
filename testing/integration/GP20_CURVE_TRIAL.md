# GP20 start with earlier curve steering — 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

A subsequent [GP20 LED indicator](GP20_INDICATOR.md) is now flashed and its
stationary slow blink confirmed. The records below describe the earlier revision.

Prepared at the user's request to replace the five-second power-on start.
The user reported that line tracking worked, but the car drove past curves.
The GP20 revision has been flashed and its stationary startup checked.
Curve tracking is **not yet track-tested**. A USB `b` command was added so future
flashes can enter BOOTSEL without the physical button; hardware entry succeeded.

## Operation after flashing

1. Power on with GP20 released. Motors remain stopped. Saved measured IR
   references load for this battery circuit profile; USB calibration/arming is
   not required with the same sensors, height, surface and lighting.
2. Wait for startup to complete. The firmware requires fresh healthy IR and
   encoder status, queues RESET, and checks its exact epoch/request ID and
   completed result before arming. USB status shows `GP20 ... armed=1` and the
   `GP20 ready` message when available. There is no timed motor start.
3. Position on the track with one line eye overlapping the black edge, then
   press GP20 for about half a second and release. One forward-only run lasts
   at most 30 seconds; USB can be disconnected for battery operation.
4. Press GP20 again to stop early. USB `x`/space also stops and disarms.
5. To prepare another run, power-cycle (remove both USB and battery power),
   or use USB `r` and wait for reset acknowledgement. No automatic replay.

A button held while arming must be released first, then pressed/released again.
An early press before readiness cannot queue later movement. Calibration
commands cancel pending startup and arming. A rejected start consumes permission.
USB `g` is disabled. Manual `r` during an active run stops it instead of rearming.
The button is polled by the diagnostic task, nominally every 100 ms, so this is
not an independent hardware emergency stop. GP20 remains input/pull-up, active-low.

## Curve changes and limits

The old controller maintained a 1200 count/s outer-wheel target, required an
error magnitude of 100 to stop the inner wheel, and reduced duty by only 0.5
percentage points per 20 ms. These are plausible contributors to overshooting;
no recorded curve telemetry establishes the exact cause.

- Straight target is now 1000 encoder counts/s. As absolute error rises from
  0 to 50, the outer target falls from 1000 to 800 and the inner target from
  1000 to zero. At error +25 the targets are 450/900; at -25, 900/450.
- Error magnitude 50 or greater stops the inner wheel, earlier than before.
  Positive error still turns left. Outputs remain forward-only, capped at 20%.
- Line mode can reduce duty by up to 2 percentage points per 20 ms; positive
  acceleration remains capped at 0.5 percentage points per 20 ms except the
  existing stopped-wheel feed-forward restart. Line startup uses the existing
  per-wheel feed-forward values instead of the straight-bench 20% kick.
- The straight-bench controller retains its old target, startup and symmetric
  slew behavior. Existing fault, stale-data, stall, count and run-time stops remain.

The S3 OUTSIDE-mode ambiguity remains: both-white can mean either a centered
line between the eyes or a missed line, and it reports zero error for up to
2 seconds before LOST. This revision does not change that sensor policy or
add blind searches. Tighter turns may still require sensor geometry/policy
work. More aggressive steering can oscillate; lower inner targets may stall.
A real curve trial must check these effects before declaring the issue solved.
Saved references are measured endpoints, not a new calibration or proof of
current sensor contrast. No Buddy 4 code or telemetry wire layout changed.

## Validation and artifact

16 host test executables passed (including the new USB BOOTSEL guard): motion-owner and console tests for diagnostic,
USB 2s, GP20 30s and legacy auto30 profiles; line-control tests for all three
motion profiles; button debounce, boot calibration, straight speed model and
feedback-bench regressions. Motion-owner tests exercise 13 scenarios per enabled
profile. Button tests cover no motion before RESET acknowledgement, held input,
one shot, stop, manual rearm, cancellation, wrong epoch, timeout and health faults.
Curve tests check target symmetry/monotonicity, direction, earlier inner stop,
faster duty reduction, acceleration bound, stall/expiry and wrap guards.
These are software checks, not a physical curve/stability validation.

The GP20 production build completed without compiler warnings or errors.
Text 389868, data 180, BSS 85632 bytes. Software log paths (ignored local files):
`tools/dashboard/.local/gp20-curve-validation.log`, `gp20-bootsel-build.log`,
and the standalone `line_bootsel_console_test.c` run.

Build flags: `SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 LINE_DEMO=1
LINE_CIRCUIT=1 LINE_AUTOSTART=0` with the existing Pico SDK/toolchain configuration.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`

SHA256: `1D98A8EE2DDC1C3B9F8836559E98FF3761F91E57D10D5CE633A5A68AF1015D40`.

The default image remains diagnostic-only. The legacy autostart profile remains
an explicit build option; it is not the selected artifact for the next flash.

## USB BOOTSEL command and hardware evidence

Send lowercase `b` through the identified MTK3PICO USB serial console. It
cancels GP20/automatic run permission and posts STOP. For at most one second,
it waits for a fresh post-request snapshot, a changed motion epoch, an
IDLE/ERROR state, and both electrical duties zero; start/reset input is ignored
while waiting. Then it calls the RP2040 ROM `UB` function, enabling mass storage
and picoboot without assigning an activity GPIO. This is a custom serial command,
not the Pico SDK reset interface or a 1200-baud reset feature. It requires a
responsive firmware/USB console; manual BOOTSEL remains the recovery fallback.

The first added reboot guard was overly strict: an idle STOP advances the epoch
but retains the old request result, and a simultaneous IR handoff fault can
advance the epoch again. The first image refused to reboot and stayed stopped.
The corrected guard checks the fresh stopped state and epoch transition instead
of requiring exactly one epoch increment and a CANCELLED result. Regression
coverage includes the real motion-owner idle STOP path, not just a mocked result.

Initial failed guard evidence: `.local/gp20-bootsel-20261103-022420.{json,log}`
under `tools/dashboard`. A stage-2 IR handoff E_OBJ fault occurred following that
STOP/epoch transition; no motor command was sent. This is not evidence of a
running line-trial fault, but remains an IR handoff/reset issue to investigate.

Corrected hardware evidence: `.local/gp20-bootsel-20261103-022810.{json,log}`.
The expected GP20 and `b` help banners appeared. Three snapshots showed IDLE,
faults=0, duties=0/0, counts=0/0 and GP20 armed/released. Sending only `b` then
removed the serial port and produced the RPI-RP2 USB drive. The same corrected
UF2 was loaded through that software-entered drive. No motor-start command was
sent during any of these checks.

The ROM call follows the installed Pico SDK 2.3.1 ABI (`0x14` function table,
`0x18` lookup entry, code `UB`). Raspberry Pi's
[ROM implementation](https://github.com/raspberrypi/pico-bootrom/blob/master/bootrom/bootrom_main.c)
performs a watchdog reboot into USB boot. Windows picotool could not claim the
picoboot interface with the current driver, so UF2 mass-storage copy was used.

A repeat remote reboot (`.local/gp20-bootsel-20261103-022944`) disconnected
serial but did not enumerate RPI-RP2. Windows reported `Unknown USB Device
(Device Descriptor Request Failed)` while the user confirmed the cable was
connected. One successful entry therefore does not establish reliable repeated
USB reconnects. A normal unplug/replug (no BOOTSEL hold) was requested to check
recovery. See the later recovery status below. No driver changes or USB hub resets were performed.

The first stationary-only host observer (022849) sent STOP in its cleanup even
on success, so its armed snapshots do not describe the state after it exited.
The observer cleanup was corrected to send STOP only on errors; final state
must be taken from the later verification. At no point did the host send a
motor-start command. A diagnostic-record drop also required a longer observation
window; this is the existing console-drop limitation, not motor motion.

Normal USB reconnect did not clear the descriptor error. A full power-off
(remove USB and battery supply), followed by normal USB reconnection, was then
requested. Remote BOOTSEL entry is therefore experimentally demonstrated but
its repeated USB re-enumeration is not yet reliable on this setup.

Recovery status at the end of the flash session: full power-cycle plus USB
reconnect still reported a descriptor failure. With explicit user approval,
Windows PnP restarted only `USB\VID_0000&PID_0002\9&205BBE2C&0&1` as administrator
(`.local/pico-usb-admin-restart.log`: device restarted successfully), but the
same descriptor error returned. No hubs or unrelated devices were reset and
no drivers were installed. The user confirmed the board power light is on;
a different known data cable/direct laptop port remains the next physical check.
The last successful live snapshots predate these recovery attempts; do not
claim current armed/IR-ready status. No motor-start commands were sent.

## Recovery instruction correction

The user reported manual BOOTSEL also did not appear, and confirmed the board
light goes completely out with USB unplugged and its switch off. Checking the
actual Robo Pico manufacturer guide exposed an incorrect recovery instruction:
leaving the Robo Pico main switch OFF is not the prescribed USB flashing setup.
Cytron explicitly requires its main switch ON for flashing/running even on USB:
https://www.cytron.io/tutorial/get-started-robo-pico-circuitpython-power-up-robo-pico

The previous repeated descriptor failures with the switch instructed OFF cannot
establish a damaged board or failed firmware. Revised check: wheels lifted,
USB unplugged and switch OFF; hold Pico BOOTSEL, reconnect USB, turn Robo Pico
switch ON while BOOTSEL remains held, then release BOOTSEL. Keep the main switch
ON for USB communication. This is a plausible recovery cause pending observation,
not a proven explanation of the first intermittent USB failure.

## Recovered and final stationary verification

User succeeded with USB connected, Robo Pico switch ON, holding the Pico
BOOTSEL button while pressing/releasing the carrier RESET button. Windows
then exposed E: RPI-RP2 (mass storage OK). The picoboot interface still lacks
its Windows driver; it was not needed for the UF2 copy.

Rechecked the UF2 SHA256
`1D98A8EE2DDC1C3B9F8836559E98FF3761F91E57D10D5CE633A5A68AF1015D40`
and RPI-RP2 Board-ID, then copied the same GP20 image. Evidence:
`tools/dashboard/.local/gp20-bootsel-20261103-024800.{json,log}`.
The corrected observation-only script sent h and six i requests, with no
STOP cleanup on success, no b reboot, and no motor-start commands. All six
motion snapshots were IDLE/COMPLETED, faults=0, raw_valid=1, epoch=3/id=1,
counts=0/0 and duty=0/0. All six button records were released, armed=1,
consumed=0, reset_pending=0. Thus the final observed state is stopped and
armed for one deliberate GP20 press/release. The updated curve behavior
has not yet been tested on the track.

Use BOOTSEL+RESET with the board switch ON for the next recovery/update;
the optional software reboot was not repeated after this recovery. The exact
cause of the earlier intermittent software-reboot/USB failure remains unproven.

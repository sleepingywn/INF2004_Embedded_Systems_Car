# GP20 battery-powered circuit trial

Superseded for the current robot by [five-second power-on start](LINE_AUTOSTART_5S.md)
at the user's request on 2026-11-02. This file retains the button-profile record.

User reported the first integrated 2-second run followed the line, then requested
a 30-second trial on the actual circuit, untethered, using the onboard GP20 button.
This is a separate opt-in firmware profile; the existing 2-second USB profile
and default diagnostic-only build keep their behavior.

## Operation

1. Flash the circuit image. It starts stopped and unarmed.
2. With USB connected, perform the existing `c` white/black IR calibration and
   `f` finish. Verify READY, all channels pass, no live faults, and fresh line data.
3. Verify GP20 reads released and responds to press/release while **unarmed**.
   Pressing it in this state cannot start the motors.
4. Power the car from its battery before unplugging USB. Keep power uninterrupted:
   calibration and run permission are RAM-only and are lost on a reboot.
5. At the intended starting position, send `r` and verify RESET completed and
   GP20 is armed. This grants exactly one subsequent button-start request.
6. Disconnect USB, position the car, then press GP20 briefly and release it.
   Hold/release for roughly half a second each so the polled input is observed.
7. The car follows the line for **up to 30 seconds**, then stops automatically.
   Press GP20 again to stop early. There is no automatic repeat or restart.
8. Reconnect USB to inspect results and explicitly arm another run if desired.

`g` does not start this profile. `x`/space stops and disarms it. Calibration
commands cancel permission. A button held at boot/arming must first be released,
then deliberately pressed and released again. A rejected start consumes permission.
The physical stop is polled by the existing diagnostic task every nominal 100 ms;
it is not a hardware emergency-stop or a hard real-time latency guarantee.

Use the **GP20 programmable button**, not the motor-driver test buttons. The
[manufacturer's Robo Pico button guide](https://sg.cytron.io/tutorial/get-started-robo-pico-circuitpython-press-the-buttons)
identifies the programmable controls; the user confirmed GP20 is available.
GP20 is claimed by S2 only in this opt-in profile, configured input with pull-up,
and read active-low. No new GPIO interrupt handler is installed.

## Behavior and limits

- Maximum PWM duty remains 200 permille (20%); forward-only speed PI and line
  steering are unchanged. GP8/9 left motor, GP10/11 right; encoder inputs unchanged.
- The motion request lease is 30000 ms, within the existing 15-bit TTL field.
  The outer wheel target remains 1200 counts/s; the inner target decreases to steer.
- The total-count guard scales from 5600 for 2 seconds to 84000 for 30 seconds,
  retaining the same 2800-count/s allowance. It is a count guard, not metric distance.
- Battery profile alone permits USB disconnection. Lost/stale/faulty line data,
  both-black junctions, encoder faults, stalls, timing faults, live source faults,
  explicit STOP and duration/count limits still stop movement. No barcode turns
  or line-search behavior has been added. A junction may stop a run before 30 s.
- The existing OUTSIDE sensor mode treats both-white as centered only temporarily;
  its 2-second unseen-line timeout is unchanged and may stop a long straight.
- No autonomous power-on calibration, persisted calibration, automatic start,
  calibrated metric odometry, or independent hardware watchdog was added.
  Buddy4's metric-valid flag remains false.

## Build and validation

Build from `build_make` using the existing configured toolchain:

```text
make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 LINE_DEMO=1 LINE_CIRCUIT=1 PICO_SDK_PATH=C:/Users/joell/.pico-sdk/sdk/2.3.1 E2U=tools/elf2uf2.exe
```

Image: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`

SHA256: `C25630737338A3B430DD7D186FE01661F87B7A2ADEC8EB6EEED21E312D3295F8`

Size: text 388496, data 176, BSS 85604. Build completed without warnings/errors.
Ignored log: `tools/dashboard/.local/line30-button-build.log`.
Build profile key includes circuit mode to prevent stale object reuse.

Host validation passed:

- Actual motion owner loop: 13 scenarios in both 2-second and 30-second profiles,
  including full-duration completion, USB disconnect behavior per profile,
  one-wheel stopping for turns, no request renewal/queueing, stalls, invalid
  encoders, stale line, explicit STOP, and late IR/junction faults.
- Line core runs through the full duration across timer wrap; duty caps, scaled
  count guard and expiry checked; sensor steering sign/invalid inputs retained.
- GP20 pure state machine: held-at-arm, bounce, timer wrap, one-shot start,
  immediate stop event on observed press and no replay.
- Actual console: GP20 start, USB `g` refusal, one-run permission, stale/failed
  health checks, fault arriving after arming, cancellation by calibration/STOP,
  and re-arm during motion requesting stop rather than another run.
- Default profile still rejects movement keys and LINE_TRACE admission.

These are software tests, not evidence of a 30-second physical circuit run.
Manual BOOTSEL and authorised flashing completed on 2026-11-02 at approximately
18:41 Singapore time; see the stationary validation record below. No 30-second
motor run has been issued.

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

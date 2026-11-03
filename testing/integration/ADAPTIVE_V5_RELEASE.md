# GP20 adaptive line following: V5 release guide

## Current status

V5 was built, flashed and identified by its exact startup banner on 4 November
2026. Six stationary snapshots showed zero motor outputs, GP20 armed, and no
motion or IR faults. The operator then reported "ok this is good" and requested
this PR. That is positive feedback from the latest physical test; its duration,
full-course completion and post-run telemetry were not provided.

This PR builds on the telemetry/line-following branch in PR #3. It includes the
previously uncommitted GP20, LED, IR handoff, speed-control and adaptive tracking
work through V5, not just the final two-count change. Earlier trial records are
retained for traceability; this guide describes the current recommended profile.

## Expected robot behavior

1. Power on with motors stopped. The V5 profile never starts on a five-second
   timer. It loads this robot's saved IR reference values and waits for healthy,
   fresh inputs and a deliberate GP20 press/release.
2. A slow green blink on the Pico W indicates GP20 armed. Put the robot on the
   black line at its calibrated sensor height. Press and release GP20 to start
   one battery-powered run of at most 30 seconds. USB and Wi-Fi association are
   not required for the local run.
3. Filtered left/right IR differences select wheel-speed targets; encoder speed
   feedback regulates the motors. The inner wheel can deliberately pause for
   tight steering. There is no fixed curve-duration cutoff in adaptive tracking.
4. If both sensors see white, treat the line position as uncertain and attempt
   bounded reacquisition. Both-black may be a crossing: the approved local trial
   policy tries straight ahead within time/count limits, then reacquires or stops.
5. Press GP20 again to stop. Faults, exhausted recovery/crossing limits and the
   30-second bound also stop the outputs. No automatic repeat is requested.

LED: slow blink = armed; two pulses = request pending; steady = accepted tracing;
fast blink = rejected/fault/stale status; off = stopped, completed or disarmed.
The LED is a status aid, not confirmation that the sensors are correctly placed.

## Changes included

- **Motor control:** softer startup/restart, wheel-speed feedback and ramps,
  minimum rolling speed, bounded output and adaptive pause hysteresis. V5 retains
  V2/V4 steering settings so the encoder-stop correction can be compared separately.
- **V5 pause settling:** admit at most two gross backward encoder counts only
  before 120 ms of an intentional zero-target/zero-duty pause. A third count,
  late backward movement or powered-wheel reversal still stops the robot.
  Bounce/short restart pulses cannot renew the allowance. Renewal requires
  60 ms and three counts of measured, powered forward rolling at the minimum
  rolling target or above. These are provisional count/time limits, not metric
  calibration or course-specific timings.
- **IR handoff:** publish calibrated sensor levels atomically with the existing
  line message and epoch. A STOP/reset race discards superseded samples and
  partial barcodes, resynchronizes acquisition and preserves genuine IR faults.
- **Crossings and recovery:** bounded straight continuation, stable reacquisition
  confirmation and explicit stop reasons. Barcode C permits straight; A/B/D
  stop for unsupported route execution in this GP20 trial.
- **Diagnostics:** saved paged line history, rejected encoder deltas, prior
  targets/duties, pause eligibility, used allowance and settling-window ages.
- **Button/indicator/build:** deliberate GP20 start/stop, deferred Pico W LED
  servicing in the radio task, saved circuit calibration and distinct profile
  names/configuration guards. Default non-demo builds remain diagnostic-only;
  legacy search/autostart profiles remain explicitly selectable historical modes.
- **USB boot entry:** a guarded software BOOTSEL command is included from earlier
  work, but Windows USB recovery was unreliable during physical attempts.
  Manual BOOTSEL is the recommended and verified flashing workflow.

## Wiring and calibration

| Function | GPIO |
| --- | --- |
| Left motor inputs | GP8 / GP9 |
| Right motor inputs | GP10 / GP11 |
| Left encoder A/B | GP4 / GP5 |
| Right encoder A/B | GP2 / GP3 |
| Barcode analog signal | GP26 |
| Left line analog signal | GP27 |
| Right line analog signal | GP28 |
| Onboard start/stop button | GP20, active low |

Saved white/black ADC references are GP26 183/2985, GP27 173/2623,
GP28 160/2533. These belong to the measured robot and placement; recalibrate
when sensor height, mounting, lighting, surface or hardware changes. With the
current OUTSIDE placement, both-white cannot uniquely distinguish a centered
line from a missing line. No GPIO or sensor arrangement is auto-detected.

`MOTION_CALIBRATED` remains zero: count-based feedback does not certify physical
speed, distance or turning angle. Buddy 4 implementation is not changed here.

## Build and verify

Use the toolchain/SDK setup in the root README. The installed image was built
with Pico SDK 2.3.1 and Arm GNU Toolchain 15.2.Rel1. In a POSIX/MSYS shell with
`arm-none-eabi-gcc`, host compiler, make and `PICO_SDK_PATH` configured:

```sh
make -C build_make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 \
  LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1
```

On the validated Windows MSYS setup, pass `E2U=tools/elf2uf2.exe` as well.
The output is
`build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_adaptive.uf2`.
Its startup banner must identify **ADAPTIVE v5**. The locally flashed artifact
SHA-256 is `A152BE67F5CEF02319A5B4E1A8E5E4D32F7FA69311A08D53CECDE81FB6ABC3C8`;
rebuild hashes can differ with local credentials, toolchains or build metadata.
Binary artifacts, Wi-Fi secrets, runtime logs and videos are intentionally ignored.

With Python 3 and host GCC on PATH, from the repo root:

```sh
python testing/subsystem2/run_adaptive_tests.py
python testing/subsystem2/run_regression_tests.py
```

These access no hardware: eight adaptive executables (including 39 motor-owner
scenarios), 38 regression executables, and two invalid-profile rejection checks.
All passed locally. Firmware compiled without warnings/errors. Tests cover
accepted settling, time/count boundaries, sustained/powered reversal, bounce,
rearming, time/count wrap, STOP/health faults, IR epoch races and crossing bounds.

For flashing, secure the wheels off the surface, enter manual BOOTSEL and copy
the intended UF2. Verify the version with USB console `h`; use `i` to check
GP20 armed, motor duty 0/0 and no faults before placing the robot on the track.
Record a run from the same starting position/direction for comparison. If it
stops unexpectedly, preserve power and read status/history (`i` / repeated `t`)
before RESET, BOOTSEL or another run overwrites the evidence.

## Remaining limitations

- Varied/unseen tracks and repeated runs still need validation. This is a bounded
  GP20 test profile, not the complete autonomous mission or arbitrary-course proof.
- Steering oscillation was seen in previous logs and is not claimed fixed by V5.
- Backward encoder counts could be physical settling or valid-looking electrical
  noise. The new bound handles the observed pattern without establishing its cause.
- Automatic barcode-directed turns and calibrated metric navigation remain open.
- Saved calibration is hardware-specific; USB software boot recovery is unreliable.

See [the V5 investigation and verification record](ADAPTIVE_V5_BOUNDED_PAUSE_SETTLING.md)
for the change rationale and physical evidence references. Ignored local evidence
paths in historical notes identify the development session; reviewers can reproduce
the software checks above without those logs.

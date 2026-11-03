# GP20 curve response and bounded continuation

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

## Evidence and scope

Candidate prepared after `video_2026-11-03_23-03-17.mp4` and the saved
`tools/dashboard/.local/gp20-startup-result-20261103-230251.{log,json}` run.
The video shows a shallow left turn running wide. Firmware elapsed time is not
the video's timestamp. At 5719 ms, targets were 299/533 counts/s but filtered
wheel speeds were 391/463; at 6619 ms targets were 293/535 versus 348/523.
Directional IR input was present before loss. At 7519..8019 ms the controller
requested equal 350/350 speeds, then searched and stopped at 9599 ms with
SEARCH_TIMEOUT. No encoder, stall or acquisition fault was reported. This
supports delayed/insufficient steering response, not a proven defective motor.

This candidate changes Buddy 2's search-enabled line profile only. Existing
sensor calibration, pin mapping, acquisition and Buddy 4 remain as before this
change. Other profiles retain their earlier gains and target behavior.

## Behavior

- Cruise remains 450 counts/s. Full rolling correction is now 250/600 by
  error magnitude 60, previously 250/550 only at magnitude 100. Full-turn
  average speed is 425 counts/s, below cruise, with a larger wheel-speed ratio.
- The line profile's proportional speed gain changes from 60 to 120 milli;
  integral gain remains 480. Target deceleration changes from 2000 to 4000
  counts/s per second, and downward PWM slew from 500 to 1000 permille/s.
  Upward target/PWM slew remains 2000 counts/s per second / 250 permille/s;
  duty remains capped at 200 permille. No reverse commands are introduced.
- A real error magnitude at least 30 within 200 ms of entering ambiguity
  selects a 300 ms curve grace. While error is zero, continue that direction
  with 250/500 targets, subject to normal target ramps. A pre-existing tight
  pause retains its 100 ms ambiguity release before rolling continuation.
  This replaces 600 ms of straight commands immediately following a bend.
- Without recent curve evidence, centered-start grace remains 600 ms at
  350/350. Weak real errors during grace now use their actual steering instead
  of being discarded. Any opposite real error clears remembered curve direction.
  Synthetic turns never refresh the timer. Weak noise cannot extend the grace.
- After grace, retain the existing bounded two-arc search: 1500 ms or 1000
  counts on either wheel, with real-input confirmation. Thus a failed recent
  curve recovery stops after approximately 1.8 s of ambiguity; centered/no-curve
  recovery retains approximately 2.1 s. The 30 s run deadline still wins.

The additional simulation initially found that reducing the inside target
immediately on startup could undo the matched 8% power seed before the first
encoder count, causing a stall with a larger modeled breakaway deadband.
`line_startup_retarget` cancels only that proportional target-change jump
while accumulated travel is zero. Normal feedback and upward slew still apply;
the original 200 ms no-progress deadline is unchanged. Positive travel restores
full target-change response. No minimum-power override or stall exemption was
added. Paused-wheel restart remains bounded by the existing soft restart.

## Validation and additional review

All 34 host executables passed: the existing 32-profile suite, the IR acquisition
test, and `testing/subsystem2/line_curve_recovery_test.c`. The new test covers:

- Both turn directions, stronger rolling target ratios and faster inside-wheel
  power reduction, with immediate zero-target stop.
- Recent versus stale curve memory, weak opposite evidence, exact grace/search
  deadlines, reset and unsigned timer wrap.
- Three illustrative motor-response assumptions, both wheel controllers, and
  repeated opposite turns with short white gaps, a load change, and control
  intervals varying from 20 to 60 ms. These models cannot validate floor grip,
  physical breakaway torque, a turn radius or electrical sensor stability.

Existing owner tests also verify final electrical outputs are zero for STOP,
stale IR, invalid encoder input, external fault, search expiry/range and run
completion. Startup checks retain the 8% seed, normal acceleration limits and
the 200 ms stall deadline. Calibration and ADC sample propagation tests pass.
Tight-turn dwell/cooldown and pause reverse-count limits remain unchanged.

Reproduce the added host regression from the repository root (GCC on PATH):

```powershell
gcc -std=c11 -Wall -Wextra -Werror -DMOTION_LINE_DEMO=1 -DMOTION_LINE_CIRCUIT=1 -DMOTION_LINE_SEARCH=1 testing/subsystem2/line_curve_recovery_test.c app_program/subsystem2/motion_logic.c -lm -o "$env:TEMP/robocar-curve-recovery.exe"
& "$env:TEMP/robocar-curve-recovery.exe"
```

Local logs: `.local/gp20-startup-validation.log`,
`.local/gp20-curve-recovery-validation.log`, and
`.local/gp20-curve-recovery-build.log` under `tools/dashboard/`.
Firmware builds without compiler warnings/errors; text/data/BSS:
397268/184/90084 bytes.

Profile: SMP1 / USB CDC / CYW43 / LWIP / MQTT / LINE_DEMO=1 /
LINE_CIRCUIT=1 / LINE_AUTOSTART=0 / LINE_SEARCH=1.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_search.uf2`

SHA256: `97415642C22D17D696BB630646F1CD643B9A1F26B5A009F9986A1FDA7DF25CA3`

Previous installed artifact saved and hash-verified at
`tools/dashboard/.local/gp20-startup-installed-4ab1c1.uf2`:
`4AB1C1A35BF2A183307040B1A5512F2033F4188B6211774C286FBA32F4FAEB99`.

## Next physical check

The user subsequently approved flashing. The sole RPI-RP2 volume on E: and
Board-ID RPI-RP2 were verified, along with the exact candidate hash above.
The candidate was copied and normal USB serial returned. Stationary verification
is recorded in `tools/dashboard/.local/gp20-curve-recovery-stationary-20261103-232148.{log,json}`.
The new curve/startup/search banners matched. Six snapshots showed zero motor
outputs, no motion fault, and GP20 unpressed/armed. All six IR snapshots had
zero faults and increasing sample counts; GP28 raw was 224..246 at the user's
unspecified stationary placement. This is not a black/white contrast check.
Only help and diagnostic commands were sent, with no motor-start command.
LED snapshots were four armed and two transient observer-busy statuses, with
no associated motion or IR fault. No physical driving test was performed.

GP20 remains the explicit run trigger.
Keep the current sensor placement and calibration for comparison. Observe
whether the car turns earlier and stays on the bend without renewed jerking.
If it stops, retain power and retrieve history before reset or a new run.

Remaining limitations: both-white is still ambiguous with two outside sensors;
the correction can briefly steer a car that has actually re-centered. It is
deliberately bounded, not a guarantee of line presence. The earlier GP28
electrical nonresponse recovered after reconnection but recurrence under
vibration has not been ruled out. Higher feedback gain and stronger rolling
steering require physical validation; passing models do not establish success
on this track. The 64-row trace retains only about 6.4 s, so a longer run can
overwrite initial startup evidence.

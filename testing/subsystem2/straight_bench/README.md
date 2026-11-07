# Stationary direction check and straight-line trim bench

Separate Pico SDK firmware, prepared and compiled 2026-11-02 with Pico SDK 2.3.1
and Arm GCC 15.2. Build passed without reported compiler warnings. Flashed for the approved
raised-wheel check below; physical direction confirmation remains pending.
The integrated robot firmware and its IR interlocks are unchanged. This temporary
image has no Wi-Fi/dashboard, IR processing, encoder feedback or PID controller.
It is for supervised motor direction and open-loop straight-line observations.

Motor mapping and forward polarity come from the current motion configuration:
left GP8/9, B input PWM; right GP10/11, A input PWM. Confirm both wheels move in
the vehicle-forward direction with the wheels lifted before putting it down.

Power defaults to 20% each side; `f` issues exactly one 500 ms forward pulse,
then at least 500 ms rest. No movement on startup, USB connection or settings
changes. `s`, space, `h`, USB disconnect or deadline stops PWM. The deadline is
polled, not an independently guaranteed stop if firmware itself hangs. Motor
inputs LOW command stop; physical stopping distance must be observed.

Commands: `a/z` raise/lower left duty by one percentage point; `k/m` raise/lower
right duty. Each is bounded to 10–40%. `1` selects 500 ms, `2` selects 1000 ms
and `3` selects 2000 ms. Startup still defaults to 500 ms and 20% each wheel.
Commands during a pulse are ignored except stop/help; pending input is drained
when the pulse ends. Do not queue movement commands. Serial printing occurs
while motors are stopped, never during an active pulse.

## First proposed run (requires approval)

1. Flash only after approval; inspect the exact startup banner and issue stop.
2. With wheels securely lifted, request one forward pulse at 20%/20%, 500 ms.
3. Confirm each wheel's physical forward direction and automatic stop.
4. Before a floor test, obtain confirmation that the car is on a clear, level
   surface and its USB cable has enough slack to avoid steering or snagging it.
5. Align the car with a floor seam or other straight reference. Request one
   short forward run and record whether the path stays straight, bends left,
   bends right, or one/both wheels fail to turn. Do not judge a tilted initial
   heading as motor drift.
6. For a repeatable left bend, first reduce right-wheel duty by one percentage
   point; for a right bend, first reduce left-wheel duty by one. Keep both above
   the reliable starting duty; if a wheel stalls, stop and reassess the power
   setting or mechanical drag. Repeat identical starts before accepting a trim.

Visual trim is specific to surface, load and supply. It does not establish
counts per revolution, distance/speed calibration or reliable closed-loop control.
A USB tether can bias the result. Do not copy a trim into production as a measured
PID gain. Restore the integrated UF2 after this separate bench session.

Build with the installed Pico SDK, for example:

```text
cmake -S testing/subsystem2/straight_bench -B build_straight_bench -G Ninja -DPICO_SDK_PATH=<sdk-path>
cmake --build build_straight_bench
```

Initial 500/1000 ms UF2 SHA-256: `A38843860457CB31B31A8507B445073D58D4D69A843E09A34A7121076807245E`.
The Windows build used `%TEMP%/robocar-straight-build` to avoid long object paths;
a copy of the UF2 is in the ignored `build` directory beside this document.

## Raised-wheel command check — 2026-11-02

After explicit user approval and BOOTSEL readiness, verified the UF2 hash above
and RPI-RP2 identity, flashed, then checked the expected banner/settings on COM5.
Sent initial stop, exactly one `f` command (left/right 20%, 500 ms), and final stop.
Firmware acknowledged RUN, automatic pulse completion, and final stop. No repeat
or floor movement was requested. User observation of both wheels' direction and
physical stopping is pending; serial acknowledgements do not prove wheel motion.
Evidence: ignored `tools/dashboard/.local/straight-raised-20261102-162355.log`
and companion JSON. Integrated firmware remains replaced by this bench image.

## Floor trim observations — 2026-11-02

User confirmed both wheels turn forward during the raised-wheel check.
At 500 ms: 20% left / 20% right curved slightly right; 19% / 20% still curved
right; 18% / 20% looked straight. A subsequent 1000 ms pulse at 18% / 20%
was acknowledged with automatic stop; no physical result was supplied yet.
These are user visual observations, not measured wheel speeds or path errors.

At the user's request, added an explicit 2000 ms option (`3`), preserving the
500 ms startup default, duty bounds, disconnect stop and one-command pulse.
The updated image required the subsequent reflash recorded below.
After flashing, restore and verify 18% left / 20% right before the requested run.

The 2000 ms revision compiled successfully without reported warnings. Updated
UF2 SHA-256: `4AA9DBDE5C5D237AE46AB963877EE932D12186AF5B3A7DDB96346570EC30C9C3`.
This revision was subsequently flashed with explicit approval on 2026-11-02.
The revised banner and settings were verified on COM5: left 18%, right 20%,
2000 ms. Setup sent only help, stop and settings commands; final stop was
acknowledged. No movement was sent during that setup step.

After the user confirmed floor readiness, sent exactly one 2000 ms forward
pulse at left 18% / right 20%. RUN, automatic STOP and final STOP were
acknowledged on COM5. Physical straightness is awaiting the user observation.
Evidence: `tools/dashboard/.local/straight-floor-20261102-163239.log` and JSON.

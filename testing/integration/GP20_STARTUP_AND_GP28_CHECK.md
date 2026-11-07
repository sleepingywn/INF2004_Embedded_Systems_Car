# Initial startup power and nonresponsive GP28 investigation

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Status: **4AB1C1... run captured; search timeout at 9599 ms. GP28
white/black contrast recovered after reconnect; earlier failure cause not isolated.**

User approved flashing after the wheels-lifted/BOOTSEL prompt. Verified the
sole RPI-RP2 volume, Board-ID and artifact SHA256 before copying. Evidence:
`tools/dashboard/.local/gp20-startup-stationary-20261103-225728.{log,json}`.
Startup, gentle and search banners verified. Six snapshots showed GP20
armed/unpressed, no motion faults and both motor outputs zero. GP28 raw
142..165 remained in the recovered white range. Only `h` and six `i` commands
were sent; no motor-start command was issued.

## Startup correction

The previous 9DEF5D... image reduced pause restarts but retained initial
155/185-permille feed-forward. Its last track trace showed speeds 870/1076
counts/s against 350/350 targets at 219 ms. Reducing target speed alone did
not eliminate initial overspeed.

For the search trial only, `line_follow_begin()` now seeds both wheels at
80 permille (8% duty), with the integral initialized to match that output and
the cruise target. Admission still does not write motor outputs. The first
20 ms update is limited to at most 85 permille; subsequent updates retain
ordinary PWM slew and feedback. The shared straight-bench controller and
non-search starts retain their previous behavior. No startup stall exemption,
longer search timeout or threshold change was introduced. Full power remains
limited to 20%, but initial power is no longer preloaded near that cap.

Lower initial duty is a provisional hardware setting; host checks do not
prove loaded breakaway or guarantee no overshoot. Stall protection remains.

## Right-sensor findings

Installed firmware remains 9DEF5D... throughout the read-only investigation.
Only `i` diagnostic commands were sent, with final motor outputs zero.

- Earlier good black: GP28 2509..2538 (`gp20-recal-black-20261103-221932`).
- User-confirmed white now: GP28 42..50, eight samples
  (`gp28-investigate-white-20261103-224519.{log,json}`).
- User-confirmed black now: GP28 33..49, eight samples
  (`gp28-investigate-black-20261103-224754.{log,json}`).

Evidence files are under `tools/dashboard/.local/`; names use the host clock.
These overlapping ranges do not provide usable contrast. Do not replace the
calibration endpoints with them or classify the low values as black.

Source review confirms GP28 is ADC2 / line B; raw FIFO values are stored before
normalization. ADC channels are selected 0, 1, 2 in order, with no competing
sample ADC driver enabled. No production ADC/IR code changed in this fix.
An added host regression feeds values 43, 160 and 2533 through the actual scan
ISR and verifies channel selection and unchanged raw propagation to line B.
It passes, along with acquisition fault/timeout checks. This validates software
logic, not the physical ADC input, wiring, supply or optical hardware.

The user has been asked to remove USB and battery/powerbank power, reseat the
existing right-sensor signal/power/ground connections without changing pins,
then reconnect normally and present black again. If contrast remains absent,
isolate the signal lead/module/input with a controlled powered-off swap or
electrical measurement before another track test. A successful ADC scan does
not prove a connected or functioning sensor.

After the user replied "reconnected", read-only evidence
`gp28-reconnected-20261103-225528.{log,json}` captured GP28 2409..2441 in eight
fresh idle snapshots, with GP27 140..174 and correct rightward line errors
-94..-95. An additional buffered startup sample read 2476 and is excluded from
the fresh range. All fresh motion snapshots were fault-free with outputs zero,
GP20 armed/unpressed, and no acquisition/handoff errors. The black response has
returned. Since reconnect involved both power cycling and a requested connection
check, this does not isolate a loose wire versus another recoverable condition.
Read GP28 over white again before declaring contrast restored. No firmware was
flashed or motor command issued during this recovery check.

Matching post-reconnect white check:
`gp28-reconnected-white-20261103-225626.{log,json}`. All eight GP28 samples
were 141..174, matching the earlier good white range. Together with fresh black
2409..2441, minimum separation is 2235 raw counts. All eight motion snapshots
were fault-free with zero outputs; acquisition and handoff errors stayed zero.
Current contrast is restored without changing the calibration. Recovery does
not prove that the fault cannot recur under vibration or battery operation.

## Validation and candidate artifact

32 host executables passed via `.local/validate_gp20_startup.py`, including new
initial-start tests across turn directions, 20/40/60 ms intervals, no-motion
stall at the original 200 ms deadline, overspeed response, reset/time wrap and
unchanged shared bench initialization. Actual owner fixtures assert initial
motor writes at no more than 85 permille on both wheels. The separately run
IR acquisition executable also passed (33 executables total).

Firmware build: clean, text/data/BSS 396732/184/90076.
Profile SMP1/USB CDC/CYW43/LWIP/MQTT, LINE_DEMO=1, LINE_CIRCUIT=1,
LINE_AUTOSTART=0, LINE_SEARCH=1.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_search.uf2`

SHA256: `4AB1C1A35BF2A183307040B1A5512F2033F4188B6211774C286FBA32F4FAEB99`

Previous installed image backup:
`tools/dashboard/.local/gp20-gentle-installed-9def5d.uf2`.

Flash approval was subsequently received and the artifact above installed.
Startup banner:
`STARTUP PROFILE: both wheels seed at 8% duty with matched integral; normal slew and stall checks apply.`

## Captured run after startup fix

`tools/dashboard/.local/gp20-startup-result-20261103-230251.{log,json}` contains
all 64 retained rows, consistent trace epoch/id 3/2 and identical duplicates.
Read-only `i`/`t` calls; no movement/reset commands. Final outputs zero.
Stop at 9599 ms: IR fault 0x8, step 11 SEARCH_TIMEOUT, fresh LOST/error 0,
age 2 ms. No encoder/stall/acquisition/handoff failure was reported.

The ring begins at 3319 ms, so initial startup speed is no longer retained.
Do not claim the physical startup surge was eliminated based on this trace.
Directional input was present in both directions: negative errors around
3319..3619 and 6019..6119 ms, positive errors throughout much of 4219..7419.
At 5919 ms left paused, and at 6019 ms it restarted with duty 80; its filtered
speed was 293 at 6119 and 340 at 6219, versus the prior run's 716-count/s pause
restart surge. Different trajectories prevent a controlled causal comparison.

Last retained nonzero error +34 at 7419 ms, followed by zero through the end.
Targets were 350/350 during grace, search phase 1 visible at 8119, phase 2 at
8619, and timeout at 9599. GP28 after stop read 228..254, not the previous
near-40 failure level. User observation/video is needed to locate the loss on
the physical track and judge initial startup smoothness. Evidence is preserved;
no additional firmware change or flash accompanied this capture.

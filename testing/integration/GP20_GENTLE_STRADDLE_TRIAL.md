# Gentle tracking with the measured straddling sensor layout

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Follow-up: [GP20_STARTUP_AND_GP28_CHECK.md](GP20_STARTUP_AND_GP28_CHECK.md)
records the initial startup correction and failed right-sensor contrast check.
That candidate is not flashed; this document describes installed 9DEF5D....

Status: **9DEF5D... track-tested; early search timeout at 2159 ms. Do not
describe the gentle trial as a successful tracking fix.**

User confirmed flash readiness with wheels lifted. Verified the sole RPI-RP2
volume, Board-ID and artifact SHA256 before copying. Evidence:
`tools/dashboard/.local/gp20-gentle-stationary-20261103-223642.{log,json}`.
The GENTLE PROFILE banner was confirmed. All six snapshots showed GP20
released/armed, zero motor outputs, no motion faults, and armed LED mode 1.
IR samples advanced without acquisition or handoff errors. Only `h` and six
`i` commands were sent; no motor-start command was issued.

The user approved testing this approach after stationary placement checks.
The boards already touch. Using freshly measured endpoints, the centered
position reads left 2..4%, right 5..6%; a user-estimated 2 mm left shift reads
left approximately 0..1%, right 92..93%; the opposite shift reads left 86..88%,
right 0..1%. These are approximate manually positioned samples, not a measured
transfer curve. They support OUTSIDE/straddling placement; forcing a 50/50
calibration or adopting both-black-as-centered is not justified.

## Calibration evidence

Eight samples each at operating height, in `tools/dashboard/.local/`:

- `gp20-recal-white-20261103-221840.{log,json}`: GP27 152..185, GP28 142..176.
- `gp20-recal-black-20261103-221932.{log,json}`: GP27 2607..2638, GP28 2509..2538.
- `gp20-photo-placement-20261103-222732.{log,json}`: centered photo placement.
- `gp20-placement-left-shift-20261103-222903.{log,json}`.
- `gp20-placement-right-shift-20261103-223034.{log,json}`.

Evidence names use the host clock. New median white/black references are
GP27 173/2623 and GP28 160/2533. GP26 remains 183/2985 because the barcode
sensor was not included in the user-confirmed black placement. Saved line
references affect battery circuit builds; no live calibration command was sent.
Thresholds remain 15% white and 75% black. Proportional steering still uses
intermediate readings; no arbitrary 80% threshold is added.

## Trial behavior

Changes to movement below apply only with `MOTION_LINE_SEARCH=1`:

- Cruise target 450 counts/s, full rolling curve 250 inside / 550 outside.
  Full correction is spread across error magnitude 0..100 instead of reaching
  maximum at 60. The loaded 250 inner minimum is retained.
- Target ramp reduced from 4000 to 2000 counts/s per second. Existing PWM
  slew, line PI gains and 20% duty maximum remain.
- Up to 600 ms without magnitude >=20 gives slow-forward targets 350/350,
  without synthetic steering memory during that interval. This also applies
  to a fresh LINE_LOST/zero-error input at admission. Actual target changes
  obey the ramp; 350 is not an immediate measured-speed guarantee.
- If a tight-turn pause encounters ambiguous input, retain it for up to
  100 ms before release into the remaining grace interval. Directional
  evidence cancels the ambiguity timer. The existing 800 ms tight-turn
  deadline can still stop sooner and is not extended.
- After grace, retain the two search arcs (first 500 ms toward last seen
  side, then opposite). Each search stops by 1500 ms or 1000 counts on either
  wheel without confirmed reacquisition. Grace plus a complete unsuccessful
  search is at most about 2.1 seconds plus the control sampling interval.
- A wheel restarting from a deliberate pause uses its last rolling output
  clamped to 80..110 permille as a seed, rather than jumping to 155/185.
  Its PI integral is initialized to match that seed, then normal feedback
  and slew apply. This is provisional breakaway tuning; a loaded wheel may
  still stall. The 200 ms stall protection is retained without exemptions.

The one-count pause-settling allowance remains. Freshness, epoch, invalid
encoder edges, powered-wheel reversal, speed/range checks, external faults,
both-black junction stop, GP20/console STOP and 30-second maximum remain.
No S3 detector, wire-format, encoder ISR, shared PI function or Buddy 4 changes.
Both-white remains ambiguous: this trial cannot guarantee recovery or eliminate
weaving on every curve.

## Physical result: early straight-section stop

User video `C:/Users/joell/Downloads/Telegram Desktop/video_2026-11-03_22-41-43.mp4`
is about 4.48 seconds. Sampled frames show a short advance on the straight and
a stop, before completing a bend. User expressed fatigue with repeated tests.

Read-only evidence `tools/dashboard/.local/gp20-gentle-result-20261103-224239.{log,json}`
contains all 23 trace rows (epoch/id 3/2, no conflicting duplicates).
Stop: fault IR 0x8, SEARCH_TIMEOUT step 11, elapsed 2159 ms, input LOST/error 0,
age 3 ms, final motor outputs zero. No encoder, stall or handoff error reported.
The first sample at 19 ms had error +21; every subsequent retained sample had
zero error. Slow-forward grace was followed by both search arcs without
reacquisition, so the stop was expected from the programmed ambiguity limit.

The trial softened pause restarts but left initial startup at the original
155/185 feed-forward. That was a material omission for the slower target:
at 219 ms measured speeds were 870/1076 against 350/350 targets. No tight pause
occurred in this run; the new pause-restart behavior was not exercised. Startup
overspeed may contribute to trajectory error; causation is not proven by this
trace. Do not claim that reducing target speed alone reduced startup speed.

Post-stop GP28 raw was 38..43, below the prior white range 142..176, while GP27
was 276..281 and barcode GP26 2872..2893. Acquisition remained healthy. These
stationary values do not prove the surface each sensor saw during movement,
but the GP28 change warrants checking the input/mounting against saved black
and white evidence before relying on thresholds. Do not recalibrate low
readings as black or increase search timeout to conceal lack of line evidence.

No further firmware edits or flash were performed during result review.
Next investigation priorities: initial startup output, anomalous GP28 level,
and the unresolved centered-white versus off-line ambiguity. Avoid another
track iteration based only on changed timeouts.

## Validation and flash handoff

All **31 host executables passed** using
`tools/dashboard/.local/validate_gp20_gentle_tracking.py`; output retained in
`gp20-gentle-tracking-validation.log`. Includes actual-owner search/STOP/fault
cases, fresh-reference normalization, calibrated input logic, both-wheel
one-count protection, grace/debounce, restart cap/slew, restart stall, and
unchanged non-search profile checks. No hardware movement was commanded.

Firmware builds without compiler warnings/errors. Text/data/BSS:
396584/184/90076. Profile: SMP1, USB CDC, CYW43/LWIP/MQTT,
LINE_DEMO=1, LINE_CIRCUIT=1, LINE_AUTOSTART=0, LINE_SEARCH=1.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_search.uf2`

SHA256: `9DEF5D52005BF255D5DEDDDB4DA2C07C963A560596FFBC0C9B08F7200CBE1C63`

Previous installed image backup: `.local/gp20-settling-installed-104eff.uf2`
under `tools/dashboard/`. User must confirm flash readiness before install.
After flashing, stationary diagnostics should show the GENTLE PROFILE banner,
GP20 armed, zero motor outputs and healthy acquisition. Then the user can
center the robot and start a GP20 track run. Preserve power afterward and read
`i` / `t` before RESET/BOOTSEL. Compare paused-wheel restart speed and curve
exit behavior; do not claim physical success from host tests.

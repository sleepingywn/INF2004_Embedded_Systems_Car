# Barcode black/white thresholds: how they were set, and how to redo them

Handoff for whoever re-tunes the barcode sensor after the light hood is fitted
(24 Nov 2026). Base branch: `CLOSE-TO-FINISH-2`.

## Scope

- Only the **barcode sensor** (left IR module, GP26) in the **analogue**
  build (`BARCODE_DIGITAL=0`, the default) is covered here.
- The line sensors (GP27/GP28) use saved white/black calibration endpoints,
  not these cutoffs. If the hood also covers them, recalibrate them separately
  (`config/line_boot_calibration.h`, console `c` / `f` / `i`).
- In the digital build (`BARCODE_DIGITAL=1`) GP26 reads the module's D0 pin.
  The threshold is then the potentiometer on the module, and none of the
  numbers below are used (`ir_acquisition.c:124`).

## Where the thresholds are

All in `app_program/subsystem3/barcode_level.h`, used by `bc_level_black()`,
which `ir_runtime.c:260` calls on every 1 ms raw sample (`raw[IR_CH_BARCODE]`,
12-bit ADC, higher = darker).

| Constant | Value | Meaning |
|---|---|---|
| `BC_LVL_WHITE_MAX` | 200 | a reading at or below this is always white |
| `BC_LVL_BLACK_MIN` | 240 | a bar can only start at or above this |
| `BC_LVL_RISE` | 60 | a bar must rise at least this much above the lowest white before it; a bar end needs a drop of at least this much below the bar's peak |
| `BC_LVL_REF_CAP` | 300 | the "white level" used for the half-way rule is never taken above this |
| `BC_LVL_HOLD` | 8 ms | the half-way drop must last this long to end a bar |
| `BC_LVL_END_NUM/DEN` | 1/2 | a bar ends when the reading falls half way from its peak back to the white level before it |

There are two rules:
- **Fixed cutoffs** (Ken's original 200/240).
- **A relative rule** (18 Nov), added because the hump knocks the sensor out
  of position. A bar also ends half way down from its own peak, so a blurred
  thin gap between two strong bars still counts as a gap.

## How the values were found last time (18 Nov 2026)

### 1. Measure raw levels with the car still

Use the USB console in the route build. Press `p` if needed so the status line
prints, and read `raw bc NNNN` in the `[con] IR ... raw bc` line
(`int_console.c:141`). Put the sensor over each spot and note the value:

| Spot | Raw (18 Nov, no hood) |
|---|---|
| white floor/paper | ~177 |
| wide black bar | ~1309 (up to ~3800 when the sensor bobs) |
| narrow black bar | ~307 (270 to 3100 along one barcode) |
| thin white gap between bars | ~177 |

The narrow bars only reach about 300 because the sensor's spot is wider than a
narrow element. That is why the fixed cutoffs sit so close to white.

### 2. Derive the constants from those levels

These rules reproduce the current numbers. Apply them to the new measurements:

| Constant | Rule | 18 Nov |
|---|---|---|
| `BC_LVL_WHITE_MAX` | white + noise margin (about 25) | 177 + 23 = 200 |
| `BC_LVL_BLACK_MIN` | midway between white and a narrow bar | (177 + 307) / 2 = 242 → 240 |
| `BC_LVL_RISE` | about half of the narrow-bar contrast | (307 − 177) / 2 = 65 → 60 |
| `BC_LVL_REF_CAP` | just below a narrow bar | 300 |
| `BC_LVL_HOLD` | longer than a print flicker (6 ms seen), shorter than a real gap (14 ms or more at track speed) | 8 ms |

Keep the order WHITE_MAX < BLACK_MIN < REF_CAP < narrow-bar level. If the hood
makes white darker or narrow bars stronger, the gap between them grows and
the cutoffs get easier, not harder.

### 3. Check on the track with `levels_raw`

After every scan the route build prints:

```
[route] SCAN elements=29 decode: distance=Z time=Z -> Z
[route] widths_ms: ...
[route] widths_counts: ...
[route] levels_raw: B244 S171 B1357 ...   <- darkest raw of each bar / lightest raw of each gap
```

(`line_console.c:80`.) This line is how every change on 18 Nov was found:
- a gap whose lightest value stays high (for example `S558` when other gaps
  are ~180) was blurred and nearly merged two bars;
- a bar whose darkest value is near `BC_LVL_BLACK_MIN` was nearly missed.

### 4. What was tried, in order (builds KR2b to KR2f, 18 Nov)

| Build | Change | Result |
|---|---|---|
| KR2 | fixed 200/240 only | bars merged, no decode |
| KR2b | bar ends half way from its own peak | Z 4/4, A failed (6 ms flicker split a narrow bar) |
| KR2c | half-way drop must last 8 ms | Z 3/4 |
| KR2d | end at 3/4 of the way instead of 1/2 | Z 1/3, **reverted** (missed the gap between Z's two wide bars) |
| KR2e | added the `levels_raw` print | Z 2/4, then 2/2 |
| KR2f | a new bar after a gap that never got back near white needs a rise of 1/4 of the previous bar's height; one-marker decode only when time and distance widths agree | Z decoded, left turn and exit |

Logs: `testing/subsystem3/logs/2026-11-18_KR2b..KR2e_*.txt` (the KR2f track
log was not committed). The full write-up is
`testing/integration/BARCODE_DISPLACED_SENSOR.md` on branch
`feature/barcode-displaced-sensor`. It was not carried into
CLOSE-TO-FINISH-2, but the code was (`barcode_level.h` is identical).

## Laptop checks to run after any change

```sh
cd testing/subsystem3
gcc -O2 -Wall -DBARCODE_SLOW_SCAN -DBARCODE_WIDE -I../../app_program/subsystem3 \
    -o barcode_level_test barcode_level_test.c -lm && ./barcode_level_test
gcc -O2 -Wall -DBARCODE_SLOW_SCAN -DBARCODE_WIDE -I../../app_program/subsystem3 \
    -o barcode_replay_test barcode_replay_test.c && ./barcode_replay_test
```

(Both are also run by `python testing/subsystem2/run_route_tests.py`.)

- `barcode_level_test.c` uses a sensor model fitted to the 18 Nov levels:
  `raw = 177 + 1323 * f^4.5`, where f is the black fraction under the spot.
  If the hood changes white or the bar levels, refit the 177 and 1323 to the
  new measurements. Otherwise the test checks the old sensor, not the new one.
  It asserts: never a wrong letter, the new rule beats the fixed cutoffs, and
  at least 95% of Z decodes at realistic spot sizes. It also covers the track
  cases: narrow bar, blurred gap, 6 ms flicker, 30 ms shallow sag, gap bump.
- `barcode_replay_test.c` replays all 21 scans logged on the car (8–18 Nov)
  as widths: 14 strict decodes, 2 one-marker decodes, 0 wrong letters. Widths
  don't depend on the raw levels, so it stays valid; it guards the decoder.

## Suggested procedure with the hood

1. Fit the hood. Car still. Read `raw bc` over white, a wide bar, a narrow
   bar and a thin gap, in the room light used for the demo.
2. Recompute the five constants with the rules in step 2. Change only
   `barcode_level.h`.
3. Refit the model in `barcode_level_test.c` and run both laptop checks.
4. Build the route profile (`LINE_ROUTE=1`, `BARCODE_DIGITAL=0`), run each
   barcode used (A, B, C, D, Z) a few times, and save the `[route]` lines.
   Look at `levels_raw` for gaps that stay high or bars that stay low.
5. Record the measurements, the new constants and the results in
   `testing/subsystem3/TEST_LOG.md`, with the log file names.

If the measured levels barely change with the hood, leave the constants as
they are. The relative rule already adapts to each bar's own peak.

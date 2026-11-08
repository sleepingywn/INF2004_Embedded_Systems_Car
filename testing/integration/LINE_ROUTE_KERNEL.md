# LINE_ROUTE: barcode routing on top of V5 (kernel)

Branch `local/kernel-v5-barcode-route`, based on Joel's V5 (`d3a939a`).
Kernel port of the standalone bench build that worked on the track on
8 Nov 2026 (`testing/subsystem3/ir_line_bench`, branch
`local/v5-barcode-turning`, W3). Not yet tested on the car.

## What it does

1. **Start:** GP20 starts V5 line following, exactly as in the plain V5 build.
2. **Barcode:** the barcode sensor (GP26) is read while V5 drives. A full
   29-element Code 39 capture is decoded. Every capture of 9 or more elements
   is logged.
3. **Arming:** a decoded letter arms ONE manoeuvre for the next junction:

   | Letter | Manoeuvre |
   |---|---|
   | A | right arc |
   | B or Z | left arc |
   | C | straight 100 mm |
   | D | 180° U-turn |

   Other letters are logged but arm nothing. A newer barcode replaces an
   armed one.
4. **Junction:** both line sensors at 75 or more for 20 ms. If nothing is
   armed, V5 crosses straight as usual.
5. **Arc:** outer wheel 750 counts/s, inner wheel 200 counts/s.
6. **Exit (Ken's rule):** the branch is found when, after at least 150
   counts of turn, the turn-side sensor reads 35 or more and is at least 10
   darker than the other sensor. After 300 counts, either sensor counts.
   V5 then takes over and keeps the arc's motor power (bench W1 fix).
7. **C / D:** the car stops for 300 ms, drives the leg, stops for 300 ms,
   then V5 restarts.

## What changed (files)

| File | Change |
|---|---|
| `subsystem3/ir_types.h`, `barcode_sense.c/.h` | Ken's 16-bit widths and 1.5 s quiet time. Allowed in the kernel only with `BARCODE_WIDE` (set by `LINE_ROUTE=1`). |
| `subsystem3/ir_runtime.c` | With `LINE_ROUTE` only: barcode black/white from Ken's raw ADC cutoffs (black ≥ 240, white ≤ 200, with hysteresis). The line sensors are unchanged. |
| `subsystem2/motion_logic.c/.h` | Ken's `bc_width_t` and Z support in the old decoder. Used only for comparison in the log. |
| `subsystem2/motion_route_decode.h` | A–Z decoder that classifies bars and spaces separately. Unchanged from bench W3. |
| `subsystem2/motion_route.h` | Route logic: odometry history, distance widths, Ken's exit rule. |
| `subsystem2/motion.c/.h` | Route state machine inside the LINE_TRACE control step, odometry recording, barcode arming, and an event log. |
| `subsystem2/line_console.c` | Prints `[route]` lines from the console task, never from the motor loop. |
| `build_make/pico_rp2040.mk` | `LINE_ROUTE=1` profile. It requires `LINE_ADAPTIVE=1`. |

With `LINE_ROUTE=0` (default) the build is Joel's V5. All 38 regression
tests and the 8 adaptive tests pass unchanged.

## Decoding: distance vs time

The S3 task samples at 1 kHz (unchanged), so time widths are in ms.

- **Distance widths:** the motion task keeps 10 s of odometry at 20 ms steps
  (left + right forward counts). Each bar's start and end times are converted
  to encoder counts by interpolation.
- **Which result is used:** distance first; time if distance does not
  decode. If the two give different letters, nothing is armed.
- **Sampling rate:** the bench used 333 µs sampling. The kernel keeps 1 kHz,
  because the IR acquisition, queue-age and gap checks all assume 1 ms
  frames. Narrow bars measured 40–100 ms on the track, so 1 ms resolution is
  enough.

## Build

```sh
make -C build_make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 \
  LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1 LINE_ROUTE=1
```

This is the same profile as the K1 V5 image. The output is
`build_make/mtk3pico_smp1_usb_cdc_wifi_line30_button_adaptive_route.uf2`.

## Host tests

```sh
python testing/subsystem2/run_route_tests.py
```

This runs:

- route logic tests;
- 7 owner-loop scenarios on the real `motion.c`: A, Z, C, D, an unarmed
  junction, letter E, and exit not found;
- the route-profile IR handoff test;
- the profile guard;
- Joel's adaptive and regression suites.

## Car test (first time)

1. **Setup:** flash the route image. Use the same mat, barcode height and
   lighting as the working bench layout. Barcodes go on the car's left, off
   the black line, on a straight, with clean white after the last bar.
2. **Barcode check:** run past one barcode with nothing after it. The log
   should show `[route] SCAN elements=29 decode: distance=X time=X -> X`.
   This checks the kernel reads like the bench.
3. **Full run:** the layout that worked on the bench, A then Z. Expect:

   ```
   [route] ROUTE: A armed ... → [route] JUNCTION: A -> right arc → [route] RIGHT EXIT ...
   [route] ROUTE: Z armed ... → [route] JUNCTION: Z -> left arc → [route] LEFT EXIT ...
   ```

4. **Failure codes:** `[route] FAILED ... reason=`
   - 16: exit not found within 6 s;
   - 13: leg timeout;
   - 4: stall;
   - 3: encoder/control;
   - 18: line lost or stale during the arc.

   V5 failures are reported the usual way (`i` for diagnostics).

## Known limits

- **30 s limit:** the whole GP20 run is still bounded to 30 s by the motion
  request. The bench restarted the 30 s at each handover.
- **Early handover:** the exit rule is Ken's original and can hand over
  early on a crossing. It was seen once on the bench.
- **Mapping:** Z = left is a test mapping. Letters E–Y arm nothing.

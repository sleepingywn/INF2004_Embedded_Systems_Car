# S3 line/barcode logic bench test

Standalone Pico SDK program that runs the **real** S3 logic from
`app_program/subsystem3/` (`sensor_line.c`, `barcode_sense.c`) outside the
micro T-Kernel image:

1. **Unit tests S3-U05..S3-U11** with synthetic readings, printed as
   `[TEST] <ID> PASS|FAIL` (press `t` to re-run).
2. **Live mode:** 1 kHz ADC sampling (repeating timer → lock-free ring), a
   10 ms loop that feeds every sample to the barcode capture and the
   averaged line readings to `line_update()`. Prints CSV
   `t_ms,nBC,nA,nB,err,state,bc_active` at 20 Hz and one `BARCODE ...` line
   per captured barcode.

Wiring is the same as `../ir_bench_test/README.md`.

## Build and flash

Open this folder with the Raspberry Pi Pico VS Code extension (*Import
Project*, board **Pico W**, *Compile Project*), or with the SDK on your PATH:

```sh
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

Copy `build/ir_line_bench.uf2` to the `RPI-RP2` drive (hold BOOTSEL while
plugging in USB). The program waits up to 30 s for a serial monitor, runs
the unit tests, then calibrates.

## Using live mode

1. During the 3 s **CALIBRATING** window, slide the car sideways so both line
   sensors cross the black line, and so the barcode sensor crosses a bar.
   Any channel with less than 600 counts of contrast is reported as
   `LOW CONTRAST -> IR FAULT` and reads 0. Press `c` to calibrate again.
2. Line: `err` is −100 (line far right) … 0 (centred) … +100 (line far
   left). `state` is ON, EDGE, LOST_HOLD, LOST, BOTH_BLACK or FAULT.
   In OUTSIDE mode (the default) the sensors sit on white either side of
   the line, so A ≈ B ≈ 0 with `state=ON` is the normal centred reading.
   Build with `-DLINE_MODE=0` for the EDGE-mode variant.
3. Barcode: push the car across a barcode at a steady speed. The capture
   prints the width of each bar/space in ms, bar first.

## How bar widths are measured without a speed sensor

The sensor is sampled at a fixed 1 kHz, so counting consecutive black (or
white) samples gives how long the sensor spent over that bar (or space).
At constant speed that time is proportional to the printed width. Code 39
is read from the **ratio** of wide to narrow elements (≈ 3 : 1), so the
absolute speed cancels out. The car only has to keep a roughly constant
speed while crossing the barcode (≈ 0.5 s at 0.3 m/s), which is why
`line_msg_t.bc_active` asks the Vehicle Controller to hold its speed.

If testing shows the speed is not steady enough, the fallback raised in
Tribunal 1 Version A is to count widths in encoder distance instead of time.
That needs Buddy 2's wheel-distance data and is not implemented yet.

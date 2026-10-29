# S3 IR sensor bench test

A standalone Pico SDK program (not part of the micro T-Kernel image) that
checks the three IR modules can tell the white background from the black
line before the real S3 driver is written. It is not compiled by
`build_make/` — the team build only compiles `app_program/`.

## Wiring (analog mode, D0 unconnected)

| Sensor | Role | A0 → | VCC | GND |
|---|---|---|---|---|
| IR Left | Barcode | GP26 / ADC0 | 3V3 | GND |
| IR Centre | Line sensor A (left-hand) | GP27 / ADC1 | 3V3 | GND |
| IR Right | Line sensor B (right-hand) | GP28 / ADC2 | 3V3 | GND |

The Robo Pico GP28 VBAT-sense jumper must be **open**, otherwise ADC2 reads
the battery divider.

## Build and flash

Either open this folder with the Raspberry Pi Pico VS Code extension
(*Import Project*, board **Pico W**, then *Compile Project*), or:

```sh
export PICO_SDK_PATH=~/pico/pico-sdk
cmake -S . -B build -DPICO_BOARD=pico_w
cmake --build build
```

Hold BOOTSEL while plugging in USB and copy `build/ir_test.uf2` to `RPI-RP2`.

## Reading the output

USB serial, any baud (VS Code Serial Monitor, PuTTY, `screen`, or on
Windows `powershell -ExecutionPolicy Bypass -File monitor.ps1 -Port COM3`).

Each line shows, per channel, the averaged raw 12-bit value, the voltage,
the min/max seen so far, and WHITE/BLACK once the channel has seen at least
600 counts of contrast.

1. Put the car **flat on the white surface** (not lifted: in the air a
   sensor reads like black) and press `r`. The first second after a reset
   is taken as the white reference.
2. Slide the car so each sensor crosses the black line / a barcode bar.
3. Save the output to `../logs/` and record the result in `../TEST_LOG.md`.

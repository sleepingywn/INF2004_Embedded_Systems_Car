# Subsystem 3 – IR line tracing and barcode sensing (Buddy 3)

The original algorithm files remain hardware-independent and are used by
`testing/subsystem3/ir_line_bench`. The integration branch now adds
`ir_acquisition.c` and `ir_runtime.c`: a hardware-paced ADC sampler and an S3
task that calibrates the sensors and calls Buddy 2's `motion_ir_bridge_sample()`.
That bridge reuses the logic below unchanged, publishes line state every 10 ms
and sends completed barcode widths to B2 for decoding.

See [runtime ownership, calibration and recovery](README_RUNTIME.md) and
`../subsystem2/README_MOTION.md`. **The new runtime is not built, tested or
flashed.** Older bench results validate only the code/configurations recorded
in those results; the GP28 wiring failure remains unresolved evidence.

| File | Feature |
|---|---|
| `ir_types.h` | Pins (GP26 barcode, GP27 line A, GP28 line B), thresholds, and the minimum-bit messages `line_msg_t` (32 bits) and `bc_msg_t` (16-bit header + widths) |
| `sensor_line.c/.h` | Calibration (min/max, rejects a sensor with < 600 counts contrast), raw → 0 white … 100 black, proportional line error and line state |
| `barcode_sense.c/.h` | Barcode capture: width of each bar/space in ms, sent as soon as the 29th element ends or after 150 ms of white; blips < 5 elements dropped |

**Line error** (`LINE_MODE` in `ir_types.h`, currently **OUTSIDE**):

- *OUTSIDE* (selected – the sensor eyes cannot be placed closer than the
  line width): both sensors sit on white, one each side of the line.
  `e = A − B`; both white = centred (err 0, ON); LOST only after 2 s with
  neither sensor seeing the line.
- *EDGE* (kept for if the sensors are remounted): each sensor over one
  edge. `e = 100 × (A − B) / (A + B)`; both white = line lost after 200 ms.

Both modes: −100 = line far right, 0 = centred, +100 = line far left.
States: ON, EDGE_ONLY (a sensor fully black), LOST_HOLD (edge mode only),
LOST, BOTH_BLACK (junction), FAULT.

**Barcode:** only measured here. Narrow/wide classification and decoding
are done by Buddy 2 (Tribunal 1 decision). Widths are timed at 1 kHz, so
the car must keep a steady speed while crossing; Code 39 uses the
wide/narrow ratio, so the actual speed does not need to be known.

All thresholds are TBC and must be tuned on the real track.


## Digital barcode trial (12 November 2026)

The kernel now has an optional `BARCODE_DIGITAL=1` build mode. This is a new
adaptation of the existing capture/decoder, not a merge from a digital branch.
The remote branches checked on this date still use analogue ADC barcode input.

Wire the barcode module **D0 to GP26**, replacing its A0 connection there. Keep
line sensor A0 outputs on GP27 and GP28. Use a 3.3 V-compatible D0 signal and
common ground; do not drive the Pico input with 5 V. The sensor's potentiometer
sets the digital threshold. Confirm its output changes between paper and black.
`BARCODE_BLACK_LEVEL=1` means HIGH is black; use `=0` if the module is inverted.

D0 is sampled from GPIO at the existing 1 kHz deadline. ADC0 is skipped; ADC1/2
still sample the two line sensors. The internal barcode raw diagnostic is now a
synthetic 0 (white) or 4095 (black), not an analogue voltage reading. D0 bypasses
software analogue thresholds/calibration; line calibration is unchanged. The
existing run-width capture and Code39 decoder, route mapping, and STOP controls
are retained. This mode does not add edge debounce or change the sampling rate.

Build the existing remote/route profile with `BARCODE_DIGITAL=1 BARCODE_BLACK_LEVEL=1`.
Its filename ends `_remote_bc_digital1.uf2`, and USB `h` prints the digital mode,
pin and polarity. The default remains `BARCODE_DIGITAL=0` for existing A0 wiring.
Both options participate in build-cache invalidation, so objects are rebuilt
when switching input mode or polarity. The digital no-time-limit / GP20-retry image was flashed on 12 November 2026; stationary checks passed. Barcode reliability still needs physical tuning.

The dashboard shows `X / 29 elements (last capture)`. This counts completed
bars/spaces including the two character gaps for `*X*`; it is not a live progress
counter. Captures finish at 29 elements or after the white timeout. Captures of
5–8 elements are now reported as failed scans too; fewer than 5 remain filtered
as noise. A full count does not guarantee a valid decode.

Host test: compile `testing/console/ir_acquisition_test.c` with the existing
console mock include paths and `-DIR_BARCODE_DIGITAL=1`, for both values of
`-DIR_BARCODE_BLACK_LEVEL`. It exercises GPIO/ADC channel ordering, the actual
IRQ-to-capture-to-Code39 path, timing/fault handling, and unchanged line samples.


### Saved digital branch

`feature/digital-barcode` preserves the full current digital-capable source,
including dashboard mappings, telemetry, GP20 retry and optional no-time-limit
runs. Build the tested digital profile with:

```sh
make -C build_make -j8 PICO_SDK_PATH=C:/Users/joell/.pico-sdk/sdk/2.3.1 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1 LINE_ROUTE=1 LINE_REMOTE=1 BARCODE_DIGITAL=1 BARCODE_BLACK_LEVEL=1 LINE_UNLIMITED=1
```

Latest flashed UF2 SHA256:
`E34E6EA7C8A55B5C92E6BE446FA7C5D68D58F32CC2589220196AD577DD65BD96`.
Build products and credentials stay local and are not part of the Git snapshot.


## Digital interrupt capture update (12 November 2026)

Local branch `feature/digital-barcode` now captures GP26 D0 edges through the
existing B2 GPIO ISR, rather than using 1 kHz samples for barcode widths. The
1 kHz ADC acquisition of GP27/28 is unchanged. The analogue rollback remains at
`feature/robocar-telemetry-line-follow-autostart`, commit `2b9fc38`.

- ISR records raw microsecond time, signed-corrected summed wheel position and
  encoder-error counter into a 128-entry single-producer/single-consumer ring.
  Both-edge ambiguity and ring overflow reject the capture. No decode, printing
  or kernel calls run in the barcode ISR path.
- S3 drains at most 128 edges per sample. A transition must survive 2 ms; the
  accepted transition retains its original timestamp and position. Pulses
  shorter than 2 ms are removed. This threshold requires real-sensor tuning.
- Widths use actual encoder counts at accepted edges, with no invented sub-count
  precision. Every element must have at least two forward counts and no encoder
  errors for distance decoding; otherwise timing is available for hand swipes
  and insufficient wheel movement. Timing widths are rounded to milliseconds.
- Completion requires trailing white. With usable distance, quiet travel must
  exceed both 80 summed wheel counts and ten times the narrowest captured element.
  These are count thresholds, NOT calibrated millimetres. Otherwise white for
  1.5 s finishes the capture. Three seconds without edges or wheel movement ends
  a stalled capture; a black stall is rejected. Reaching 29 alone does not finish.
- Digital decoding validates both bars AND spaces in both `*` markers. Existing
  A-Z mapping and time/distance agreement rules remain. Failed scans cannot arm
  a junction. Epoch changes or invalid/gapped IR frames discard pending edges.
- Existing dashboard letter and X/29 telemetry remain wire-compatible. USB route
  logs additionally show removed glitches, dropped edges, rejection reason,
  time widths and distance widths. Rejection codes: 1 lost/ambiguous edges,
  2 stalled on black, 3 incomplete capture. A 29-element scan without a letter
  indicates invalid widths/markers or conflicting decodes. These detailed
  rejection fields are USB-only; the dashboard still shows 'Not decoded'.

Build with the digital command above (`BARCODE_DIGITAL=1 BARCODE_BLACK_LEVEL=1`).
Run `python testing/subsystem2/run_digital_edges_tests.py` with GCC on PATH.
The tests cover both scan directions, changing speed, hand swipes, timer wrap,
chatter, delayed quiet-zone completion, overflow, ambiguous IRQ edges, stalls,
partial captures, reset fencing and the actual owner/telemetry snapshot path.

This update is software-tested only. The robot still has the analogue image
flashed earlier today. Before a digital flash, move the barcode output from A0
to D0 on GP26 and verify black polarity; GP27/28 stay on A0. A clean stationary
white/black test and repeat scans are needed before claiming improved reliability.

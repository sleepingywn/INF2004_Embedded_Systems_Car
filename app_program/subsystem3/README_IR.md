# Subsystem 3 – IR line tracing and barcode sensing (Buddy 3)

Logic only: no kernel or hardware calls, so it is tested on the Pico with
`testing/subsystem3/ir_line_bench`. The S3 task that samples the ADC and
sends the messages to the Vehicle Controller is not written yet (the
interface with Buddy 2 still has to be agreed).

| File | Feature |
|---|---|
| `ir_types.h` | Pins (GP26 barcode, GP27 line A, GP28 line B), thresholds, and the minimum-bit messages `line_msg_t` (32 bits) and `bc_msg_t` (16-bit header + widths) |
| `sensor_line.c/.h` | Calibration (min/max, rejects a sensor with < 600 counts contrast), raw → 0 white … 100 black, proportional line error and line state |
| `barcode_sense.c/.h` | Barcode capture: width of each bar/space in ms, sent as soon as the 29th element ends or after 150 ms of white; blips < 5 elements dropped |

**Line error:** `e = 100 × (A − B) / (A + B)`, −100 = line far right,
0 = centred, +100 = line far left. States: ON, EDGE_ONLY, LOST_HOLD,
LOST (both white ≥ 200 ms), BOTH_BLACK (junction), FAULT.
Assumes *edge* mounting (each sensor over one edge of the line). With the
sensors outside the line, "both white" is normal and would show LOST; an
outside mode is not implemented yet.

**Barcode:** only measured here. Narrow/wide classification and decoding
are done by Buddy 2 (Tribunal 1 decision). Widths are timed at 1 kHz, so
the car must keep a steady speed while crossing; Code 39 uses the
wide/narrow ratio, so the actual speed does not need to be known.

All thresholds are TBC and must be tuned on the real track.

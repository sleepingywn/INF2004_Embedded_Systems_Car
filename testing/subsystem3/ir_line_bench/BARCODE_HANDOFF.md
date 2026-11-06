# Barcode and motion handoff

Use this file as the starting point for future sessions. It summarizes the project documents, the current test firmware, and the decisions made while testing the car. Read the underlying sources when a requirement is disputed or when implementing a new integration.

## Current work and next decision

- Current local trial is `feature/barcode-a-right-turn`. After a two-second startup delay, the car drives straight for up to eight seconds looking for a junction. GP20 cancels a run and can start another pass.
- Both calibrated line sensors must reach **75/100** and keep `LINE_BOTH_BLACK` for **20 ms**. That selects **A → trace the right branch** once. After 100 ms of braking, both wheels move forward with targets of 750 left and 200 right counts/s. The first motor output is biased towards the right arc. After about 10° of encoder-inferred rightward heading change, the right sensor must see an edge for 30 ms outside the junction. The existing proportional steering helper then follows the outgoing line for up to eight seconds. The trial also implements **C → straight for 100 mm** and **D → 180° U-turn** for future selection. All distances and angles are encoder targets that need verification on the car.
- Decoded letters are logged. They do not select the junction manoeuvre in this trial. A full capture can finish before the junction without stopping the approach. GP20 scan mode still retries incomplete captures of 5–28 elements.
- Future routing can store the decoded command, keep moving towards the junction, then execute and clear the pending command once. Dynamic assignment is planned later.

## What the project documents establish

| Source | Relevant decision |
|---|---|
| `SE08_ImmTri1.docx`, Version B decision log | Buddy 3 sends sensor readings and Buddy 2 handles decoding and navigation in the vehicle controller. |
| `SE08_PostTri1.docx`, Version C findings C01 and C12 | Buddy 3 owns the calibrated and timestamped IR input. GP26 is dedicated to barcode sensing, GP27 is left line and GP28 is right line. Buddy 2 owns decoding, navigation and normal motor output. |
| `SE08_PostTri1.docx`, Version C C06 | Four navigation instructions plus “no instruction” require 3 bits. Values 5–7 are invalid. Printed letters are barcode identities and the agreed route instruction is a separate representation. |
| `Change Requirement 1.docx` | Minimize the bit width of firmware application data and use safe access for shared packed values. Keep a field/range/bit-width table and provide RAM, message-size, correctness and timing evidence for the final submission. |
| [Buddy 3 motion and sensing notes](../../../../project/Buddy3_IR_LineTracing_Barcode.md) | In the local project folder, covers sensor roles, wiring, capture and Buddy 2 handoff. |
| [Motion integration notes](../../../app_program/subsystem2/README_MOTION.md) | Positive turn angles are left and negative angles are right. A right-angle turn is **−90°** in the integrated controller. The provisional project mapping is **A left, B right, C straight, D U-turn**. The current local trial intentionally uses **A to trace the right branch** at the user's direction. The notes refer to the Project Briefing, Project Write-up, Barcode & Line Specification and Barcode Sample. |
| [Adaptive controller crossing notes](../../integration/ADAPTIVE_V3_CURVES_AND_CROSSINGS.md) | Documents the integrated controller's crossing and route-required behavior. Treat the standalone barcode trial and integrated line follower as separate test paths. |

The tribunal and requirement files are in the local `embedded/project` folder beside the Git checkout. The integrated motion controller's turn geometry is in `app_program/subsystem2/motion.c` under `begin_turn()`. The standalone U-turn uses the same pivot geometry, with about **2667 counts per wheel** for 180°. The right route instead arcs forward, acquires a right edge and traces the outgoing line. The acquisition distance, edge threshold and physical path still need track validation. Add and test left routing after the right route works.

## Firmware and branch reference

| Branch | Purpose |
|---|---|
| `feature/barcode-terminal` | Main analogue scanner trial, pushed through `2ea77c2`. GP26 reads A0. Raw ADC **≤200** is white, **≥240** is black, and readings between keep the current colour. Samples every **333 µs**, prints widths in ms, uses a **1.5 s** white timeout, and has no 6 mm filter. |
| `feature/barcode-a-right-turn` | Current local motion trial. It inherits the analogue scanner settings and adds junction detection plus the A, C and D manoeuvres. |
| `feature/barcode-digital` | Local DOUT comparison. Wire the barcode module's 3.3 V-compatible D0 to GP26. Press `p` to see the level and `i` to invert its interpretation. GP27 and GP28 use analogue inputs. |
| `feature/barcode-relative-edges` | Local analogue experiment using a 7% change sustained for five samples and a white ceiling of 210. The temporary three-channel raw display edit is in a named Git stash. |
| `feature/gp20-adaptive-v5` | Integrated GP20 line follower and source of the motion and sensor setup used for the standalone trials. |

The Pico W bench code is in this folder. `ir_line_bench.c` acquires samples, applies the barcode thresholds, reports scans and tracks the two line sensors. `scan_drive.h` controls the wheels, retry state and turn demo. Shared capture is in `app_program/subsystem3/barcode_sense.c` and Code 39 decoding in `app_program/subsystem2/motion_logic.c`.

## Testing and interpreting logs

- Flash [the current right-turn trial UF2](build-terminal/a-right-turn/ir_line_bench.uf2) via BOOTSEL. Build it again with `cmake --build build-terminal/a-right-turn -j 8` from this folder. The normal analogue build is in `build-terminal` and the DOUT build is in `build-digital`.
- Open VS Code Serial Monitor at **115200 baud** on the Pico's COM port, previously COM5. Send `p` to toggle live readings. `BC_RAW` is the 12-bit ADC value used by the analogue scan decision. `BC` is a calibrated 0–100 display. `A` and `B` are calibrated left and right line readings. `c` recalibrates for the current session and does not move the fixed raw barcode cutoffs.
- Photos in the local `embedded` folder were measured as `qr3.jpg` = **A** and `qr2.jpg` = **Z**. The older `qr1.jpg` also encodes A and has an unusually thin first bar. The firmware prints the decoded letter.
- The one-character Code 39 trial expects **29 elements**: three nine-element groups for start, data and stop, plus two separator gaps. Both black bars and white spaces carry width information. The rank-only trial classifies the longest three elements in each group as wide. It still checks the start and stop patterns, supported letter, count and ambiguous ties. A valid count alone does not prove a decoded letter.
- In GP20 scan mode, the forward leg has an eight-second limit. A partial capture of 5–28 elements triggers an encoder-based return to the pass start and a retry. A 29-element capture reports a decode or rejection while the car keeps approaching the junction.
- Sensor height, paper clipping and narrow white gaps were recurring issues. A 27-element scan with an unusually long black run is consistent with a missed white gap merging two bars. Very short pulses can also create extra edges. Use the ordered `sequence:` and `widths_ms:` lines to distinguish a count problem from a pattern rejection.
- For the current right-branch test, record whether it logs `RIGHT EXIT: line acquired`, follows the outgoing line, and the final `DRIVE stopped` reason. **12** means the turn encoder goals were reached. **13** means the turn timed out. **14** means straight travel completed and **15** means it timed out. **16** means the right exit was not acquired within six seconds, **17** means the eight-second line trace finished, and **18** means the line was lost or sensor data became invalid or stale.

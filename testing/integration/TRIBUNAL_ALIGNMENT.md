# Tribunal alignment on the BARR-C branch

The Group Project copy of SE08_PreTri2 supplies the operating requirements for this A0 build. The team chose its unarmed straight crossing and its per-run hump maximum. The worksheet supplies compatible safety changes. The earlier audit and tribunal submissions describe earlier revisions.

## Changes and software evidence

| Tribunal reference | Branch behavior | Check |
| --- | --- | --- |
| T1 K1/K2, SR3 | Four ordinary dashboard commands fit in arrival order. A remote STOP has an independent latch that the Wi-Fi producer can write while the transport mutex is busy. An accepted STOP takes priority and discards queued motion. Boot, session, age and command ID are checked. With fresh motor status the epoch must match. With unavailable motor status a fresh in-session STOP can still inhibit drive. GP20 and terrain can request the persistent motor STOP directly. | Remote wire and console suites exercise a full FIFO, order, contention, stale epoch, reconnect, zero-output acknowledgement and deliberate restart. |
| T1 K2 and T2 FR9 | A dashboard Start selects the 5 s lease. GP20 selects local autonomous operation. The chosen T2 policy determines authority from the start source. | Remote watchdog and unlimited-run simulations cover both sources beyond 60 s. |
| T1 K3 | The S3 status copy contains all three raw and normalized channels, calibration revision, acquisition time and sample validity. A successful calibration advances the revision. Frames discarded during an epoch change cannot be reported as a valid new sample. The T2 motor interface continues to carry the steering pair and completed barcode captures separately. Calibrated metric wheel data stays unavailable pending physical calibration. | Actual IR runtime and bridge simulation covers failed and successful calibration, epoch races, coherent publication and fresh acquisition. |
| T1 minimum-bit change and T2 NFR4 | MQTT status heartbeat carries a one-byte wrapping sequence. The robot subscribes to commands. The dashboard telemetry frame keeps its 96-byte v2 layout, including reserved space for compatibility. | Target build, C encoder to JavaScript decoder tests, and dashboard receiver tests. |
| T2 FR2 and worksheet 3/5 | Active search starts after the uncertain approach. Search stops at 1,500 ms or 1,000 counts of displacement on either wheel from the search origin. The 240/320 differential sweeps can end it sooner. A deliberately paused adaptive wheel may settle by two cumulative reverse ticks while its window is younger than 140 ms. Powered stalls and excessive reverse motion stop the run. | Boundary tests cover 1,499/1,500 ms, 999/1,000 counts, timer wrap, both sweep directions and wheel-pause budgets. |
| T2 FR3/FR8 | A decoded letter latches a junction action. A map edit, an Ignore edit, or a second scan cannot replace it. Subsequent eligible scans use the updated table. An unarmed junction is crossed straight. Both-dark confirmation still includes the bend check. The dashboard labels the displayed instruction as the mapping for the next scan. | Real route owner simulations cover remapping D on approach, an ignored mapping, unarmed crossing and nominal route completion. The flash journal tests cover interrupted writes. |
| T2 FR5/FR6 and worksheet 7/14 | An accepted line Start requests a new terrain run. The terrain task clears previous and partial hump measurements while preserving calibration and lift protection. Bad acquisitions invalidate terrain at once. A configured sensor recovering from a fault must provide a baseline and two changed XYZ samples before fault clearance. The line owner checks calibrated, recent, unlifted terrain before starting and during a run. | Simulated admission denial, interruption during motion, accepted-run reset and synthetic terrain/lift cases. Physical I2C recovery still needs a board. |
| Worksheet 13 | Motion arms the RP2040 watchdog before admitting drive. A completed periodic owner iteration feeds it. A failed safety handoff withholds the feed. The selected timeout is 250 ms. | Cross-built the register driver. Injected a failing safety flag read into the real owner loop and observed zero simulated outputs and no further feeds. Actual hardware timeout, reboot and wheel coast remain unmeasured. |

## Size comparison

Same Arm GNU 14.2 toolchain and A0 route, MQTT, remote and unlimited profile. The starting revision was d4577a1. The values below are arm-none-eabi-size results for the linked images.

| Measure | Starting revision | Updated branch |
| --- | ---: | ---: |
| text (bytes) | 424,972 | 427,120 |
| data (bytes) | 0 | 0 |
| bss (bytes) | 106,936 | 107,176 |
| Telemetry/event application payload (bytes) | 96 | 96 |
| Status heartbeat application payload (bytes) | 4–13 for a 32-bit decimal sequence | 1 |

The ordinary command FIFO and safety state are included in the updated footprint. The figures exclude MQTT/TCP overhead, live stack high-water marks and broker retransmissions. The comparison measures this change from its actual parent. The original pre-packing application has no measured whole-image baseline here.

## Verification limits

The route, adaptive steering, IR handoff, barcode replay, remote control, flash journal and telemetry suites passed. The dashboard passed 33 tests. Ubuntu GCC passed 16 synthetic lift/run-reset checks, terrain packing and math checks. Its synthetic full-width hump measured 18.9 mm for a 19.9 mm input. The Windows GCC lift suite reproduced an existing failure on both revisions. Case 7 missed the simulated 0.03 g slow lift with stationary wheels under the Windows noise sequence. Real pickup sensitivity is still open.

Physical wheel calibration, maximum 2 ms release jitter, 20 ms STOP-to-safe-output latency, 5-degree turn accuracy, watchdog reset and stopping distance require instrumented robot trials. A zero-duty host assertion establishes the software command only. The target linker emitted the existing RWX LOAD-segment warning on both builds. Run commands for the host suites and Ubuntu terrain simulators are in [BARR_C_SIMULATION.md](BARR_C_SIMULATION.md).

## Submitted Tribunal 1 follow-up

The PostTri1 C06 action has five legal values. The active A–Z table now occupies 12 bytes, down from 26, using three aligned words. Each action fits in a single three-bit field. Command queue entries use the same representation. Firmware tests exercise all five actions for each of the 26 letters, adjacent fields across word boundaries, invalid values, edits after arming and interrupted saves. RC/2 and RS/2 retain their deployed byte offsets, and MAP1 flash pages remain readable by earlier firmware. Transport compatibility therefore remains an explicit exception to minimum-width packing.

When the terrain task loses the encoder baseline during a hump, it discards the partial measurement and retains the highest completed hump. A repeated fresh snapshot adds no distance and can be followed by the next count delta. Pitch, roll and lift protection continue from the IMU. Comments at the relevant source paths explain the source-selected communication lease, IR capture rate, provisional wheel geometry, terrain validity, tilt thresholds and IMU retry policy.

Relative to c13f669, the forced rebuild of the same A0 profile changed linked BSS from 107,176 to 107,112 bytes and text from 427,120 to 427,448 bytes. The route, remote control, barcode replay and telemetry suites passed. All 33 dashboard tests passed. Ubuntu passed 17 lift/terrain checks, including interruption during descent, as well as packing and math checks. A 19.9 mm synthetic hump measured 18.9 mm. Windows reproduced the previously recorded case 7 slow-lift failure with its noise sequence. The sensitivity and integrated timing targets still require physical measurements.

## Stationary slow-lift follow-up

In case 7 the Windows noise trace reached 0.006898 g·s at the last upward sample against a 0.007065 g·s threshold. Ubuntu reached 0.007308 against 0.007220. At the first braking sample under Windows, the median-filtered rise reached 0.007299 but the vibration-adjusted threshold climbed to 0.007701. The stationary detector now also recognises a sustained rise immediately followed by braking.

Windows GCC and Ubuntu 24.04 GCC each passed all 19 lift checks, including 4,096 native random seeds each for the 0.03 g lift and the 0.012 g negative control. Both passed packing and math, and the recorded A8 hump replay produced zero lift events. The synthetic 19.9 mm hump measured 19.4 mm on Windows and 18.9 mm on Ubuntu. Both detected the 5, 10 and 13 mm one-sided humps and missed the 4 mm hump. The A0 UF2 rebuilt successfully. Physical pickup sensitivity remains unverified.

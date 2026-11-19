# RoboCar project handoff

**Resume here · 5 November 2026 · Asia/Singapore**

We are Buddy 2 on the INF2004 RoboCar project. The goal is to follow an unfamiliar black-line circuit using onboard sensors and wheel feedback, with barcode route decisions and telemetry eventually integrated. The current driving milestone is **GP20 Adaptive V5**. It starts on the button, runs on battery for up to 30 seconds, and received positive feedback from the operator. The most recent repository action was merging PR #4 into PR #3’s branch. PR #3 is still open.

## Start a new session

1. Work in the repository below. Run `git status --short --branch` and `git fetch origin`, then check the current PRs before choosing a branch. The fetched remote PR #3 branch is the combined telemetry/V5 baseline for follow-up work. Preserve any local work.
2. For **code or Git work**, no robot connection is necessary. The user’s last merge instruction was specifically to merge #4 into #3 first. A merge of #3 into integration is a separate decision.
3. For **hardware work**, identify the running image from its USB banner with GP20 unpressed. V5 is the last image verified in this session. A separate barcode firmware also assigns a function to GP20. The board was not queried when preparing this handoff.
4. The user approved software-only compilation and host checks earlier. Prompt before another flash or movement test. For flashing, confirm the wheels are lifted and the board is in BOOTSEL. The operator starts floor runs with GP20. If investigating an existing stop, preserve its evidence before flashing or starting another run.

## Repository, branches and course material

Local repository on this laptop:

```text
C:\Users\joell\OneDrive\Documents\SIT SHIT\Y2T1\INF2004 - Embedded Systems Programming\INF2004_Embedded_Systems_Car
```

[GitHub repository](https://github.com/sleepingywn/INF2004_Embedded_Systems_Car). Remote state checked on 5 November:

| Ref | Commit | What the next session should use it for |
| --- | --- | --- |
| `origin/main` | `8b5430d` | Template-era main branch. |
| `origin/integration/buddy2-motion` | `86daedc` | Earlier motor/IR integration. [PR #2](https://github.com/sleepingywn/INF2004_Embedded_Systems_Car/pull/2) is open against main. |
| `origin/integration/s2-s3-s4` | `2050fa1` | Integrated motion, IR and terrain baseline. This is PR #3’s destination. |
| `origin/feature/robocar-telemetry-line-follow-autostart` | `d3a939a` | **Combined branch.** [PR #3](https://github.com/sleepingywn/INF2004_Embedded_Systems_Car/pull/3) contains telemetry, the dashboard and V5. Its historical title still highlights five-second autostart. |
| `origin/feature/gp20-adaptive-v5` | `88b4e01` | [PR #4](https://github.com/sleepingywn/INF2004_Embedded_Systems_Car/pull/4) merged into #3. Its file tree matches the combined branch at the recorded commits. The local checkout was on this branch when the handoff was written. |
| `origin/feature/barcode-terminal` | `33b96c3` | Separate barcode bench work based on `88b4e01`. Inspect separately before integrating any of it. |

On this laptop the local branch *named* `feature/robocar-telemetry-line-follow-autostart` was still at `12b6bec` at handoff. Read the fetched `origin/...` ref for the merged state. A safe inspection of the separate barcode work is `git show origin/feature/barcode-terminal:testing/subsystem3/ir_line_bench/README.md`.

Consult the module’s [project briefing](../Group%20Project/Briefings/Project%20Briefing.pdf), [barcode and line specification](../Group%20Project/Briefings/Barcode%20%26%20Line%20Specification.pdf), [project write-up](../Group%20Project/Briefings/Project%20Write-up.pdf), [Tribunal 1 Draft 4](../Group%20Project/Tribunal/Draft4/Week8_Tribunal1_Draft4.md), and the [HTML wiring viewer](../output/html/PicoCar-Wiring-Viewer-v2.html). These are beside the repo in the shared module folder. The report records proposed contracts and evidence requirements. Use current firmware and dated measurements to establish implementation status. The course’s minimum-bit representation requirement covers application state, faults, task/ISR messages and MQTT data. Keep validity, freshness, safe shared access and measured evidence in scope when extending an interface.

## Responsibilities and interfaces

| Owner | Responsibility and integration point |
| --- | --- |
| Buddy 1 | Samples motion, IR and terrain status, packages telemetry/events, serves diagnostics and hands records to the Wi-Fi task. The laptop runs the broker and dashboard. |
| **Buddy 2** | Owns motor outputs, quadrature encoder counts, speed control, request arbitration, STOP and fault handling. V5’s line-trace request is local. Request IDs, run epochs and freshness prevent old requests or sensor data from being reused. |
| Buddy 3 | Samples GP26–GP28, normalizes reflectance, derives line state and captures barcode widths. The S2 bridge publishes the left/right pair together with the line message, acquisition time and epoch. A STOP/reset race discards superseded frames and partial barcodes, then resumes on a fresh epoch. |
| Buddy 4 | Uses the GY-511 for terrain/hump/tilt/collision observations. It consumes S2’s coherent wheel counts and acquisition time to derive motion estimates. Encoder data matters to Buddy 4. Provisional wheel geometry limits the accuracy of those estimates. Coordinate owner review for terrain changes. |
| Whole-car controller | Final mission arbitration, barcode manoeuvres and terrain reactions still need integration. V5 implements the local line-following trial. |

See [TEAM.md](TEAM.md) for interface ownership and kernel objects. S2/S3 and S4 reused some IF numbers in their original plans. Use the supplying module and function names when discussing an interface. [October integration issues](testing/integration/ISSUES.md) are a historical baseline. Later work added telemetry, powered line following, bounded crossing behavior and saved calibration for the circuit profile.

## Physical robot and calibration

The target is a Raspberry Pi Pico W on a Cytron Robo Pico motor carrier. Pin assignments are fixed. Use the analog output labelled AO/A0 for the line inputs.

| Function | Connection | Verified behavior |
| --- | --- | --- |
| Left motor control | GP8 / GP9 | Forward motor sign −1 |
| Right motor control | GP10 / GP11 | Forward motor sign +1 |
| Left encoder A/B | GP4 / GP5 | Raw count decreases when moving forward |
| Right encoder A/B | GP2 / GP3 | Raw count increases when moving forward |
| Barcode reflectance | GP26 | Saved white/black ADC readings 183 / 2985 |
| Left line reflectance | GP27 | Saved white/black 173 / 2623 |
| Right line reflectance | GP28 | Saved white/black 160 / 2533 |
| Start/stop | GP20 onboard button | Active low, deliberate press and release |
| Status | Pico W’s small green LED by USB | Wireless chip WL_GPIO0, separate from the carrier power light |

**Wheel calibration.** Short runs sometimes appeared straight for one second, then drifted right or weaved. With taped tyres and wheels lifted, two roughly 1.5-turn recordings produced −3835 left and +3835 right counts in separate tests. That suggests roughly 2550 counts/revolution, with uncertainty in the visual angle and an earlier outlier. The production configuration still carries a provisional 2774 counts/revolution, 204.2 mm wheel circumference and 125 mm track width. `MOTION_CALIBRATED=0`. V5 uses counts and counts/second. The user chose to stop manual wheel calibration and prioritize line following. [Measurement history](testing/subsystem2/MEASUREMENTS.md)

**Sensor placement.** The two front sensing spots were moved closer together. Readings are normalized separately on a 0–100 darkness scale. A reading of 80 describes calibrated reflectance, not a literal 80% area of black under the tip. In OUTSIDE placement, both-white can mean the line is centered between the sensors or that the robot has left it. Check at normal operating height using fully white, fully black, centered, and small left/right offsets. GP28 once read about 42–50 on white and 33–49 on black. Contrast returned after reseating the signal/power/ground connections. Recheck raw contrast before tuning steering if one channel goes flat. [Right sensor investigation](testing/integration/GP20_STARTUP_AND_GP28_CHECK.md)

## What the V5 robot does

- **Start and stop.** Power-up leaves the motor outputs at zero. After healthy inputs and arming, GP20 starts one run. Another press requests STOP. The trial lasts at most 30 seconds with a 20% duty cap. Saved IR endpoints are loaded for the battery circuit profile. Local driving works without the laptop, MQTT or Wi-Fi association. The old five-second autostart image is a separate build.
- **LED.** Slow blink means armed, paired pulses mean a request is pending, and steady green means the matching run was accepted. Fast blinking can indicate rejection, a fault or stale/unavailable status. Off can mean startup, completion, explicit stop or disarm. Read the stored reason before diagnosing a fault from the LED.
- **Steering.** Positive left-minus-right darkness means steer left by slowing the left wheel. Filtered error and its rate determine wheel targets. The count-domain settings are 450 cruise, 250 minimum rolling and 600 maximum counts/s. Sustained steering demand can pause the inside wheel after confirmation. The speed loop tracks each target separately and softens startup/restart. No assumed bend location or duration determines when to exit a curve.
- **Ambiguous surface.** When both eyes lose directional evidence, V5 briefly continues under a travel bound, then searches toward the last known side and back the other way. Recovery is bounded by 3000 ms / 800 counts and sweep limits. Both eyes at or above 75 darkness can indicate a crossing. With no route instruction, the agreed policy tries straight ahead. Mark limit 1800 ms / 400 counts, overall crossing limit 3500 ms / 900 counts. Stable clear-line evidence is required before renewing the crossing budget. A crossing is one possible explanation for both eyes being dark.
- **Barcode routing.** In the integrated trial, C permits straight travel. A/B/D request unsupported route execution and cause a stop. The separate terminal bench recognizes printed letters, which is a different milestone from executing turns.
- **Encoder settling.** During a deliberate pause, the previous target and motor duty must both be zero. V5 tolerates up to **two gross backward counts before 120 ms**. The third count, reversal at or after 120 ms, powered reversal and invalid transitions still fault. Encoder bounce or a brief restart cannot replenish the budget. A new allowance requires at least 60 ms of consecutive powered forward progress, at least three forward counts, and the minimum rolling target. Ordinary STOP, stalls, invalid/stale inputs and run bounds remain active.

[Release and expected behavior](testing/integration/ADAPTIVE_V5_RELEASE.md) · [V5 investigation](testing/integration/ADAPTIVE_V5_BOUNDED_PAUSE_SETTLING.md) · [IR handoff fix](testing/integration/IR_HANDOFF_EPOCH_FIX.md)

## Why V5 exists and what was actually observed

| Run | Evidence and interpretation |
| --- | --- |
| Early wheel and IR trials | Both wheels moved forward. Straight-line trim alone varied across runs. A two-second integrated test followed the line. Later battery runs jerked, missed bends or stopped early. Repeated extensions of curve-specific timers did not address the goal of varied courses. |
| Adaptive V2 | A right-start run traversed the bend and reached a transverse mark. It stopped at 17.261 s for a route decision, with no motion fault. The chassis appeared to lag the sensor path near the bend exit. |
| Adaptive V3 | Added straight crossing and changed pivot behavior. The latest left-start run stopped at 12.567 s. Its right encoder receded while the right motor was paused. The retained history contained no crossing mode and healthy IR. Its reason cannot be assigned to the older right-start recording. |
| Adaptive V4 | Restored V2 steering and kept V3 crossing. Added frozen encoder diagnostics. At 14.158 s, right-wheel target/duty were 0/0, left target was 600, and the right encoder recorded −1 at pause age 40 ms and −1 again at 60 ms. The second count exceeded the one-count budget. IR was fresh with no handoff fault. Telemetry established the stop rule but could not distinguish physical rollback from encoder noise. |
| Adaptive V5 | Changed pause settling alone for a controlled comparison. Built and flashed on 4 November. Exact V5 banner, GP20 armed, six zero-output snapshots and no motion/IR faults were verified. The user subsequently reported “ok this is good.” Exact duration, full-course completion and a saved V5 floor-run trace were not supplied. Repeated steering corrections and chassis alignment remain open. |

The recordings live under the operator’s local `Downloads/Telegram Desktop`. Useful identifiers are `video_2026-11-04_00-49-34.mp4` for V2 and `video_2026-11-04_01-58-52.mp4` for the latest V3 left-start run. `01-58-56.mp4` was the older right-start run despite its filename. Use video for chassis behavior and the corresponding saved trace for the stop cause.

## Build, verification and recovery

The checked-in source map is small enough to navigate by responsibility:

- [motion.c](app_program/subsystem2/motion.c) owns motor writes, faults and stopped-run history. [motion_config.h](app_program/subsystem2/motion_config.h) defines pins, signs and calibration state.
- [line_follow.h](app_program/subsystem2/line_follow.h) applies encoder/time guards and wheel speed targets. [line_adaptive.h](app_program/subsystem2/line_adaptive.h) computes steering and search. [line_crossing.h](app_program/subsystem2/line_crossing.h) bounds crossings.
- [line_console.c](app_program/subsystem2/line_console.c) implements the GP20 trial console and status. [ir_runtime.c](app_program/subsystem3/ir_runtime.c) and [motion_ir_bridge.c](app_program/subsystem2/motion_ir_bridge.c) connect sensor acquisition to the motor owner.
- [config/line_boot_calibration.h](config/line_boot_calibration.h) contains the measured ADC endpoints. [tools/dashboard](tools/dashboard/README.md) contains the laptop receiver and its setup guide.

From the repo root, with host GCC on PATH, the software validation commands are:

```sh
python testing/subsystem2/run_adaptive_tests.py
python testing/subsystem2/run_regression_tests.py
```

The recorded result is **46 executable suites** (8 adaptive, including 39 motor-owner scenarios, and 38 regression) plus **2 invalid-profile checks**. On Windows, the host GCC used was under `C:\msys64\ucrt64\bin`. The V5 cross-build used Pico SDK 2.3.1 and Arm GNU Toolchain 15.2.Rel1. In MSYS with the cross-compiler on PATH and `PICO_SDK_PATH` set:

```sh
make -C build_make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 \
  LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1 \
  E2U=tools/elf2uf2.exe
```

The verified laptop artifact is `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_adaptive.uf2`. Its SHA-256 is **`A152BE67F5CEF02319A5B4E1A8E5E4D32F7FA69311A08D53CECDE81FB6ABC3C8`**. The hash was rechecked for this handoff. Builds on another laptop may differ. Local secrets, supplied separately, are needed for the matching network configuration.

**Flash procedure after operator approval.** Keep wheels lifted. Use manual BOOTSEL, find the volume labelled `RPI-RP2`, confirm `Board-ID: RPI-RP2` in `INFO_UF2.TXT`, and check the intended UF2 hash before copying. The drive letter can change. After reboot, confirm the exact `ADAPTIVE v5` banner, GP20 armed, zero motor duty and no faults with help/status. The software USB boot command previously caused Windows USB descriptor failures. On this carrier, if manual BOOTSEL is elusive, power fully off until the board light goes out, hold Pico BOOTSEL while reconnecting USB and turning on the carrier, then release. Keep GP20 unpressed for stationary verification.

**After an unexpected stop**, keep the current power source connected. Connect USB without RESET/BOOTSEL or another GP20 start. The integrated console uses `h` for help/version, `i` for status and `t` for the next eight history samples. Read the entire stopped-run history. Confirm run ID/epoch, fresh sensor levels, time, stop reason, prior wheel targets/duty, signed encoder deltas and used allowance. Compare target versus measured speed before changing motor power. Compare both starting directions under the same placement. A recording and trace from the same run are more useful than another blind threshold adjustment.

On this laptop the following are **ignored, local-only** files under `tools/dashboard/.local/`:

| File | Purpose |
| --- | --- |
| `adaptive-v5-manifest.json`, `adaptive-v5-build.log` | Build identity, verification and operator feedback |
| `gp20-adaptive-v5-stationary-20261104-023636.json` | Six verified V5 stationary snapshots |
| `gp20-adaptive-v4-result-20261104-021735.log` | Frozen V4 failure and paged history used to design V5 |
| `check_gp20_adaptive_v5_stationary.py` | Verifies V5 with status commands and can send STOP on verification failure |
| `read_adaptive_v5_result.py` | Reads help/status/history and saves evidence. Uses 115200 baud and identifies the Pico by USB VID 0x2e8a and serial MTK3PICO. COM6 was seen previously. |

Run the helpers from the repo root. On this host set PowerShell’s `$env:PYTHONPATH = "$env:TEMP\robocar-python"` for the existing pyserial package. Another clone can use the console commands directly. A reset, new run or loss of power clears the RAM evidence.

## Dashboard and telemetry

The user chose a **private Wi-Fi network or phone hotspot** for Pico-to-laptop traffic. Mosquitto and Node-RED run on the laptop. The robot publishes through MQTT and friends view Node-RED privately through Tailscale Serve. Navigation runs onboard. Keep the laptop awake for telemetry.

| Setting | Value |
| --- | --- |
| Broker | Authenticated Mosquitto on the laptop’s private Wi-Fi IPv4, TCP 1884. The installer’s separate service uses 1883. |
| Robot/topic | `inf2004/se08/car01/v1` with `/telemetry`, `/events`, and `/status` |
| Payload | 80-byte little-endian telemetry/event frames, QoS 1. Status is a text heartbeat. |
| Receiver | Node-RED at `http://127.0.0.1:1880/dashboard/robot`, editor disabled |
| Friends’ access | Use the private HTTPS hostname from `tailscale serve status` plus `/dashboard/robot`. Access requires Tailscale authorization and the laptop serving the route. |

S1 samples every 250 ms. It keeps the latest telemetry and an eight-event FIFO. The Wi-Fi task owns MQTT calls. Sequence/boot IDs distinguish restarts and duplicates. Dashboard age measures time since receipt. Earlier stationary tests received real Pico telemetry and verified private dashboard access, authenticated delivery and reconnect. One capture had 16 receipt gaps over one second, maximum 2.936 s, even when firmware publication times were 250 ms apart. Investigate the transport separately from on-device sensor freshness.

To resume on this laptop, check existing services and listeners before starting duplicates. From `tools/dashboard`, the project broker uses `mosquitto.exe -c .local/mosquitto.conf`, and Node-RED uses `npm start`. Dependencies can be restored with `npm ci --ignore-scripts`. When the hotspot address changes, update the local broker listener, firmware broker address and private-subnet firewall rule. See [dashboard setup](tools/dashboard/README.md). Keep `config/wifi_secrets.h`, broker accounts/password files and `flows_cred.json` local and ignored.

## Next work, in order of the user’s chosen task

1. **Repository continuation.** PR #3 is ready for a description update covering telemetry and GP20 V5 if the user asks to continue preparing it. A later merge into `integration/s2-s3-s4` needs the user’s instruction. The team should review shared S2/S3/S4 interfaces. Starting a new coding task from the combined branch avoids replaying #4.
2. **Line-following validation.** Preserve V5 as the comparison baseline. Record matched starting position, direction, sensor height and lighting. Compare bend entry/exit, chassis alignment, repeated inner-wheel pauses, crossings and exact stop reason. Operator feedback is positive, but repeatability and varied-course performance need evidence. Investigate steering oscillation separately from encoder settling.
3. **Barcode milestone.** Inspect `feature/barcode-terminal` before duplicating work. Its standalone **terminal v11** bench recognizes Code 39 A/B/C/D/Z. It can drive forward on GP20, reverse by measured encoder travel following a partial scan and retry. It has per-leg bounds and no overall retry-count cap. Its experimental rank-only decoder relaxes width checks. These semantics differ from V5’s GP20 line run. Physical recognition/return accuracy and the final mapping from a printed character to a route still need verification. Branch-specific guide: `testing/subsystem3/ir_line_bench/README.md` on that ref.
4. **Whole-car integration.** Coordinate fresh, one-shot barcode route decisions with B3. Coordinate terrain event reset/consumption, tilt/collision handling and calibrated wheel data with B4. Keep Buddy 4 changes scoped to an agreed interface. Check the minimum-bit contract and timing under load as new state and messages are added.
5. **Calibration and telemetry follow-ups.** Revisit measured rolling distance and metric navigation only when the user wants that work. Investigate receiver gaps independently. For any new fault, first establish whether the run stopped for motion, IR, exhausted recovery, route handling, a button press or the 30-second bound.

For the next session, read this file first, verify the current branch and runtime banner as relevant, then follow the user’s selected next task. The document is a snapshot of the local project and verified remote history on 5 November 2026.

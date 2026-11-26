# SE08 autonomous robocar · final A0 firmware

Submission branch `feature/barr-c-standard` targets the Robo Pico with a
Raspberry Pi Pico W running dual-core micro T-Kernel. The barcode sensor uses
analogue A0 on GP26. The left and right line sensors use GP27 and GP28.
The IMU uses GP16 (SDA) and GP17 (SCL).

GP20 starts an autonomous run. Dashboard Start selects a five-second heartbeat
lease. Neither start mode has a fixed run duration. A decoded letter arms the
action for the approaching junction. Mapping edits apply to later eligible
scans. The highest completed hump resets at the start of each run.

## Build the submission image

Start at the root of this checkout. Use a shell with GNU Make, Git, a host C++
compiler and the Arm bare-metal toolchain on PATH. The verified compiler is
Arm GNU 14.2.Rel1. On Windows the build also works in MSYS2 with its GNU Make.
On Linux or WSL the commands below assume Bash.

1. Point `PICO_SDK_PATH` at a Pico C SDK checkout. The verified SDK is 2.3.0. Initialise its USB, radio and lwIP submodules.

   ```sh
   export PICO_SDK_PATH="$HOME/pico/pico-sdk"
   git -C "$PICO_SDK_PATH" submodule update --init lib/tinyusb lib/cyw43-driver lib/lwip
   arm-none-eabi-gcc --version
   ```

2. For a fresh checkout, copy `config/wifi_secrets.example.h` to
   `config/wifi_secrets.h`. Enter the team's hotspot SSID, Wi-Fi credentials,
   laptop broker address and matching MQTT credentials. The dashboard uses
   the private broker on port 1884. Keep the local file out of Git.
   [Dashboard and broker setup](tools/dashboard/README.md)

3. Force a complete build with the final profile.

   ```sh
   make -C build_make -B -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 \
     LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1 \
     LINE_ROUTE=1 LINE_REMOTE=1 BARCODE_DIGITAL=0 LINE_UNLIMITED=1 TEST=0
   ```

   The output is
   `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_adaptive_route_remote_no_time_limit.uf2`.
   The inherited `line30` filename component identifies the circuit profile.
   `no_time_limit` identifies the final run policy. Adaptive steering owns line
   recovery in this configuration. A bare `make` selects a different image.

4. To install on a robot, hold BOOTSEL while connecting USB, release it and
   copy the named UF2 to RPI-RP2. The board reboots after copying. Leave GP20
   released until ready to start. The final build and simulations run without
   flashing or connecting a robot.

## Reproduce the software checks

Use Python 3, host GCC and Node.js with npm. Run these from the repository
root. On Windows PowerShell, use `python` and `npm.cmd` as appropriate.

```sh
python3 testing/subsystem2/run_route_tests.py
python3 testing/subsystem2/run_remote_tests.py
python3 testing/subsystem1/run_barcode_telemetry_tests.py
python3 testing/subsystem4/run_terrain_tests.py
npm --prefix tools/dashboard test
```

The route runner also runs the adaptive and legacy regression suites. The
terrain runner builds fresh binaries in a temporary directory and includes the
recorded A8 replay. Run it with native Windows GCC and Ubuntu GCC to exercise
both simulated noise sequences. The dashboard suite contains 33 tests.

## What still needs the robot

| Requirement | Evidence to collect on the final image |
|---|---|
| Motor calibration and navigation accuracy | Measure each wheel under load against the ±10% speed target within 1 s. Record ten trials for the 1 m distance and lateral error targets (≤20 mm) and the 90°/180° turn target (≤5°). Geometry remains provisional with `MOTION_CALIBRATED=0`. |
| Real-time control and stopping | Measure the 20 ms control period and ≤2 ms jitter under simultaneous sensing and Wi-Fi load. Time a valid STOP to safe motor outputs (≤20 ms). Measure wheel coast separately. Verify the 250 ms watchdog, reset and safe restart on the board. |
| Encoder integrity | Inject or independently count 10,000 reference pulses per wheel up to the proposed 2 kHz rate and compare missed and extra counts. |
| Track and barcode reliability | Record a complete course run, completion time, bends, crossings, recovery, and each mapped action. Count correct, missed and wrong decodes across lighting, approach angles and speeds. |
| IMU and terrain | Compare per-run hump peaks with measured heights. Check stationary slow pickups, ordinary bumps, battery-only sampling, and recovery after a real I²C fault. Both software hosts missed the 4 mm one-sided synthetic hump. |
| Wireless and resource limits | Verify battery-powered MQTT on the private hotspot, timestamp gaps and reconnects under motion, time STOP/lease loss at the wheels, and measure task stack headroom and queue pressure under concurrent load. |

The assessment scope covers Buddies 1–4. Buddy 5 obstacle avoidance and IMU
collision detection were removed from the team's scope. Previous robot footage
documents the demonstrated revisions. The final submission profile has
software and cross-build evidence.
[Tribunal traceability and results](testing/integration/TRIBUNAL_ALIGNMENT.md)

## Source and review guide

| Area | Entry point |
|---|---|
| RTOS startup | [Application entry](app_program/app_main.c) |
| MQTT transport, wire format and dashboard | [Dashboard takeover guide](tools/dashboard/TAKEOVER.md) |
| Motor owner, encoders and route execution | [Motion subsystem](app_program/subsystem2/README_MOTION.md) |
| IR acquisition and barcode capture | [IR runtime](app_program/subsystem3/README_RUNTIME.md) |
| IMU, lift and terrain | [Terrain processing](app_program/subsystem4/imu_logic.c) |
| Coding-standard scope and verification | [BARR-C review and simulation evidence](testing/integration/BARR_C_SIMULATION.md) |

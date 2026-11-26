# SE08 Autonomous Robocar

An INF2004 Embedded Systems Programming project built in C on a Raspberry Pi
Pico W and Robo Pico carrier, running micro T-Kernel.

Two analogue IR sensors guide the robot along a black line. A third reads
Code 39 barcodes. When the robot decodes a mapped letter, it carries out the
instruction at the next junction. Wheel encoders provide feedback for steering
and turns. The IMU detects when the robot is lifted and estimates hump height.

## Dashboard

The robot sends telemetry over Wi-Fi using MQTT. The dashboard shows wheel
movement, the latest decoded barcode, pitch and roll, and hump measurements.
Operators can start or stop a run and edit the A–Z barcode mappings.
The GP20 button starts an autonomous run directly from the robot.

- [GPIO assignments and wiring](TEAM.md)
- [Dashboard and MQTT setup](tools/dashboard/README.md)

## Test cases and evidence

[Watch the SE08 test-case video](https://youtu.be/IBY-M7DeD0M) (4:01). The [Buddy 1 decoder test starts at 0:20](https://youtu.be/IBY-M7DeD0M?t=20).

| Component | Test cases |
|---|---|
| Buddy 1 · Telemetry and MQTT | [Telemetry test cases and decoder results](testing/subsystem1/TELEMETRY_TEST_CASES.md) |
| Buddy 2 · Motors and encoders | [Motor test cases](testing/subsystem2/MOTOR_TEST_CASES.md) |
| Buddy 3 · Line tracking and barcode sensing | [Line and barcode test cases](testing/subsystem3/LINE_BARCODE_TEST_CASES.md) |
| Buddy 4 · IMU and terrain | [Terrain test cases and results](testing/subsystem4/TEST_LOG.md) |

The [demo video index](videos/README.md) links to the recorded telemetry,
bend, barcode, U-turn, hump and lift tests.

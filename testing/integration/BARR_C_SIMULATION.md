# BARR-C firmware branch simulation

Local branch `feature/barr-c-standard` is based on `feature/ctf2-andy` at
`ee763d4`. Scope is the first set of findings from the 26 November 2026 audit.
The Pico project still builds as GNU11. Whole-project BARR-C:2018 conformance
remains open.

| Check | Observed result |
|---|---|
| Line and terrain interfaces (5.3.a) | Removed signed bit-fields. Cross-compiled two representative line records and two terrain records with Arm GCC for Cortex-M0+. Their 24 data bytes matched the parent revision exactly, including negative values. Size/alignment stayed 4/4 bytes for line and 8/8 bytes for terrain. |
| Terrain numeric validity (5.4.b.v) | The packing test passed with undefined-behavior and float-cast-overflow sanitizers under Ubuntu GCC. NaN pitch and infinite height invalidate the terrain sample. Non-finite turn rate uses its own unavailable code. A finite pitch as large as FLT_MAX saturates. Existing rounding, saturation and timestamp-wrap checks passed. |
| IMU simulation | All 15 lift checks and the math checks passed under Ubuntu. The one-sided 5, 10 and 13 mm inputs measured 4.8, 9.4 and 12.2 mm. A 4 mm input produced no detection with the original threshold. The full-width synthetic hump measured 18.9 mm against 19.9 mm input. |
| Motor, IR and routes | The route runner passed, including adaptive steering, regression, line and barcode handoff, and replay of recorded scans. The replay accepted no incorrect letters. |
| Remote control and mapping | Remote watchdog, GP20 retry, mapping update and flash journal simulations passed. |
| Dashboard and transport | All 33 dashboard tests passed. C-to-JavaScript frame decoding, barcode snapshot ownership and transport queue tests passed. |
| Pico build | Built the A0 route UF2 with Wi-Fi, MQTT, dashboard control and unlimited run duration enabled. |

## Repeat the checks

From the repository root, run the following with Python, GCC and Node.js:

~~~sh
python testing/subsystem2/run_route_tests.py
python testing/subsystem2/run_remote_tests.py
python testing/subsystem1/run_barcode_telemetry_tests.py
npm --prefix tools/dashboard test
~~~

Under Ubuntu, set `s` and `t` as below, then run in a temporary directory so
the simulator's CSV stays outside the checkout.

~~~sh
s="$PWD/app_program/subsystem4"
t="$PWD/testing/subsystem4/imu_sim"
cd "$(mktemp -d)"
gcc -O2 -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all -I"$s" "$t/pack_check.c" "$s/terrain_pack.c" -lm -o pack && ./pack
gcc -O2 -I"$s" "$t/lift_check.c" "$s/imu_logic.c" -lm -o lift && ./lift
gcc -O2 -I"$s" "$t/math_check.c" -lm -o math && ./math
gcc -O2 -I"$s" "$t/imu_sim.c" "$s/imu_logic.c" -lm -o sim
for h in 4 5 10 13; do ./sim --fake1 "$h"; done
./sim --fake
~~~

These checks exercise firmware logic, wire frames and the target build.
Physical sensor noise, wheel traction, on-target scheduling and live Wi-Fi
remain hardware validation items.

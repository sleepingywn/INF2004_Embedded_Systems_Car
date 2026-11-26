# BARR-C submission review

## Final A0 candidate

The application uses the C99 dialect on RP2040. The micro T-Kernel BSP, SDK
and network libraries retain their integration build flags. The source checks
cover `app_program` startup, console and Buddies 1–4. The excluded Buddy 5
scaffold, third-party code and host simulators are outside this review.

| BARR-C:2018 finding | Final disposition and check |
|---|---|
| 1.1, C dialect and target extensions | The application compile command selects `-std=c99`. Named C99 typedef assertions still fail the build on invalid sizes, pin maps or thresholds. Both message headers also pass Arm GCC with `-std=c99 -pedantic-errors`. Target-specific alignment, interrupt/flash barriers and the kernel ABI still depend on Arm GCC. |
| 1.2, 1.3, 3.4 and 3.5, layout | All 95 in-scope C sources and headers passed an 80-column and zero-tab scan. The shared formatter uses four spaces, Allman braces and explicit control bodies. A C syntax-tree scan found compound bodies on the inspected if/for/while statements. |
| 4.2, interfaces and storage | Every in-scope C source has a same-stem header, and all headers have matching inclusion guards. Adaptive tuning, crossing tuning and the Code 39 table have source-file definitions. Existing subsystem entry points remain available. |
| 5.3 and 5.4, data representation | Compared two line records and two terrain records, plus alignment offsets, in Cortex-M0+ object data against c5388ca. The bytes matched. Terrain packing tests cover signed fields, saturation and invalid floating-point values. |
| 6.2, function length | The former 318-line IMU update now calls separate rise, attitude, hump, lift and tilt routines. Its main entry and each extracted routine fit within 100 source lines. Across the application, 15 other functions exceed the 100-line guideline, including the motor owner and terrain task. This remains a manual review item for their control flow and timing. |
| 8.3, switch defaults | The route phase dispatcher explicitly preserves the previous fallback for an unrecognised phase. The in-scope syntax-tree scan found a default in each switch. |

The checks establish the listed rules for the stated scope. They do not
certify every BARR-C rule. Legacy identifiers, multi-exit functions, compiler
extensions and the distinction between task and interrupt ownership still
need rule-by-rule review for an unqualified conformance claim.

### Reproduced results

- A forced A0 build with Arm GNU 14.2.Rel1 and Pico SDK 2.3.0 produced the
  UF2. Linked size was 427,728 bytes of text, zero initialised data and
  107,120 bytes of BSS. The inherited linker warning about an RWX LOAD
  segment remains. The optional on-target self-test profile also cross-built.
- The route, adaptive, regression, remote-control, telemetry and barcode suites
  passed. The regression runner ran 38 executables. All 33 dashboard tests passed.
- Native Windows and Ubuntu 24.04 GCC each passed all 19 lift cases plus
  math and packing. Each replay processed 1,090 recorded A8 samples and found
  one hump with no lift event. Both hosts detected the 5, 10 and 13 mm
  synthetic one-sided humps and missed the 4 mm case. On each host, the
  simulator output and per-sample CSV matched c5388ca in all six runs.
  Ubuntu also passed terrain packing with undefined-behaviour and
  float-to-integer conversion sanitizers enabled.

### Build the final A0 firmware

From the repo root, use GNU Make, a host C++ compiler and Arm GNU 14.2.Rel1.
Set `PICO_SDK_PATH` to Pico SDK 2.3.0 with its TinyUSB, CYW43 and lwIP
submodules initialised. For a fresh checkout, copy the
[Wi-Fi configuration example](../../config/wifi_secrets.example.h) to
`config/wifi_secrets.h`. Set the hotspot and broker credentials locally for
live telemetry.

~~~sh
make -C build_make -B -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 \
     LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1 \
     LINE_ROUTE=1 LINE_REMOTE=1 BARCODE_DIGITAL=0 LINE_UNLIMITED=1 TEST=0
~~~

The output is `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_adaptive_route_remote_no_time_limit.uf2`.

To repeat the application formatting and C99 message-header checks from the
repository root in Bash, use clang-format 23.1.5 and the Arm compiler.

~~~sh
git ls-files -z -- 'app_program/*.c' 'app_program/*.h' ':!app_program/subsystem5/*' |
  xargs -0 clang-format --dry-run --Werror --
for header in app_program/subsystem3/ir_types.h app_program/subsystem4/terrain_pack.h; do
  arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -std=c99 -pedantic-errors -x c -fsyntax-only "$header" || exit 1
done
~~~

## Earlier branch comparison
Local branch `feature/barr-c-standard` is based on `feature/ctf2-andy` at
`ee763d4`. Scope is the first set of findings from the 26 November 2026 audit.
At that revision the Pico project built as GNU11. Whole-project BARR-C:2018 conformance
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

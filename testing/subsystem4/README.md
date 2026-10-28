# Subsystem 4 test tools

| Folder | What | Runs on |
|---|---|---|
| `imu_bench/` | Standalone GY-511 bench test: plain Pico SDK loop, keyboard commands, CSV log. **Not** part of the team firmware. | Pico (flash its own `.uf2`) |
| `imu_sim/` | Laptop simulator for `imu_logic.c` (fake data, phone or bench CSV) and `math_check.c` | Laptop |
| `logs/` | Saved console output | |

Both use the **same** `app_program/subsystem4/imu_logic.c` as the firmware.

## imu_bench

```sh
cd testing/subsystem4/imu_bench
mkdir build && cd build
cmake .. && make            # -DPICO_BOARD=pico2 for a Pico 2
```

Flash `build/gy511_bench.uf2`, open a serial monitor and type a key:
`s` I2C scan · `r` raw axes · `n` status · `c` CSV log · `m` pretend moving · `z` recalibrate.
Wiring: VIN→3V3, GND→GND, SDA→GP16, SCL→GP17 (Robo Pico: Grove 4).

Flashing the bench `.uf2` replaces the team firmware on that Pico.

## imu_sim

```sh
cd testing/subsystem4/imu_sim
gcc -O2 -I../../../app_program/subsystem4 -o imu_sim imu_sim.c ../../../app_program/subsystem4/imu_logic.c -lm
./imu_sim --fake                 # synthetic hump, knock, slope
./imu_sim --g bench.csv          # replay a bench CSV log

gcc -O2 -I../../../app_program/subsystem4 -o math_check math_check.c -lm
./math_check                     # S4-U13
```

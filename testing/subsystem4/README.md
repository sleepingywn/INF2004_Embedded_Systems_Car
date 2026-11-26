# Subsystem 4 test tools

| Folder | What | Runs on |
|---|---|---|
| `imu_bench/` | Standalone GY-511 bench test: plain Pico SDK loop, keyboard commands, CSV log. **Not** part of the team firmware. | Pico (flash its own `.uf2`) |
| `checks/` | Lift detection, terrain packing and math tests | Laptop |
| `logs/` | Saved console output | |



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

## Run the unit tests

From the repository root, run `python testing/subsystem4/run_terrain_tests.py` with GCC on PATH.

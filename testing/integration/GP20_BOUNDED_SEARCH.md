# Bounded search with the existing sensor layout - 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Next trial: [GP20_GENTLE_STRADDLE_TRIAL.md](GP20_GENTLE_STRADDLE_TRIAL.md)
documents fresh line calibration, slower steering, 600 ms grace and controlled
pause restart. That 9DEF5D... image is flashed and stationary-verified; the behavior below
describes the earlier installed profile.

Status: **82ED61... track trial captured; paused-wheel encoder stop diagnosed.**
The current image (104EFF...) adds a one-count allowance during an intentional
pause. Its track trial had no encoder fault, but stopped on the 1500 ms search
timeout after a paused-wheel restart speed surge and loss of directional input. See
[GP20_PAUSED_ENCODER_SETTLING.md](GP20_PAUSED_ENCODER_SETTLING.md) for evidence,
bounds and the next physical check.

Five stationary readings with the user-confirmed line centered between GP27/28
reported fresh LINE_LOST while the sensors read white. OUTSIDE placement cannot
distinguish a centered line in that gap from an absent line. The user chose to
keep the hardware and accept some weaving from a bounded software search.
Evidence and prior faults are in [GP20_TIGHT_TURN.md](GP20_TIGHT_TURN.md).

This explicit GP20 trial adds `LINE_SEARCH=1` / `MOTION_LINE_SEARCH=1`.
Search is disabled by default, requires the GP20 circuit profile without
autostart, and requires OUTSIDE sensor mode. The build artifact gets a `_search`
suffix and the build profile stamp includes this flag, preventing reuse of
objects from the other profile. S3's detector and wire format are unchanged.

## Expected behavior

- After 300 ms without a directional reading of magnitude >=20, enter search.
- A fresh fault-free LINE_LOST with error 0 enters search immediately, including
  when GP20 starts a user-positioned centered car. LOST with nonzero error is
  inconsistent and remains rejected. No automatic run is started by search.
- If a tight-turn pause is active and directional evidence becomes insufficient,
  leave the pause once and search immediately rather than waiting for that
  mode's unseen timeout. Search clears pending tight-turn entry and never feeds
  synthetic error into it.
- First arc: 500 ms toward the last real detected side (left if none was seen).
  Second arc: up to 1000 ms toward the other side, sweeping back across the gap.
  Wheel targets are 250/550 counts/s, swapped with direction. Both remain forward
  and positive; normal target/PWM ramps apply. Exiting a previously paused wheel
  still uses its existing bounded restart behavior.
- Actual directional readings steer immediately during confirmation. A same-side
  magnitude >=20 held for 60 ms returns to normal tracking. Opposite or weak/zero
  readings restart only confirmation, never the search time or travel budget.
- Stop after 1500 ms of search, or if either wheel advances 1000 encoder counts
  from search entry. Those counts are a provisional travel bound, not calibrated
  millimeters. A successful reacquisition permits a later new search, always
  within the original 30-second run and total encoder-travel limit.

This deliberately replaces immediate response to a fresh, fault-free zero-error
LOST in the selected profile with an active bounded search. It does not pretend
that LOST is a confirmed centered reading. Freshness, epoch, missing input,
fault flags, calibration/junction states, invalid encoder transitions, backward
counts, implausible rate, per-wheel stalls, STOP and overall duration checks
still apply. Search itself does not pause or reverse wheels. Normal tight-turn
handling remains for strong actual curve readings.

The sweep is a trial with uncalibrated geometry: it may weave, fail to find the
line, or stop while centered if the gap is wider than its sweep reaches. The
separate earlier encoder fault remains unresolved; its detailed snapshot is
retained in this image. Do not treat successful software checks as track proof.

## Diagnostics and verification

`search=0` means normal tracking, `search=1` the first sweep, `search=2` the return
sweep. These fields appear in i and the stopped-run t history. During search,
`steer=+/-60` labels commanded direction; the search-specific 250/550 targets
are authoritative. A failed row contains pre-stop values; ordinary status shows
actual stopped outputs. LINE_STEP_SEARCH_TIMEOUT (11) maps to IR fault and
LINE_STEP_SEARCH_RANGE (12) to range fault. Other faults retain their codes.

All 28 host executables passed: 24 preceding-profile regressions plus explicit
search-profile tests for the real S3 centered-LOST input, both arcs, actual-side
priority, reacquisition confirmation, noise without deadline renewal, time/count
bounds, wrap/reset, tight-pause transition and unchanged fault guards. Actual
owner tests cover search admission, reacquisition/repeated search, STOP, stale
input, encoder fault, junction, external fault and the 30-second completion
boundary. Search-profile console and IR handoff regressions also pass.

Full firmware build had no compiler warnings/errors. Text/data/BSS:
395472/184/90068 bytes. Build command from `build_make`, with the existing SDK
toolchain on PATH:

```sh
make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=1 PICO_SDK_PATH=C:/Users/joell/.pico-sdk/sdk/2.3.1 E2U=tools/elf2uf2.exe
```

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_search.uf2`.
SHA256: `82ED616E20E3425F31FE6AF0CFECE406C1C12B2D671CCEC0CE4715AC1D5F5A70`.
Installed 3E2F79... backup:
`tools/dashboard/.local/gp20-encoder-reason-installed-3e2f79.uf2`.
Logs: `tools/dashboard/.local/gp20-line-search-validation.log` and
`tools/dashboard/.local/gp20-line-search-build.log`.

No hardware command or flash was performed during implementation. Prompt for
readiness before flashing/testing. Verify the SEARCH PROFILE banner and stopped
GP20 startup, then run one controlled track trial. Preserve power afterward and
retrieve i/t, including encoder diagnostics if present, before reset/new run.

## Authorized flash and stationary verification

User replied ready to flash. Verified the sole RPI-RP2 volume, Board-ID RPI-RP2,
and exact 82ED61... hash of the `_line30_button_search.uf2` artifact, then copied
gp20_line_search.uf2. Evidence:
`tools/dashboard/.local/gp20-line-search-20261103-055043.{log,json}`.
The h reply explicitly confirmed SEARCH PROFILE and its 1500-ms/1000-count limit.
Six i replies confirmed IDLE/COMPLETED, faults 0, epoch 3/id 1, raw valid,
zero counts/duties, GP20 released and armed. IR READY/running with advancing
samples and no acquisition/handoff errors. Four LED samples were armed mode 1;
two were the known transient observer mode 4 despite fault-free motion.
Only h/i were sent; no start, STOP, RESET, calibration or software-BOOTSEL.
This verifies stationary startup, not physical reacquisition or curve success.

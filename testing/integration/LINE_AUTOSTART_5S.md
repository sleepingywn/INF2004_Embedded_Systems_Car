# Five-second power-on start, 30-second circuit trial

User replaced the GP20 start workflow with: "just do a 5 second delay when turn
on". The old button permission was first disarmed with an acknowledged STOP.
The new explicit `LINE_AUTOSTART=1` profile starts one bounded line run after
approximately five seconds of boot uptime, without USB or a button start.

## Using the flashed image

Disconnect USB and turn battery power off so the Pico actually powers down.
Position the car on the circuit, with one line eye overlapping a black edge.
Turn battery power on and keep clear: the firmware attempts one run at five
seconds, for up to 30 seconds at the existing 20-percent duty cap. GP20 remains
an optional STOP/cancel button, not a start button. Power off also stops the car.

`x`/space cancels/stops over USB. `r`/`g` are disabled and cancel automatic start;
`c`/`f` also cancel it before manual calibration. After completion, rejection,
fault or cancellation, it will not try again until a reboot/power cycle.
Connecting USB to an otherwise unpowered Pico also boots it and starts this
countdown. Toggling the battery while USB still powers the Pico is not a reboot.

At four seconds the controller prepares a normal healthy RESET. At five seconds
it requires fresh healthy IR, fresh valid encoder status and the completed RESET
with the expected request ID/epoch. Actual motion admission also requires a
fresh usable line. It does not bypass the existing source, stall, line-loss,
junction, timing or encoder fault stops. If startup missed the timing window,
RESET failed or the input is unusable, it stays stopped without a late retry.
Polling is nominally 100 ms, so five seconds is not an exact hardware deadline;
the upper admission window is 5.5 s. Motion remains bounded by its own 30 s TTL.

The OUTSIDE-mode two-second unseen-line timeout is unchanged: both eyes entirely
over white for too long produces LOST, including while parked over a centered
line. Both-black junctions also stop the trial. No route selection, search,
barcode turns, metric calibration or independent hardware watchdog was added.

## Saved startup sensor references

Only this profile loads `config/line_boot_calibration.h` at IR initialization.
Values are white maxima and black minima from the measured 18:45/18:47 captures
on 2026-11-02, at normal operating height:

| Pin | White | Black |
| --- | ---: | ---: |
| GP26 | 183 | 2985 |
| GP27 | 196 | 2575 |
| GP28 | 192 | 2345 |

These are captured stationary endpoints, **not** the exact prior in-RAM min/max
accumulator, which also included manual transitions. Accordingly this changes
normalization relative to that earlier accumulator; physical steering with these
startup references still needs observation. Height, lighting or surface changes
may require recalibration. References are compiled into the image, not written
to flash by the running RTOS. Manual calibration is still RAM-only and does not
overwrite these boot defaults. The startup health source stays asserted until
fresh acquisition and bridge handoff succeed; loading endpoints alone does not
assert health. A boot using saved references reports calibration_samples=0.

## Build and checks

```text
make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=1 PICO_SDK_PATH=C:/Users/joell/.pico-sdk/sdk/2.3.1 E2U=tools/elf2uf2.exe
```

Image: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_auto5.uf2`

SHA256: `FAEA157195BD045C9CA895C42BFD1D61BD3F851F9474F1A71D70BDDE808A69C1`

Size: text 388620, data 176, BSS 85604. No build warnings/errors. Ignored build
log: `tools/dashboard/.local/line30-auto5-build.log`. Profile cache key and image
name distinguish it from the previous button and short USB profiles.

Host tests passed: actual console reset/start at 4/5 seconds, no early/late or
repeated start, missing reset acknowledgement, hardware/IR/epoch failures,
STOP/calibration cancellation, GP20 stop-only behavior; captured white/black
normalization through actual S3 sensor code; full 30-second line controller
across timer wrap; 13 motion owner-loop stop/guard/expiry scenarios. Existing
button and 2-second console tests still pass. No Buddy4 source changes; S3 has
only the guarded boot-reference initialization for this extension.

## Flash and observed startup - 2026-11-02 19:02 SGT

User confirmed "ready wheels lifted" and BOOTSEL. Verified RP2 board identity
and the hash above, flashed, and the Pico returned as COM6 / MTK3PICO.

The host monitor initially checked the banner too early (the diagnostic task
begins around 3 s), recorded "Wrong firmware profile", and sent STOP. The same
log subsequently showed the correct AUTO CIRCUIT banner. STOP was processed at
about 3.3 s, cancelling this boot before automatic RESET/start; no motor run was
observed. All captured duties and encoder counts remained zero/stable. This
checks early cancellation and startup, **not a successful hardware autostart**.
The monitor's banner wait has been corrected for future checks.

Saved IR references loaded successfully: READY/running, failed_channels=0,
calibration_samples=0, no acquisition/handoff fault and live sources=0. A separate
read-only confirmation at 19:04 saw the correct banner, AUTO phase 3 (cancelled),
three stopped snapshots, and IR READY. Current boot is stopped and cancelled;
the next power cycle enables a new automatic attempt. No circuit performance
or 30-second physical run is claimed yet.

Evidence (ignored): `auto5-boot-20261102-190252.{json,log}` and
`auto5-confirm-20261102-190411.{json,log}` under `tools/dashboard/.local`.

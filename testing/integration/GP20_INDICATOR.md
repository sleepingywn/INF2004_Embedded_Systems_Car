# GP20 activation indicator — 2026-11-03

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

A [smoother steering revision](GP20_SMOOTH_STEERING.md) is now installed after
a jerking/early-stop report. The records below describe the earlier indicator-only
revision; the LED behavior is retained.

Implemented, software-validated and flashed on 2026-11-03. Stationary
arming and the slow blink were physically confirmed. The rejected-start fast blink was subsequently confirmed. Powered acceptance
and runtime-fault patterns have not yet been physically tested.
Indicator-only image SHA256 (subsequently replaced):
`3E3EF6FF1C438CD60EA32E11B6D92DA69A42CB009BA1F55ECB685BDF94881762`.

The new image uses the small green LED on the Pico W module, to the left of its
micro-USB connector. It is WL_GPIO0 on the wireless chip, not RP2040 GP0/GP25,
not the carrier power light and not its RGB LEDs. No additional wiring is needed.
See the [Raspberry Pi wireless board layout](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html#wireless-board-layout).

| LED pattern | Meaning |
| --- | --- |
| Slow blink: 0.5 s on / 0.5 s off | GP20 armed; waiting for a deliberate press/release |
| Two short pulses each second | Run request queued, not yet confirmed accepted |
| Steady on | Matching GP20 request is accepted and the motion controller is tracing |
| Fast blink: 0.1 s on / 0.1 s off | Start/reset rejected or timed out, motion fault, or status unavailable/stale |
| Off | Starting up, completed, explicitly stopped, or disarmed |

Slow blinking confirms button permission, **not correct placement over the line**.
The existing line admission checks still apply; starting over all white after
its unseen-line timeout can be rejected. A held-at-arm GP20 still needs release,
then a fresh press/release. One run is at most 30 seconds with the existing
20% duty cap. The LED neither submits nor bypasses motor requests.

Steady on requires a fresh motion snapshot (at most 200 ms old), the exact
request ID and request epoch, current motion epoch, ACCEPTED result and TRACING
state, with no motion faults. Enqueue success alone cannot produce steady on.
A stopped/rejected command, stale snapshot, or different epoch cannot claim a
running request. Explicit GP20/USB STOP and calibration/rearm clear the previous
indicator state. Console `i` also prints the logical LED mode (0..4).

The diagnostic task updates the indication every nominal 100 ms. It publishes
one aligned 32-bit desired level/timestamp word; only the existing core-0 CYW43
service calls `cyw43_gpio_set`. The service limits LED update/retry attempts to
one per 100 ms and expires a producer request after 500 ms. This avoids adding
SPI calls, sleeps or busy waits to the motor-control task. The radio must
initialize successfully, but Wi-Fi association, MQTT and the laptop are not
required. Scheduling/radio delays can distort blink timing; the indicator is
not an independent watchdog or proof that the wheels physically moved.

The indicator is active in the GP20 battery circuit profile only. Default
motion gating, legacy autostart and USB 2s controls are unchanged. S1's GP19
heartbeat, Buddy 3 sensor logic, Buddy 4 code, telemetry format and wiring are
unchanged by this indicator addition.

## Validation and next flash

18 host executables passed: previous default/USB/button/auto motion-owner and
console checks, line/controller/bench regressions, BOOTSEL guard, plus indicator
state/pattern/mailbox-wrap checks and actual GP20 console-to-LED publication.
Cases include queued-but-not-accepted, wrong request ID/epoch, rejected request,
active fault, stale status, completion, explicit STOP, lease expiry and wrap.
Tests use fake hardware; no physical motor or LED test was run.

The GP20 build completed with no compiler warnings/errors. Text 390828,
data 184, BSS 85644 bytes. Logs: ignored local files
`tools/dashboard/.local/gp20-led-validation.log` and `gp20-led-build.log`.

Build flags: `SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 LINE_DEMO=1
LINE_CIRCUIT=1 LINE_AUTOSTART=0`.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button.uf2`

SHA256: `3E3EF6FF1C438CD60EA32E11B6D92DA69A42CB009BA1F55ECB685BDF94881762`.

Before the authorized next flash, prompt the user for readiness. Use the proven
manual recovery sequence: Robo Pico main switch ON, USB connected, wheels lifted;
hold Pico BOOTSEL, tap/release carrier RESET, then release BOOTSEL. Verify RPI-RP2
and the artifact hash before copying. Do not repeat the experimental software
reboot during this indicator validation. Start with a stationary check of the
slow blink and mode=1; powered start/solid-on validation needs explicit readiness.

## Flash and stationary result

User explicitly replied "ready to flash" after the BOOTSEL+RESET readiness
instructions. Verified E: RPI-RP2 Board-ID and the 3E3EF6F... UF2 hash, copied
`gp20_indicator.uf2`, and opened only the identified MTK3PICO serial console.

Evidence: `tools/dashboard/.local/gp20-led-20261103-030004.{json,log}`.
Commands were h and six i requests only; no motor start, STOP, reset, calibration
or software-BOOTSEL command. Six motion snapshots: IDLE/COMPLETED, faults=0,
epoch=3/id=1, raw_valid=1, counts=0/0, electrical duties=0/0. Six button snapshots:
released, armed=1, consumed=0, reset_pending=0. Six LED mode samples were 1
(armed). IR remained READY/running with acquisition faults=0.

User visually confirmed: "Yes, it's blinking slowly." The final observed state
is stationary and armed for a deliberate GP20 press/release. This confirms the
onboard LED path without Wi-Fi being needed for the visual check; it does not
validate the solid-on indication during actual motion or the new curve tuning.

## First GP20 press: rejected with line absent

User reported fast blinking. Read-only h-free diagnostics (five i commands)
were saved in `tools/dashboard/.local/gp20-fast-blink-20261103-030220.{json,log}`.
LED mode=4; GP20 released, armed=0, consumed=1. Motion IDLE, result=6
(REJECTED), faults=0, epoch=3/id=2, raw counts=0/0, duties=0/0, line active=0.
IR was READY/running, fault sources and handoff errors zero, with fresh
LINE_LOST (4) samples and both line raw values near 200 (white references).
This confirms the button request reached the owner and the error indication
matched rejection; no motor movement was observed in the encoder snapshots.
Current line absence is the evident issue; the exact admission-time IR sample
was not recorded. Reposition over a black edge, verify fresh usable line data,
then explicitly rearm before another GP20 press. No reset/rearm/start command
was sent during this read-only diagnosis.

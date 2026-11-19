# Dashboard Start/Stop - 11 November 2026

This implementation adds a separate command/status channel to the existing telemetry dashboard. Barcode decoding already exists in the route firmware and USB log; the later telemetry-v2 update adds decoded scan history to the dashboard. Current manoeuvre state is still not transmitted. See the Last barcode section in README.md.

## Operator behaviour

- Start is available only with fresh compatible control status and an armed, idle car. It requests the existing V5 line-following run; it is not manual steering.
- Start consumes the same one-shot permission as GP20. After stopping or finishing, rearm using USB `r` with the car stopped, or power-cycle and wait for healthy startup. Remote fault clearing/reset is intentionally not provided.
- Stop requests the existing persistent motor STOP path, including a GP20-started run. The dashboard displays Stopped only after the motor owner confirms a changed epoch, idle/error state and zero commanded duty. This confirms outputs, not mechanical braking.
- GP20 remains a local stop during a remote run. Motor requests are marked `MOTION_NETWORK`; only line following is permitted from this source in the opt-in build. Remote velocity, distance, turn and reset requests remain disabled.
- The initiating browser sends a keepalive every 400 ms. Other open viewers cannot renew its run. Closing, refreshing or hiding that browser stops renewal (hiding also requests Stop). No automatic restart occurs after reconnect.
- The motor owner checks a 5000 ms lease and a 30 s total remote-run bound every control iteration, independently of the console and WiFi tasks. Barcode segment handovers cannot reset this total limit. Existing line/IR/motor protection remains active.
- All users allowed to access this private dashboard can request Start/Stop. The viewer identifier separates keepalive ownership; it is not authentication. Authentication is the existing private dashboard access and authenticated MQTT broker ACL.

## Build and installation

Use the existing SDK/toolchain setup, adding `LINE_REMOTE=1`:

```sh
make -C build_make -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 \
  LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 \
  LINE_ADAPTIVE=1 LINE_ROUTE=1 LINE_REMOTE=1
```

Output: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_adaptive_route_remote.uf2`.
`LINE_REMOTE` defaults to 0 and requires MQTT and the GP20 circuit build without autostart. The firmware banner contains `REMOTE v1` when enabled. Older firmware continues supplying telemetry; the new buttons remain disabled without fresh control status.

Apply the narrowly scoped command/status topics in `acl.example` to the local broker ACL, preserving user names and existing permissions. Rebuild the dashboard with `npm run build`, restart Node-RED and refresh the page. Keep the dashboard on the existing loopback/private access path. Never publish test commands to the production broker.

## Protocol

All fields are explicitly little endian; C structures are never serialized. Topics use `inf2004/se08/car01/v1/control/`:

| Topic | Direction | Format |
| --- | --- | --- |
| `command` | Dashboard to car, QoS 1, **not retained** | 24 bytes: `RC`, version 1, opcode byte; boot ID, connection generation, command ID, issue uptime and motion epoch (five uint32 values) |
| `status` | Car to dashboard, QoS 0, not retained, every 250 ms | 36 bytes: `RS`, version 1, flags; boot ID, connection generation, sequence, uptime, epoch, acknowledgement ID; result/state/fault/op bytes; remote owner ID |

Opcodes: 1 Start, 2 Stop, 3 Keepalive. Results: 0 none, 1 queued, 2 running, 3 rejected, 4 stopped, 5 run ended, 6 lease/disconnection stop, 7 acknowledgement timeout. Flags: 1 supported, 2 armed, 4 remote active, 8 fresh motor status, 16 MQTT connected.

Commands require the current boot/connection generation and issue time no more than 1000 ms old. Start/Keepalive also require the current motion epoch. A fresh Stop can act without a readable motor snapshot; confirmation waits for a later snapshot. Start/Stop IDs increase within the connection, and duplicate IDs do not replay. Keepalives retain the Start ID and require strictly increasing issue timestamps. A reconnect changes the generation and drops pending commands. Retained commands from a previous connection cannot pass the generation check. Malformed/expired/duplicate commands can be silently ignored: the UI then reports no acknowledgement, not success.

The WiFi task only stages commands and publishes status. The diagnostic/GP20 owner validates and submits the motor request. A one-entry mailbox reserves priority for Stop. The motor owner independently enforces the lease; a failed remote submission cannot attach a watchdog to a later local run.

## Source map

- `app_program/subsystem1/control_wire.h`: binary contract and time checks.
- `telemetry_transport.c`: bounded command/status slots under the existing mutex.
- `subsystem2/line_console.c`: shared GP20 permission, command acknowledgement and lease renewal.
- `subsystem2/motion.c`: guarded network admission and motor-owner lease enforcement.
- `lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c`: fragmented receive handling, command subscription and periodic status.
- `tools/dashboard/control.js`: server-side freshness, IDs, ownership and binary encoding.
- `tools/dashboard/web/remote-control.js`: buttons, acknowledgement display, per-view renewal.
- `overview-template.vue` and `flows.json`: same-origin frame bridge and MQTT wiring.

## Validation and remaining hardware work

Run `python testing/subsystem2/run_remote_tests.py`, `python testing/subsystem2/run_route_tests.py`, `npm --prefix tools/dashboard test` and `npm --prefix tools/dashboard run build`.

Software evidence: tests exercise the actual console arbiter and actual motor owner, including held GP20, unhealthy IR, stale/duplicate commands, wrong keepalive owner, Stop confirmation, lost connection/reconnect and the total time bound across barcode handover. Isolated Node-RED on 1881 and a loopback-only broker on 1885 verified a real button click through MQTT to a simulated car, keepalives, Running acknowledgement, Stop and Stopped acknowledgement. No simulation packets are sent to port 1884.

Installed on the robot on 11 November 2026 with wheels lifted and GP20 unpressed. The REMOTE v1 and ROUTE banners, six zero-output/fault-free stationary snapshots, and live dashboard control readiness were verified. Image SHA-256: `7779DDDCA699E0D38D53E284B0CB2749F8090609FBF5CF244F96444FCA0A281E`. Local evidence: `.local/remote-control-flash-20261111.json` and `.local/gp20-adaptive-v5-stationary-20261111-173105.{json,log}`. No real motor Start command was sent.

Physical movement validation remains: have the operator authorize a supervised run. Check Start, dashboard Stop, GP20 override, browser/network loss and no restart after reconnect. Stationary telemetry alone does not prove remote stopping on the real car. Preserve stopped-run evidence before reset/flash.


## A–Z mapping extension

Start/Stop/Keep remain RC version 1, 24 bytes. Mapping save is RC version 2,
54 bytes, opcode 4: the same 24-byte header, expected revision u32 at offset 24,
then 26 action bytes at 28 (0 ignore, 1 right, 2 left, 3 straight 100 mm, 4 U-turn).
Exact length, action range, boot/session, epoch, freshness and newer command ID
are validated. Expected revision prevents overwriting changes from another view.
STOP retains mailbox priority. Status flag 32 advertises live mapping edits.
During line tracing and reacquisition the console publishes the new RAM revision.
A decoded letter selects its action once, so an already armed junction keeps its
action. Further edits replace the pending table. At the first idle or fault stop,
the console confirms a STOP epoch with zero duty before rebooting to save. Keep
power on until persisted readback. The save reboot clears RAM run history.

Status RS version 2 extends the 36-byte v1 prefix to 68 bytes: revision u32 at 36,
26 actions at 40, persisted flag at 66, save-pending flag at 67 (both 0/1).
Dashboard accepts v1/v2. Persisted means a CRC-valid flash slot was loaded.
The UI waits for matching persisted data after a boot/revision change, not merely
an MQTT send or queued acknowledgement. After 45 s without confirmation, inspect
the reported map before retrying. The car can have saved even if the link failed.

`route_settings.c` uses scratch registers 0–3 to carry a packed, checksummed save
request through a watchdog reset. Scratch4=0 requests normal flash boot. On early
boot, after data initialization with IRQs disabled and before the kernel launches
core 1 or starts USB/WiFi/DMA, it writes the inactive settings sector. Only the
small commit function runs while XIP is disabled; it and all input buffers are in
SRAM, with flash ROM routines and a RAM copy of boot2 restoring XIP. Runtime SMP
flash lockout is deliberately unnecessary. The linker reserves 0x1fe000–0x1fffff
and asserts both text and the data load image cannot overlap it. Inspect the map
and disassembly when changing the startup/linker path or compiler settings.

Checks: `python testing/subsystem2/run_remote_tests.py` includes mapping journal,
command framing and actual console/motion-owner coverage. `npm test` includes
mapping protocol validation. Run route and barcode suites for regressions.
Hardware installation and a real cold power cycle remain to be verified.


## GP20 repeat-run update (12 November 2026)

After a previous run has stopped, the console waits for at least 600 ms of fresh,
healthy, zero-output stopped status before offering GP20 retry (slow blink).
A new debounced press/release queues RESET, waits for its exact epoch/request
acknowledgement, and then queues a local line-follow run. There is no automatic
restart. Holding the stop button or releasing it does not count as a new press.
STOP and a new GP20 press during the pending reset cancel the restart.

The stopped-run history is preserved until the user requests retry. A retry
clears that history as part of RESET. Line-loss, route timeout and range stops
can be retried after repositioning if the live sensors are healthy. Encoder,
stall, hardware, handoff, external-source faults and stale/unhealthy IR block
GP20 retry. USB reset remains available for diagnosis/recovery. Configuration
save, BOOTSEL, calibration and active remote transactions do not auto-rearm.
Dashboard Start cannot consume the physical retry gesture; GP20 explicitly
starts the next local run. Existing remote lease and total-run limits remain.

Host tests exercise fresh/held gestures, reset acknowledgement, timeout and STOP
cancellation, recoverable line loss, fatal faults and mapping-save exclusion.
This update is built but has not yet been flashed or physically tested.


## No run-time cutoff profile (12 November 2026)

The latest requested build uses `LINE_UNLIMITED=1` with digital barcode input and
GP20 retry. This disables the active line-follow request's duration cutoff, the
V5 elapsed-run cutoff and the remote total-run cutoff. The dashboard keeps
renewing an owned run beyond 30 seconds while status is fresh. Admission still
rejects stale commands; other finite motion commands retain their TTLs.
The 5 s remote heartbeat lease, STOP, sensor/encoder/stall checks, individual
manoeuvre timeouts and cumulative line distance guards remain active.

The default build option remains `LINE_UNLIMITED=0` for legacy bounded profiles;
the new image name contains `_no_time_limit_`. Rebuild using the same digital
remote route flags plus `LINE_UNLIMITED=1`. Host tests run the actual owner beyond
60 s in both local/remote modes and verify heartbeat loss still stops it. This
new image is built, not flashed. Restart Node-RED to load the updated control
module before testing a dashboard-started run beyond 30 s.

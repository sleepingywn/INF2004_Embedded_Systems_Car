# RoboCar local telemetry

**Taking over this dashboard? Read [TAKEOVER.md](TAKEOVER.md)** for setup, the source map, telemetry behaviour, validation evidence and remaining hardware checks.

The laptop hosts an authenticated Mosquitto instance on port **1884**, separate from the installer-created Windows service on 1883. Node-RED runs on localhost:1880 with its editor disabled. Tailscale Serve exposes the dashboard privately to authorised tailnet devices. Remote Start/Stop is available with the opt-in `LINE_REMOTE=1` firmware and matching broker permissions; see [REMOTE_CONTROL.md](REMOTE_CONTROL.md).

## Live 3D Overview

The Overview replaces the old readings table at the same `/dashboard/robot` URL. It retains the existing MQTT decoder, duplicate filtering, event handling and telemetry log. The model, Three.js bundle and styles are served locally at `/robocar/`; no CDN or internet connection is needed to render the car. `npm start` builds the browser assets and copies `overview-template.vue` into the flow before starting Node-RED. Run `npm run build` after editing the UI while managing the runtime yourself, then restart Node-RED and refresh the browser.

- Wheel rotation comes from each wheel's signed, wrap-safe encoder count difference. Left raw counts decrease in forward travel; right counts increase. Count rates use the source acquisition interval, not network arrival spacing. Calibrated mm/s is still unavailable while `MOTION_CALIBRATED=0`.
- Relative heading comes from differential wheel travel using the firmware's provisional **2774 counts/revolution, 204.2 mm circumference and 125 mm track width**. It is an estimate, not an IMU yaw or surveyed course position. It starts at zero on a new boot/motion epoch or new browser session. The model is kept centered so its orientation can be inspected.
- Pitch follows S4's positive-nose-up convention. S4's positive left-side roll maps to negative model Z rotation (the model's positive Z raises its right side). Confirm mounting/signs with a physical tilt check before relying on the direction of tilt. Invalid attitude holds the last pose and clears the numeric readout.
- Transitions interpolate for at most 150 ms to received poses; motion is never extrapolated. A 2-second receipt timeout freezes the model and clears live numbers. Invalid encoder samples, source gaps over 1 second, receipt gaps over 2 seconds and implausible count jumps establish a new baseline without inventing travel. The heading estimate is labeled interrupted after a gap. Boot/epoch changes reset the baseline and relative heading.
- Perspective, top, front and side views, drag-to-orbit, scroll-to-zoom, camera rotation buttons and Reset view are local display controls. Reduced-motion preference removes pose interpolation.
- Start/Stop requires fresh compatible remote-control firmware. Start uses the existing GP20 one-shot permission; Stop waits for motor acknowledgement. A remote run stops when its initiating browser stops renewing its lease. Camera/model interactions do not send motor commands. See [REMOTE_CONTROL.md](REMOTE_CONTROL.md) for installation and physical validation.
- Line error, terrain/hump values, readiness and faults show actual decoded fields. Individual IR darkness and current route state are unavailable. Version 2 telemetry adds the actual last barcode result; version 1 remains supported. Diagnostic details retain the last packet, including calibrated speeds and source timestamps.

Editable UI sources are `web/`, `pose.js` and `overview-template.vue`. `build-overview.js` produces `public/robocar/overview.js` and updates the flow template. The photo-based model and asset notes are in `public/robocar/`; its mesh dimensions are not used as calibration. Keep `THREE-LICENSE.txt` and the emitted `overview.js.LEGAL.txt` with the bundle. The SE08 identity and headings use locally bundled Chakra Petch; telemetry numbers use IBM Plex Mono. The build copies their pinned Latin WOFF2 files and font licenses into `public/robocar/fonts/` for offline use.

Run `npm test` for decoder, receiver, pose handling and frame-bridge checks. Software validation on 10 November 2026 included a separate loopback MQTT broker/Node-RED receiver (1885/1881), actual binary-frame delivery to the browser, unequal wheel rates producing a visible turn, IMU pose, camera views, and stale freezing. The adapter also processed 14,247 accepted records from the local historical telemetry log without non-finite poses. This does not establish a new physical driving or tilt-sign test.

## Running

From this directory, start Mosquitto with `"C:\Program Files\mosquitto\mosquitto.exe" -c .local/mosquitto.conf`, then run `npm start` in another terminal. Dependencies are pinned in package-lock.json; restore them using `npm ci --ignore-scripts` if needed. Keep the laptop awake.

Local dashboard: http://127.0.0.1:1880/dashboard/robot

Private sharing URL: use the HTTPS hostname reported by `tailscale serve status`, followed by `/dashboard/robot`. The hostname is specific to the laptop/tailnet.

Tailscale Serve was configured for localhost:1880 on HTTPS port 443. `tailscale serve status` shows the active route. Do not use Funnel for this private setup.

## Credentials and robot connection

Ignored `.local/accounts.json` holds generated robot/dashboard passwords; `.local/passwords` holds broker password hashes. `.local/acl` grants the robot publish access and the dashboard read access. `flows_cred.json` holds the receiver credentials and must remain ignored. Never commit these files or copy secrets into logs.

Configure the broker to listen on loopback and the laptop's private Wi-Fi IPv4 address on port 1884. That address is specific to the current Wi-Fi connection. Before using the Pico, connect the laptop and Pico to the chosen private Wi-Fi/hotspot, add a listener for that adapter's IPv4 address to `.local/mosquitto.conf`, and permit TCP 1884 from the private subnet in Windows Firewall. Do not bind the broker to the campus interface. Restart the project broker after changing listeners.

Fill ignored `config/wifi_secrets.h` with the Wi-Fi credentials, laptop private IPv4 address, port 1884 and the generated robot account. Create and configure that local file on each laptop; keep credentials out of version control. Reserve the laptop address or update it when the hotspot changes it.

## Data and limitations

Topics use `inf2004/se08/car01/v1`: `telemetry` and `events` carry explicitly little-endian binary frames: version 1 is 80 bytes, version 2 is 96 bytes with barcode history. `status` carries a one-byte heartbeat sequence that wraps after 255. See telemetry_wire.c and decode.js for offsets. C bitfield layouts are never sent over the wire.

S1 samples every 250 ms into one replaceable telemetry slot and an eight-event FIFO. Only the Wi-Fi task calls MQTT; QoS 1 acknowledgement removes queued records. Duplicate delivery is possible. Boot tags are probabilistic 32-bit ROSC-derived identifiers, not authentication. Snapshot components have independent acquisition times and validity bits.

Events contain source, type, time, count and value. Type 1 reports sampled motor fault/result changes; types 16–20 correspond to S4 event indices. Source counters and droppedEvents expose observed losses; this is not a lossless recorder of every intermediate motor state. Events are logged without refreshing live dashboard age. Logs are JSONL under ignored logs/; archive them between runs.

Data age on the dashboard is time since receipt. Source times and validity are available in diagnostic details. Invalid sensor values display as unavailable.

## Validation status

Passed: four decoder tests, six motor-logic tests, C-to-JavaScript wire interoperability, MQTT-enabled firmware compilation, authenticated simulated telemetry delivery, timestamped log creation, stale display indication and rejection of anonymous broker connections.

Also passed: private Tailscale browser access, broker restart followed by automatic Node-RED reconnect, and host queue checks for overflow, FIFO ordering, replacement, contention and stale acknowledgements.

Also passed: read-only dashboard publish ACL rejection, startup deadline/release/wrap policy tests, actual receiver duplicate/reboot/sequence-wrap checks, and authenticated access through the laptop Wi-Fi address from the laptop itself.

Configure the inbound firewall rule for the selected private Wi-Fi adapter/address and its private subnet on TCP 1884. Local firewall rules and credentials are not installed by cloning this repository.

Later validation received real Pico telemetry, exercised stationary timing/IR diagnostics and completed a user-observed 2-second line-following run. These do not establish full circuit performance or access from another Wi-Fi device. See `testing/integration/LINE_AUTOSTART_5S.md` and `testing/integration/STATIONARY_FAULT_INVESTIGATION.md` for the current evidence and limits. Motor/flash operations require explicit operator readiness; no such operation occurs when running the dashboard tests.

## Last barcode (telemetry version 2)

The Terrain card shows **Last barcode**, the decoded character, time since the scan and element count. `No scan yet` means no qualifying capture since boot; `Not decoded` means the latest capture failed decoding. A space character is shown as `(space)`. A disconnected dashboard retains the last result with a stale label. This is scan history, not a claim that the next junction manoeuvre is armed. Captures shorter than 9 elements remain ignored, matching the existing firmware log.

The motor owner copies the latest result into the existing status-mutex snapshot. S1 reads that snapshot without consuming the console event queue, retaining its last coherent historical copy if the lock is temporarily busy. Normal 250 ms telemetry repeats the latest scan, so reconnecting clients recover it. Several scans between telemetry ticks may coalesce into the newest one; this is not a lossless scan-event log. Reboot clears history; a motion stop/reset retains the last scan and its original epoch.

Version 2 preserves offsets 0-79 from version 1 (with version byte 2) and adds:

| Offset | Field |
| --- | --- |
| 80 | Decoded ASCII character; zero means no successful decode |
| 81 | Captured element count (0-63) |
| 82 | Flags: 1 route feature supported, 2 snapshot available, 4 scan seen |
| 83 | Reserved zero |
| 84 | uint32 scan sequence |
| 88 | uint32 scan timestamp in robot uptime milliseconds |
| 92 | uint32 motion epoch at the scan |

Topics retain their existing names. Custom clients must accept the 96-byte/version-2 payload; the updated dashboard also accepts 80-byte/version-1 frames and marks barcode data unavailable for them. The separate Start/Stop protocol is unchanged.

Validation: `python testing/subsystem1/run_barcode_telemetry_tests.py` checks the C encoder against the JavaScript decoder, the real motor-owner snapshot path and enlarged transport buffers. Dashboard tests cover malformed frames, all supported character classes, no/failed scans, stale links and reboot. An isolated broker/UI test displayed `A` with its scan age. A new physical barcode pass is still needed to validate sensing on the current setup.

Installed on the robot on 11 November 2026. Live version-2 telemetry reports barcode support and an available snapshot; the dashboard shows `No scan yet` before a capture. Six stationary snapshots confirmed stopped motor outputs and no motion/IR faults. Flash/evidence record: `.local/barcode-telemetry-flash-20261111.json`. No Start command or physical barcode pass was performed during this update.


## Editable barcode instructions (11 November 2026)

Open **Barcode instructions / A–Z settings** below Start/Stop. Each letter can
Ignore, Turn right, Turn left, Straight (100 mm), or U-turn. These are the existing
route manoeuvres, performed at the next junction; this does not add adjustable
angles, distances or speeds. The displayed Last barcode remains the scanned letter.
Default instructions are A=right, B/Z=left, C=straight, D=U-turn, others=ignore.

Apply instructions updates the robot's RAM table while it is tracing or idle.
A decode latches one action for the upcoming junction. Later edits take effect
on a subsequent scan when the route can accept an instruction. At the next
stop, the firmware waits for the motor owner's zero-output acknowledgement,
then reboots to save the table. Keep power on until the dashboard confirms the
stored values. Discard edits reloads the robot's current table.

The last 8 KB of the Pico W's 2 MB flash is reserved as two independent 4 KB
sectors. Versioned records contain a revision, all 26 actions and CRC32. Updates
write the inactive slot; an interrupted/corrupt write leaves the previous valid
slot available. With neither valid, firmware uses the defaults. No-op saves avoid
extra flash wear. Normal UF2 updates stay outside this reserved region; full-chip
erasure removes the settings. This layout targets the current 2 MB Pico W.

See [REMOTE_CONTROL.md](REMOTE_CONTROL.md) for the protocol and boot-time writer.
Software checks cover persistence fault injection, remapped route execution,
command validation, and a browser save/readback through an isolated MQTT broker.
A physical power-off/reconnect test is still required after installing this image.

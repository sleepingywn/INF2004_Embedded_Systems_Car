# RoboCar local telemetry

**Taking over this dashboard? Read [TAKEOVER.md](TAKEOVER.md)** for setup, the source map, telemetry behaviour, validation evidence and remaining hardware checks.

The laptop hosts an authenticated Mosquitto instance on port **1884**, separate from the installer-created Windows service on 1883. Node-RED runs on localhost:1880 with its editor disabled. Tailscale Serve exposes the dashboard privately to authorised tailnet devices. No remote driving commands are implemented.

## Live 3D Overview

The Overview replaces the old readings table at the same `/dashboard/robot` URL. It retains the existing MQTT decoder, duplicate filtering, event handling and telemetry log. The model, Three.js bundle and styles are served locally at `/robocar/`; no CDN or internet connection is needed to render the car. `npm start` builds the browser assets and copies `overview-template.vue` into the flow before starting Node-RED. Run `npm run build` after editing the UI while managing the runtime yourself, then restart Node-RED and refresh the browser.

- Wheel rotation comes from each wheel's signed, wrap-safe encoder count difference. Left raw counts decrease in forward travel; right counts increase. Count rates use the source acquisition interval, not network arrival spacing. Calibrated mm/s is still unavailable while `MOTION_CALIBRATED=0`.
- Relative heading comes from differential wheel travel using the firmware's provisional **2774 counts/revolution, 204.2 mm circumference and 125 mm track width**. It is an estimate, not an IMU yaw or surveyed course position. It starts at zero on a new boot/motion epoch or new browser session. The model is kept centered so its orientation can be inspected.
- Pitch follows S4's positive-nose-up convention. S4's positive left-side roll maps to negative model Z rotation (the model's positive Z raises its right side). Confirm mounting/signs with a physical tilt check before relying on the direction of tilt. Invalid attitude holds the last pose and clears the numeric readout.
- Transitions interpolate for at most 150 ms to received poses; motion is never extrapolated. A 2-second receipt timeout freezes the model and clears live numbers. Invalid encoder samples, source gaps over 1 second, receipt gaps over 2 seconds and implausible count jumps establish a new baseline without inventing travel. The heading estimate is labeled interrupted after a gap. Boot/epoch changes reset the baseline and relative heading.
- Perspective, top, front and side views, drag-to-orbit, scroll-to-zoom, camera rotation buttons and Reset view are local display controls. Reduced-motion preference removes pose interpolation.
- Start/Stop are deliberately disabled and direct the operator to GP20: the existing MQTT account is read-only and firmware has no remote command handler. No model/view interaction sends motor commands.
- Line error, terrain/hump values, readiness and faults show actual decoded fields. Individual IR darkness, barcode and route state are absent from this 80-byte protocol and are not fabricated. Diagnostic details retain the last packet, including calibrated speeds and source timestamps.

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

Topics use `inf2004/se08/car01/v1`: `telemetry` and `events` carry 80-byte, explicitly little-endian binary frames. `status` retains the existing text heartbeat. See telemetry_wire.c and decode.js for offsets. C bitfield layouts are never sent over the wire.

S1 samples every 250 ms into one replaceable telemetry slot and an eight-event FIFO. Only the Wi-Fi task calls MQTT; QoS 1 acknowledgement removes queued records. Duplicate delivery is possible. Boot tags are probabilistic 32-bit ROSC-derived identifiers, not authentication. Snapshot components have independent acquisition times and validity bits.

Events contain source, type, time, count and value. Type 1 reports sampled motor fault/result changes; types 16–20 correspond to S4 event indices. Source counters and droppedEvents expose observed losses; this is not a lossless recorder of every intermediate motor state. Events are logged without refreshing live dashboard age. Logs are JSONL under ignored logs/; archive them between runs.

Data age on the dashboard is time since receipt. Source times and validity are available in diagnostic details. Invalid sensor values display as unavailable.

## Validation status

Passed: four decoder tests, six motor-logic tests, C-to-JavaScript wire interoperability, MQTT-enabled firmware compilation, authenticated simulated telemetry delivery, timestamped log creation, stale display indication and rejection of anonymous broker connections.

Also passed: private Tailscale browser access, broker restart followed by automatic Node-RED reconnect, and host queue checks for overflow, FIFO ordering, replacement, contention and stale acknowledgements.

Also passed: read-only dashboard publish ACL rejection, startup deadline/release/wrap policy tests, actual receiver duplicate/reboot/sequence-wrap checks, and authenticated access through the laptop Wi-Fi address from the laptop itself.

Configure the inbound firewall rule for the selected private Wi-Fi adapter/address and its private subnet on TCP 1884. Local firewall rules and credentials are not installed by cloning this repository.

Later validation received real Pico telemetry, exercised stationary timing/IR diagnostics and completed a user-observed 2-second line-following run. These do not establish full circuit performance or access from another Wi-Fi device. See `testing/integration/LINE_AUTOSTART_5S.md` and `testing/integration/STATIONARY_FAULT_INVESTIGATION.md` for the current evidence and limits. Motor/flash operations require explicit operator readiness; no such operation occurs when running the dashboard tests.

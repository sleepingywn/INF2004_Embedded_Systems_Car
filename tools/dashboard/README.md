# RoboCar local telemetry

The laptop hosts an authenticated Mosquitto instance on port **1884**, separate from the installer-created Windows service on 1883. Node-RED runs on localhost:1880 with its editor disabled. Tailscale Serve exposes the dashboard privately to authorised tailnet devices. No remote driving commands are implemented.

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

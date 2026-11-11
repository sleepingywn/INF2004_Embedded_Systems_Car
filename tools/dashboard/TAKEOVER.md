# Dashboard takeover guide

Updated 11 November 2026. Start here if you are taking over the SE08 RoboCar dashboard.

## Scope and branch

This change replaces the original readings table with the live 3D Overview, camera controls, SE08 branding, and locally hosted fonts. It changes only `tools/dashboard/`. Firmware, the 80-byte wire format, broker permissions and motor behaviour are unchanged.

The dashboard PR targets `feature/robocar-telemetry-line-follow-autostart`, the existing telemetry branch in PR #3. It is separate from `feature/kernel-v5-barcode-route` and does not include its barcode/driving commits. Review or merge it into the telemetry branch first. If PR #3 is merged before this PR, rebase onto the branch that receives it and check that the PR diff still contains dashboard files only.

## Get it running

Requirements: Git, Node.js **22.9 or newer**, npm, and a browser with WebGL. The development laptop was checked with Node 25.8.0. Mosquitto is needed for live packets, but not to inspect the interface or run unit tests.

From the repository root:

```sh
cd tools/dashboard
npm ci --ignore-scripts
npm test
npm start
```

Install development dependencies too: `esbuild` is used by `npm start` through the `prestart` build. The build generates the browser bundle, copies the fonts and licenses, and embeds `overview-template.vue` in `flows.json`.

Open **http://127.0.0.1:1880/dashboard/robot**. A disconnected robot should produce “Waiting for telemetry”, unavailable readings and a dimmed stationary model. This is an expected state, not a failed 3D load. The UI, fonts and model work without internet after installation.

Check for an existing process listening on port 1880 before starting another instance. Stop a foreground instance with Ctrl+C. For a background instance, identify its process and stop that instance only. Restart Node-RED after flow/template/settings changes and refresh the browser after rebuilding assets. Do not start a second production receiver with the same MQTT client ID.

## Connect live telemetry

The receiver subscribes to `inf2004/se08/car01/v1/telemetry` and `/events` on the authenticated laptop broker at **127.0.0.1:1884**. Node-RED listens on loopback only. Authorized remote viewers use the existing private Tailscale Serve route; its hostname is machine-specific.

Obtain the existing connection configuration from the team through a private channel. The following files are intentionally ignored and are **not** supplied by this PR:

- `.local/mosquitto.conf`, `.local/acl`, `.local/passwords`: listeners, topic permissions and broker password hashes.
- `flows_cred.json`: the dashboard account used by the MQTT broker node with ID `broker`. Preserve that ID when editing the flow.
- Any credential encryption key/runtime configuration needed by that credentials file, or `ROBOCAR_CREDENTIAL_SECRET` if the team uses an explicit key. Do not replace the key independently of the credentials.
- `../../config/wifi_secrets.h`: firmware-side network settings, managed separately by the firmware owner.

Use the checked-in `mosquitto.conf.example` and `acl.example` as references when provisioning another laptop. The dashboard account is read-only. Keep credentials, private connection files and telemetry logs out of Git.

For the current Windows setup, start the configured broker from `tools/dashboard` in a separate terminal:

```powershell
& 'C:\Program Files\mosquitto\mosquitto.exe' -c .local/mosquitto.conf
```

The laptop and car must share the chosen private Wi-Fi/hotspot. Check the broker's private-adapter listener address after a network change, the firmware's configured broker address, and the private-subnet firewall rule. Do not substitute a campus/public listener to make a connection error disappear. See [README.md](README.md) for the established network setup.

On 11 November the dashboard UI was restarted successfully, but the broker was unavailable. No new live-car run was performed. A new maintainer must check current connectivity rather than treating that snapshot as current service status.

## Where to make changes

| File | Responsibility |
| --- | --- |
| `web/index.html` | Overview layout, labels and disabled vehicle controls |
| `web/overview.css` | Dark theme, responsive layout, Chakra Petch interface and IBM Plex Mono readings |
| `web/overview.js` | Three.js renderer, camera controls, packet bridge, UI freshness and pose interpolation |
| `pose.js` | Encoder signs, counter wrapping, count rates, heading estimate, reset/gap handling and IMU conversion |
| `overview-template.vue` | Node-RED message-to-iframe bridge and responsive iframe height |
| `decode.js` | Existing 80-byte binary protocol decoder |
| `flows.json` | MQTT subscriptions, duplicate filtering, event/log routing, UI configuration; template format is generated |
| `settings.js` | Loopback server, static asset serving and decoder registration |
| `build-overview.js` | Builds the renderer and synchronizes generated assets and the flow template |
| `public/robocar/robocar.glb` | Photo-based car model with separate `Wheel_Left` and `Wheel_Right` pivots |
| `public/robocar/MODEL-NOTES.txt` | Model coordinates, approximate dimensions and original asset provenance |

Edit the source files, then run `npm run build`. Include regenerated `public/robocar/` assets and `flows.json` in the same change. Do not hand-edit the generated browser bundle or the flow's template string. Keep the model, Three.js license and font licenses with the served assets. The original Blender source is a separate local asset, not included in this dashboard PR; request it from the asset owner for geometry edits.

## How real data drives the car

1. MQTT delivers binary frames to the existing Node-RED decoder. Events are logged but do not refresh the live display. Duplicate/out-of-order telemetry within a boot is filtered.
2. The Vue template copies the reactive payload into a plain object and sends it to the same-origin Overview iframe. Both sides check message origin and source. No browser control publishes MQTT commands.
3. `PoseTracker` derives per-wheel count rates from acquisition timestamps. Left raw counts decrease in forward travel; right raw counts increase. Wheel rotation follows observed count differences.
4. Heading is relative wheel odometry using the firmware's provisional 2774 counts/revolution, 204.2 mm circumference and 125 mm track width. It is **not a measured compass heading or course position**. Tests flag changes to these firmware constants so the visual adapter can be reviewed.
5. IMU pitch/roll drive body attitude with the S4-to-model sign conversion. New boot/motion epochs reset the relative pose. Refreshing a viewer also starts a new local heading reference.
6. The renderer interpolates toward observed poses for at most 150 ms. It does not continue predicted movement. After two seconds without fresh receipt, it freezes the pose and clears live numbers. Invalid counts, long gaps and implausible jumps rebaseline the estimate; missing travel is not invented.

### Deliberate limitations

- Start/Stop are disabled. GP20 remains the vehicle control. Remote control requires a separate firmware command/acknowledgement design, ownership review, permissions and physical validation; enabling the HTML buttons alone is not an implementation.
- Calibrated mm/s stays unavailable while firmware has `MOTION_CALIBRATED=0`. The visible wheel rates are counts/s.
- Individual IR darkness, barcode characters and route state are not transmitted in the current frame. The Overview displays line error and clearly marks unsupported values unavailable.
- Heading can drift from wheel slip and provisional geometry. The car is centered in the viewer; it is not plotted on a course map.
- Confirm physical IMU tilt direction and wheel direction with the hardware owner. Source-level sign matching and simulated packets do not replace a physical check.
- Receipt age uses the receiver timestamp and browser clock. Keep laptop/viewer clocks synchronized. This UI is not a safety controller.

## Validation and next checks

`npm test` covers 19 checks across decoder, receiver, pose adapter and Vue frame bridge: forwards/reverse, unequal wheels, count/time/sequence wrap, reboot/epoch resets, duplicate/event filtering, invalid/stale samples, gaps, firmware geometry, stopped wheels and cloneable Vue payloads.

Completed software checks on 10–11 November:

- Local build and real Node-RED page loading with bundled 3D model and fonts.
- Separate loopback test broker/receiver on ports 1885/1881 delivering binary frames through MQTT → decoder → UI → 3D view. Unequal wheel rates visibly turned the car; IMU values tilted it; stream loss froze it.
- Adapter replay of 14,247 accepted historical local telemetry records without non-finite poses.
- Camera presets and orbit controls; narrow and desktop layout checks; unavailable-data presentation.

No physical driving, fresh IMU sign check or new flash is claimed by this PR. Next: perform a stationary live-telemetry check on the private hotspot, then agree on a supervised wheel/tilt check with the operator. Preserve any existing stopped-run evidence before resetting the board.

When adding browser fixtures, use a separate broker/client ID/user directory and separate logs. Never publish test samples to the production telemetry topic on the production broker. The temporary test instances used during development were shut down and are not part of normal startup.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Connection refused at port 1880 | Node-RED process, startup log, Node version and dependency installation |
| UI loads but says waiting | Broker on 1884, MQTT credentials, private network/address and robot publication |
| Model is dimmed/readings show dashes | No fresh or valid data; inspect last packet and source validity under Diagnostic details |
| 3D unavailable while numbers work | Browser WebGL support and `/robocar/robocar.glb`; inspect console/network errors |
| Heading jumps/resets | Boot ID, motion epoch, refresh, sample gaps and encoder validity before changing visual math |
| Interface changes are missing | Rebuild, restart Node-RED for template changes, then refresh the browser |
| Port already in use | Identify the existing dashboard; avoid killing unrelated Node processes |

Keep changes under `tools/dashboard/` for dashboard-only work. Run the build, tests and `git diff --check` before handing it on. Review staged paths so logs, credentials and unrelated firmware edits never enter the dashboard PR.

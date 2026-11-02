# Buddy 1 / Buddy 2 software integration review

Historical software-stage snapshot. Later hardware observations and the current flashed profile are recorded in [LINE_AUTOSTART_5S.md](LINE_AUTOSTART_5S.md) and [STATIONARY_FAULT_INVESTIGATION.md](STATIONARY_FAULT_INVESTIGATION.md).

Date: 2026-11-02. Working-tree changes; not committed or pushed. User approved software validation only. No flashing or motor operation performed.

| Requirement | Implementation and evidence | Remaining limitation |
|---|---|---|
| Binary telemetry | Explicit 80-byte little-endian encoder and decoder; C fixture decoded correctly by Node.js | Fixed format is compact relative to JSON, not an optimal bitstream encoding |
| Single network owner | MQTT calls remain in mqtt_utk.c callbacks running on the Wi-Fi task | Hardware timing under radio load unmeasured |
| Bounded handoff | One latest snapshot, eight-event FIFO, polling mutex; host tests passed overflow, FIFO, replacement, contention and acknowledgement cases | Mock mutex tests do not prove SMP scheduling |
| Event reporting | Motor sampled transitions and S4 counter/time/value records; source gaps and insertion failures counted | Fast intermediate motor transitions may be missed between samples |
| Retry and reconnect | Pending record removed after QoS 1 completion; retry guard prevents poll loop bypass; Node-RED reconnected after broker restart | Pico reconnect not exercised on hardware |
| Snapshot validity | Raw encoder availability separated from calibrated physical values; observers poll separate S2 status mutex | Calibration remains disabled pending measured parameters |
| Startup protection | Explicit app-ready gate, motors stopped beforehand, timeout latched; release/deadline/wrap policy tests passed | Real startup/scheduler fault injection pending hardware permission |
| S4 odometry consumer | Acquisition time, freshness, sequence and epoch checks; baseline/history reset on loss | Real terrain measurements require revalidation |
| Development controls | Console off by default; high pulse limits require MOTION_DEVELOPMENT | Existing bench calibration still needed |
| Host tests | Six decoder/receiver checks; six existing motor-logic cases; encoder interoperability; queue suite; startup policy suite passed | No claim of exhaustive verification |
| Firmware build | MQTT-enabled build succeeded after latest edits; text 382304, data 176, BSS 85300 bytes | Wi-Fi fields configured; no secrets recorded here |
| Local dashboard | Browser empty state, simulated values, stale age and log creation verified | Real robot values not received |
| Broker access | Anonymous connection and dashboard publish rejected; robot authenticated through loopback and local Wi-Fi address | Firewall and credentials configured; other-device connectivity untested |
| Private sharing | Tailscale Serve configured; HTTPS dashboard opened in browser on this laptop | Another team device has not been checked |
| Secrets | Wi-Fi settings, account passwords, broker hashes and Node-RED credentials are Git-ignored | Local files must remain private |

The project broker uses port 1884 because the Mosquitto installer created a Windows service on 1883. Node-RED uses localhost:1880 with its editor/admin API disabled. Tailscale exposes the runtime privately; no Funnel is enabled.

Software implementation and local validation are complete. Wi-Fi credentials and the restricted inbound firewall rule are configured. Another-device and Pico checks remain hardware-stage validation and require separate approval. Shared-library and S4 changes remain proposals for team/instructor review under TEAM.md. These results do not substitute for that review.

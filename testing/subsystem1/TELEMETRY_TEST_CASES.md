# Buddy 1 final test cases

## B1-01 · Startup telemetry

**Procedure.** Connect the laptop to `antecon1`, start the project MQTT broker on port 1884 and open [the local dashboard](http://127.0.0.1:1880/dashboard/robot). Power on the robot with GP20 released. Watch diagnostics for 10 seconds after the first packet arrives.

**Pass criterion.** The dashboard shows live telemetry before anyone presses Start. Packet sequence numbers and Pico timestamps advance. Save the first and last records and note the longest gap between arrivals.

## B1-10 · Battery-only telemetry

**Procedure.** With battery and USB connected and the robot idle, record the boot ID, packet sequence, IMU timestamp and pitch. Disconnect USB while keeping battery power on. Tilt the nose up and return it to level while watching the dashboard for 10 seconds.

**Pass criterion.** The boot ID stays the same, packets and IMU timestamps keep advancing, and pitch changes with the tilt. Save the readings before and after disconnecting USB. If the test fails, record which value stopped updating.

## B1-07 · Telemetry packet validation

**Procedure.** From the repository root, run `node --test tools/dashboard/decode.test.js`. This laptop test uses simulated packets.

**Pass criterion.** The decoder rejects malformed frames and unsupported versions, reports unavailable readings as null, decodes the known pitch of −64° and roll of +63.5°, and requires calibration before reporting wheel speed.

**Recorded result.** All four checks passed on both runs on 26 November 2026 at dashboard commit `ee763d4`. [Terminal output](logs/B1-07_20261126_164433.txt) · [Repeat run in the team video at 0:20](https://youtu.be/IBY-M7DeD0M?t=20)

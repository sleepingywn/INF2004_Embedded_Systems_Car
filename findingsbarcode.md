# Barcode Pipeline Review & Findings

- **Date:** 18 November 2026
- **Branch / Commit Reviewed:** `feature/digital-barcode` @ `049de7f` (reverted back to `c2b6496` 1 kHz sampled `D0` digital barcode baseline; edge-IRQ experiment archived at `archive/digital-barcode-edge-capture` @ `43385d9`)
- **Host Test Baseline:** `testing/subsystem2/run_route_tests.py` and `testing/subsystem1/run_barcode_telemetry_tests.py` pass

---

## 1. Current Pipeline Overview & What Works Well

The current barcode subsystem cleanly respects the Tribunal 1 / μT-Kernel 3.0 dual-core architecture:

1. **1 kHz Hardware-Paced Sampling (`app_program/subsystem3/ir_acquisition.c`)**
   - Hardware `TIMER_ALARM3` triggers every `1000 µs` on Core 0.
   - With `BARCODE_DIGITAL=0`, ADC channels `0, 1, 2` (`GP26` barcode, `GP27` left line, `GP28` right line) are scanned in sequence.
   - With `BARCODE_DIGITAL=1`, `GP26` (`D0`) is sampled digitally at the 1 kHz deadline (`4095` for black, `0` for white) while `GP27`/`GP28` remain on ADC1/ADC2.
2. **Normalization & Epoch Fencing (`app_program/subsystem3/ir_runtime.c` & `app_program/subsystem2/motion_ir_bridge.c`)**
   - `ir_task` (Core 0, priority 16) drains the 64-entry ring buffer, enforces epoch and gap fences across `STOP`/`RESET`, updates line tracking, and feeds `bc_feed()` (`app_program/subsystem3/barcode_sense.c`).
   - With `LINE_ROUTE=1` (`BARCODE_WIDE`), barcode element widths use 16-bit counters (`uint16_t`) and a `1500 ms` quiet timeout (`BC_QUIET_MS`), avoiding 8-bit `255 ms` saturation on large barcodes.
3. **Bar/Space-Separated Decoding & Odometry Widths (`app_program/subsystem2/motion_route_decode.h`, `motion_route.h`)**
   - `route_code39_decode()` ranks the 5 bars (`1.5×` wide/narrow ratio) separately from the 4 spaces (`1.25×` wide/narrow ratio) in both forward and reverse scan directions. Classifying bars separately from spaces naturally tolerates ink spread and comparator threshold bias (where bars grow by `+Δ` and spaces shrink by `-Δ`).
   - `route_distance_widths()` converts 1 ms element timestamps into summed wheel-encoder counts using a 512-entry (`10.2 s`) odometry ring buffer.
4. **Junction Execution & Telemetry (`app_program/subsystem2/motion.c`, `route_settings.c`, `subsystem1/telemetry_collect.c`)**
   - Decoded letters map through flash-persisted `route_mapping[26]` (`A–Z` → Ignore, Right arc, Left arc, Straight 100 mm, or 180° U-turn) and arm one manoeuvre for the next `LINE_BOTH_BLACK` junction, carrying running motor duty back into V5 on arc exit (W1 fix).

---

## 2. Summary of Findings

| # | Severity | Area | Finding | Key Files |
|---|---|---|---|---|
| 1 | **High** | S3 Digital Capture | Zero glitch/debounce filtering on `D0` + immediate completion at 29 elements splits a single 1-ms comparator glitch into a failed 29-element scan plus a second spurious capture | `ir_acquisition.c`, `ir_runtime.c`, `barcode_sense.c` |
| 2 | **High** | S3 → S2 Handoff | Two rapid barcode completions overflow the single `shared.barcode` slot (`E_OBJ`), which `ir_runtime` treats as a **fatal hardware fault**—stopping the ADC and locking out GP20 retry | `motion.c`, `motion_ir_bridge.c`, `ir_runtime.c` |
| 3 | **Medium–High** | S2 Distance / Speed | Up to **19 ms** of wheel travel is chopped off the final bar(s) on mid-period barcode wakeups; V5 ignores `bc_active`; `route_choose()` lets speed-distorted `by_time` veto valid `by_distance` | `motion.c`, `motion_route.h`, `line_follow.h` |
| 4 | **Medium** | S2 Decoder | `route_code39_decode()` ignores all 4 spaces in both `*` sentinels and ignores the inter-character gaps (`widths[9]`, `widths[19]`) | `motion_route_decode.h` |
| 5 | **Medium–High** | S2 Junction Logic | Manoeuvres trigger **only** on `LINE_BOTH_BLACK`; `route.armed` never expires if a junction is missed; `150`/`300`-count exit lead (`~5°–10°`) exits onto the straight line on `+` crossroads | `motion.c`, `motion_route.h` |
| 6 | **Low–Medium** | Config & Logging | Compiled-in default `A`/`B` turn directions (`A` = Right, `B` = Left) are opposite to `README_MOTION.md`; `route_mapping_code()` overwrites scanned char before `ROUTE_EV_NOT_ARMED` log | `route_settings.c`, `motion.c`, `README_MOTION.md` |

---

## 3. Detailed Technical Findings

### Finding 1 — Digital `D0` Mode Has Zero Glitch Filtering + Immediate 29-Element Cutoff (High)

- **Locations:**
  - `app_program/subsystem3/ir_acquisition.c:121-125`
  - `app_program/subsystem3/ir_runtime.c:209-211`
  - `app_program/subsystem3/barcode_sense.c:68-102`
- **Mechanism:**
  - In `BARCODE_DIGITAL=1`, `GP26` is converted directly to synthetic `4095` / `0` and then normalized to `100` / `0`. Because the input to `bc_feed()` is strictly binary (`0` or `100`), the hysteresis band in `barcode_sense.c:68` (`BC_BAR_ON = 60`, `BC_BAR_OFF = 35`) has **zero effect**.
  - At the same time, `barcode_sense.c:98-102` finishes a capture **immediately on the very first 1-ms white sample** when `c->count == BC_EXPECTED_ELEMS` (`29`), without confirming a trailing white quiet zone:
    ```c
    if (BC_EXPECTED_ELEMS != 0u && c->count == BC_EXPECTED_ELEMS && !c->overflow) {
        return bc_finish(c, out);
    }
    ```
- **Failure Mode on the Car:**
  1. LM393-style IR `D0` comparators commonly chatter for 1–2 ms on blurred ink edges, paper sheen, or chassis vibration over the hump/mat.
  2. A **single 1-ms glitch** mid-barcode inserts 2 extra elements (`+1` glitch pulse, `+1` split element), shifting all subsequent element indices.
  3. Capture terminates early at element `29` **while the sensor is still physically crossing the stop `*` character**, causing `route_code39_decode()` to fail (`0`).
  4. On the next black sample (1 ms later), the remaining real bars of that same stop `*` immediately trigger `if (bar) { c->active = true; ... }` and start a **second zombie capture**:
     - If `< 5` elements remain, `c->active` stays open for `1500 ms` (`BC_QUIET_MS`), merging with any junction line or paper edge encountered within the next 1.5 s.
     - If `>= 5` elements remain, 1.5 s later it publishes a failed short scan (e.g., `5 / 29 elements`) that **overwrites `barcode_status` on the dashboard** (`Not decoded · 5 / 29 elements`), hiding the actual 29-element capture failure.
- **Recommended Fix:**
  - Add a small minimum-run / glitch filter (e.g., 2–3 ms sample debounce or merging 1-ms blips back into the surrounding run in digital mode) **and** wait for a short confirmed white quiet zone (e.g., 20–40 ms or equivalent wheel counts) before finalizing a 29-element capture instead of cutting off on a single 1-ms white sample.

---

### Finding 2 — Back-to-Back Barcode Completions Trigger a Fatal Subsystem 3 Shutdown (High)

- **Locations:**
  - `app_program/subsystem2/motion.c:634-638` (`motion_publish_barcode`)
  - `app_program/subsystem2/motion_ir_bridge.c:88-94` (`motion_ir_bridge_sample`)
  - `app_program/subsystem3/ir_runtime.c:238-247` (`process_frame` → `runtime_fault`)
  - `app_program/subsystem3/ir_acquisition.c:210-213` (`GPIO(26)` pull configuration)
- **Mechanism:**
  - `motion_publish_barcode()` buffers at most **one** unread barcode (`shared.have_barcode`). If `shared.have_barcode` is still `true` when another capture finishes, it returns `E_OBJ` (`< E_OK`).
  - `motion_ir_bridge.c:93` comments: `/* A full mailbox explicitly rejects this barcode; never replay it later. */` and returns `error` (`E_OBJ`).
  - However, `ir_runtime.c:238` treats **any** `error < E_OK` (other than `MOTION_IR_SUPERSEDED`) as a fatal handoff failure and invokes `runtime_fault()`:
    - `ir_acquisition_stop()` permanently halts the 1 kHz timer and ADC ISRs.
    - `MOTION_HEALTH_IR` is asserted, stopping the motors with `MOTION_FAULT_EXTERNAL`.
    - Because `MOTION_HEALTH_IR` stays asserted and S3 is in `IR_RUNTIME_FAULT`, **neither GP20 retry nor USB `r` (`MOTION_RESET`) can recover the car** without recalibrating (`c`) or power-cycling.
- **When It Can Happen:**
  - On `SMP=1`, `ir_task` runs on Core 0 (priority 16) below `cyw43_utk_task` (Wi-Fi/MQTT, priority 4) and `usb_console` (priority 3). After a 30–60 ms Wi-Fi/MQTT burst, `ir_task` drains all queued 1-ms frames in a tight `for` loop (`< 100 µs` total) on Core 0 before Core 1's `motion_task` necessarily wakes and clears `shared.have_barcode`.
  - If `GP26` (`D0`) chatters rapidly across those queued frames (two `>= 5`-element or `29`-element bursts), the second completion returns `E_OBJ` and kills Subsystem 3.
  - Note also that `ir_acquisition.c:212` clears both `GPIO_PUE` and `GPIO_PDE` on `GP26`, leaving the pin high-impedance if the `D0` jumper wire loosens.
- **Recommended Fix:**
  - Either overwrite/coalesce in `motion_publish_barcode()` (or count dropped barcodes in diagnostics and return a non-fatal status) **or** handle barcode-slot-busy separately from line-handoff failure in `motion_ir_bridge.c` so noisy barcode chatter can never shut down line-following IR acquisition.

---

### Finding 3 — 0–20 ms Odometry Lag on Barcode Wakeup, Ignored `bc_active`, and `route_choose()` Veto (Medium–High)

- **Locations:**
  - `app_program/subsystem2/motion.c:1270-1275`, `1719-1737`, `1769-1779`
  - `app_program/subsystem2/motion_route.h:59-63`, `89-134`
  - `app_program/subsystem2/motion_ir_bridge.c:55`
- **Three Interacting Effects:**
  1. **Up to 19 ms of wheel travel missing on the last bar(s):**
     - `motion_publish_barcode()` sets `WAKE_BIT`, waking `motion_task` immediately on any 1-ms boundary.
     - In `motion_task` (`motion.c:1719`), `update_odometry()` and `route_odo_add()` only execute when `dt >= MOTION_PERIOD_MS` (`20 ms`). On an early barcode wakeup (`dt = 1..19 ms`), `route_odo` is **not** updated before `route_on_barcode()` runs.
     - `route_odo_at()` (`motion_route.h:92-98`) then flat-clamps any timestamp up to `40 ms` newer than the latest 20 ms sample to `o->total[newest]`, treating the car as stationary for the last `1..19 ms` and shrinking `widths[28]` (and `widths[27]` on fast scans).
     - *(Note: `testing/subsystem2/line_route_test.c:179-200` only advances `clock_ms` in exact steps of `20 ms`, so this mid-period wakeup path is never exercised in host tests.)*
  2. **V5 never reads `line_msg_t.bc_active` + off-center sensor geometry:**
     - `motion_ir_bridge.c:55` sets `line.bc_active`, documented in `ir_types.h:141` as *"barcode capture in progress: hold speed constant"*, but V5 (`line_follow.h` / `line_adaptive.h`) never reads `bc_active`.
     - During V5 tracking, wheel speeds vary between `0` (paused inner wheel), `250`, `350`, `450`, and `600 counts/s`. Moreover, `route_odo` records `left_count + right_count` (center-axle travel), whereas the barcode sensor (`GP26`) sits on the **left** side of the chassis. Steering during a scan changes left-sensor ground speed relative to both wall-clock time and center-axle counts.
  3. **`route_choose(by_dist, by_time)` lets distorted time widths veto valid distance widths:**
     - Every route letter (`A, B, C, D, Z`) shares its **exact 5-bar pattern** with 3 other Code 39 characters in `code39_table` and differs **only** in which single space passes the `1.25×` (`5/4`) ratio check:
       - `A` shares bars `W N N N W` with `1`, `K`, `U`
       - `B` shares bars `N W N N W` with `2`, `L`, `V`
       - `C` shares bars `W W N N N` with `3`, `M`, `W`
       - `D` shares bars `N N W N W` with `4`, `N`, `X`
       - `Z` shares bars `N W W N N` with `6`, `P`, `' '`
     - If the car accelerates or slows down across the barcode, `by_distance` can decode `'A'` while `by_time` shifts the widest space and decodes `'U'` or `'1'`. `route_choose('A', 'U')` (`motion_route.h:61`) sees `by_distance != by_time` and **returns `'\0'`, discarding the valid distance decode**. *(This was fixed in archived commit `43385d9` and reverted in `049de7f`.)*
- **Recommended Fix:**
  - Snapshot encoders and call `route_odo_add()` whenever `have_barcode` is true before calling `route_on_barcode()`.
  - Prefer `by_dist` authoritatively when odometry coverage across the barcode is valid (falling back to `by_time` only when wheel movement is unavailable, e.g., stationary bench swipe), or only let `by_time` veto if `by_dist` is low-margin.

---

### Finding 4 — Decoder Ignores `*` Sentinel Spaces and Inter-Character Gaps (Medium)

- **Location:** `app_program/subsystem2/motion_route_decode.h:63-79`
- **Mechanism:**
  - `route_code39_decode()` validates only the 5 **bars** of the start `*` (`widths[0,2,4,6,8]`) and stop `*` (`widths[20,22,24,26,28]`).
  - It ignores all 4 spaces inside both `*` characters (`widths[1,3,5,7]` and `widths[21,23,25,27]`) and does **not** bound the two inter-character spaces (`widths[9]` and `widths[19]`), unlike `motion_logic.c:173-187` which checks `widths[offset + 9] <= 2 * minimum`.
- **Impact:**
  - Two separate noisy marks on the mat separated by up to `1499 ms` of white space (`< BC_QUIET_MS`) that happen to sum to 29 elements are not rejected for having a huge `widths[9]` or `widths[19]` gap, and random bar noise has a higher chance of matching `*` when sentinel spaces are unchecked.
- **Recommended Fix:**
  - Bound `widths[9]` and `widths[19]` (e.g., reject if an inter-character gap is wider than the widest element in the adjacent characters, such as `> 2×` wide bar / space) and optionally validate the single wide space in both `*` sentinels when using clean digital edges.

---

### Finding 5 — Junction Trigger Assumptions, Non-Expiring `route.armed`, and Early `+`-Crossroads Exit (Medium–High)

- **Locations:**
  - `app_program/subsystem2/motion.c:983-996`, `1194-1244`
  - `app_program/subsystem2/motion_route.h:19-28`, `140-149`
- **Three Track/Geometry Risks:**
  1. **Junctions must produce `LINE_BOTH_BLACK` ($\ge 75$ on both `GP27` and `GP28` for $\ge 20\text{ ms}$):**
     - Because `GP27` and `GP28` straddle the `18 mm` line in `OUTSIDE` mode, a **one-sided branch** (`⊢` straight + right with no left black bar, `⊣` straight + left with no right black bar, or a dead-end `D` with no transverse black stop bar) only darkens **one** (or zero) line sensor (`LINE_EDGE_ONLY`). `route_step()` will never trigger the turn on such a feature.
     - Similarly, because a printed Code 39 barcode is ~`141 mm` long (`README_MOTION.md:20`), if a barcode is placed so close to a junction crossbar that `GP27`/`GP28` hit `LINE_BOTH_BLACK` *before* `GP26` exits the 29th element, V5 will begin crossing straight before `route.armed` is set.
  2. **`route.armed` never expires by distance or time:**
     - Once `route.armed` is set (`motion.c:1297`), nothing clears it except executing a `LINE_BOTH_BLACK` junction, replacing it with a newer barcode, or ending the run. If a junction fails to register `LINE_BOTH_BLACK` for 20 ms (e.g., skewed approach or one-sided branch), the stale manoeuvre remains armed indefinitely and executes at the **next** crossing elsewhere on the track.
  3. **Early arc exit on 4-way (`+`) intersections (`ROUTE_ARC_MIN_LEAD = 150`, `ROUTE_ARC_EITHER_SIDE_LEAD = 300`):**
     - With ~`2550–2774` counts/rev (`~0.075 mm/count`) and `125 mm` track width, `150` differential counts is only **$\sim 5.2^\circ$** of turn (and `< 10 mm` forward travel), and `300` counts is **$\sim 10.3^\circ$**.
     - On a `⊤` T-junction (white straight ahead), turning $5^\circ\text{–}10^\circ$ before looking for a branch mostly works (though `LINE_ROUTE_KERNEL.md:118` notes it still handed over early on the crossbar once).
     - On a **4-way `+` crossroads** (where a black line continues straight ahead), starting a $90^\circ$ right/left arc sweeps the turn-side sensor across the **$0^\circ$ straight-ahead branch first** at $\sim 5^\circ\text{–}15^\circ$, satisfying `b >= 35`, `b - a >= 10`, and `!both_black`—causing the car to abort the $90^\circ$ turn and resume V5 straight ahead.
- **Recommended Fix:**
  - Expire `route.armed` after a generous maximum travel distance (e.g., ~`500–800 mm` in encoder counts) if no junction is seen, logging an event for visibility.
  - If the assessment track includes 4-way `+` junctions, raise `ROUTE_ARC_MIN_LEAD` / `ROUTE_ARC_EITHER_SIDE_LEAD` so exit acquisition only arms after clearing the straight-ahead branch (e.g., $\ge 35^\circ\text{–}45^\circ$, roughly `900–1300` differential counts).

---

### Finding 6 — Default `A`/`B` Mapping Opposite to Project Spec & Minor Route Log Overwrite (Low–Medium)

- **Locations:**
  - `app_program/subsystem2/route_settings.c:8`
  - `app_program/subsystem2/motion_route.h:50-57`
  - `app_program/subsystem2/motion.c:1291-1300`, `1815`
  - `app_program/subsystem2/README_MOTION.md:18-19`
- **Details:**
  1. **Compiled-in default mapping:** `route_settings.c:8` initializes `route_mapping[26]` with `A = 1 (Right)`, `B = 2 (Left)`, `C = 3 (Straight)`, `D = 4 (U-turn)`, `Z = 2 (Left)`. Meanwhile, `README_MOTION.md:18-19` (citing `Project Write-up.pdf`) and the legacy non-route path (`motion.c:1815`) define **`A` = Left (`+90°`), `B` = Right (`-90°`)**. While the dashboard A–Z mapping UI persists custom mappings in flash (`0x1fe000–0x1fffff`), flashing onto a new/erased Pico will default to `A = Right, B = Left`.
  2. **USB console `[route]` log loses original letter:** In `motion.c:1292-1295`:
     ```c
     chosen = route_mapping_code(chosen); /* retain original letter in barcode telemetry */
     if (route.phase != ROUTE_IDLE || !route_letter_armable(chosen)) {
         route_event(ROUTE_EV_NOT_ARMED, chosen, route.phase != ROUTE_IDLE ? 1 : 2, 0, 0, 0);
         return;
     }
     ```
     When a scanned letter is mapped to Ignore (`0`), `route_mapping_code()` returns `'\0'`, so USB logs `[route] ROUTE: ? not armed` instead of the scanned letter; and when `'Z'` is mapped to Left (`2` → `'B'`), USB logs `ROUTE: B armed` instead of `Z`.
- **Recommended Fix:**
  - Confirm the official assessment mapping for `A`/`B`/`C`/`D` and set `route_settings.c:8` defaults to match it so a fresh board behaves identically without needing a dashboard save.
  - Keep `char scanned = chosen;` for `ROUTE_EV_NOT_ARMED` / `ROUTE_EV_ARMED` logging while storing the mapped action for execution.

---

## 4. Suggested Fix Order

1. **Prevent fatal S3 shutdown on barcode slot contention (Finding 2):** Make a busy `shared.barcode` slot non-fatal to `ir_runtime` so barcode noise can never kill line following.
2. **Stabilize `D0` digital capture (Finding 1):** Add 2–3 ms glitch suppression in digital mode and require a short white quiet zone after element 29 instead of cutting off on a single 1-ms white sample.
3. **Fix distance-width accuracy & `route_choose()` (Finding 3):** Snapshot encoders into `route_odo` on barcode wakeup before `route_on_barcode()`, and stop letting speed-skewed `by_time` veto valid `by_distance` decodes.
4. **Tighten inter-character gap / sentinel validation (Finding 4):** Reject implausibly wide inter-character gaps (`widths[9]`, `widths[19]`).
5. **Align track/junction assumptions & default mapping (Findings 5 & 6):** Confirm track junction types (`⊤` vs `+` vs one-sided branches) and official `A`/`B` turn directions, add a max-distance expiry to `route.armed`, and raise `ROUTE_ARC_MIN_LEAD` if `+` intersections are used.

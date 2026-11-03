# Team and subsystems

The tables are pre-filled with the **example** in `app_program/` (marked
*example*). Replace those rows with your own as you replace the example code.

Fill this in at the start of the project and keep it up to date. It is the
single place that says **who owns what**, so that five people can work in
one firmware image without clashing over pins, priorities, or kernel objects.

A **subsystem** is one student's part of the product, with its own folder.
It may contain several tasks and own several peripherals (see section 3).

Rule: **only the owner edits a subsystem's folders.** To change another
subsystem, ask its owner or open a pull request for them to review.

## 1. Members and subsystems

| # | Subsystem (what it does) | Student ID | GitHub username | Code | Tests |
|---|---|---|---|---|---|
| 1 | Binary telemetry and bounded MQTT handoff; GP19 heartbeat retained | | | `app_program/subsystem1/` | `testing/subsystem1/` |
| 2 | Motors, encoders, PID, motion controller and barcode decoding; example producer retained for compatibility | 2501308 | | `app_program/subsystem2/` | `testing/subsystem2/` |
| 3 | *example:* Consumer: collects samples into statistics | | | `app_program/subsystem3/` | `testing/subsystem3/` |
| 4 | IMU / terrain monitoring: pitch, roll, humps, tilt, collision (GY-511, LSM303D) | | | `app_program/subsystem4/` | `testing/subsystem4/` |
| 5 | | | | `app_program/subsystem5/` | `testing/subsystem5/` |

Shared files (`app_program/app_main.c`, `app_program/app.h`, this file)
are changed only by agreement of the team. The exception is claiming an empty
row in this file (a pin, a peripheral, an interface ID), which anyone may do
(section 2).

## 2. Pins

One owner per pin. Claim a pin here **before** you wire or code it.

**Why:** two subsystems using the same pin is one of the most common and
hardest-to-find mistakes in a team project. The code builds without any
error, but the hardware misbehaves. Each subsystem can pass its own tests
and then fail once they are combined, and two outputs driving the same pin
against each other can damage it. Writing the claim down first means a clash
is spotted when someone edits this table, not hours later on the bench.

**No instructor check needed.** The team keeps this table itself. You do not
need the instructor, or the rest of the team, to check or approve a claim:
if the row is empty, fill it in and commit. If two people claim the same
pin, they agree between themselves who moves. Only the *reserved* rows are
fixed.

| Pin | Owner | Used for |
|---|---|---|
| GP23, GP24, GP25, GP29 | *reserved* | Pico W radio |
| GP0 | | (taken whenever the team builds with `CRASH_REPORT=1`) |
| GP1 | | (taken whenever the team builds with `CRASH_REPORT=1`) |
| GP2 | S2 | right encoder A (proposed; disabled until electrical/mapping verification) |
| GP3 | S2 | right encoder B (proposed; disabled until electrical/mapping verification) |
| GP4 | S2 | left encoder A (proposed; disabled until electrical/mapping verification) |
| GP5 | S2 | left encoder B (proposed; disabled until electrical/mapping verification) |
| GP6 | | |
| GP7 | | |
| GP8 | S2 | left motor M1 input A; user confirmed wheel identity |
| GP9 | S2 | left motor M1 input B |
| GP10 | S2 | right motor M2 input A; user confirmed wheel identity |
| GP11 | S2 | right motor M2 input B |
| GP12 | | |
| GP13 | | |
| GP14 | | |
| GP15 | | |
| GP16 | S4 | I2C0 SDA: GY-511 (Robo Pico Grove 4) |
| GP17 | S4 | I2C0 SCL: GY-511 (Robo Pico Grove 4) |
| GP18 | | |
| GP19 | *example:* S1 | external LED (LED + 330 Ω to GND) |
| GP20 | S2 | Robo Pico active-low button; opt-in 30 s circuit start/stop |
| GP21 | | |
| GP22 | | |
| GP26 | S3 | IR Left A0 – barcode sensor (ADC0) |
| GP27 | S3 | IR Centre A0 – line sensor A, left-hand (ADC1) |
| GP28 | S3 | IR Right A0 – line sensor B, right-hand (ADC2); Robo Pico VBAT-sense jumper must be open |

Pico W onboard green LED (`WL_GPIO0`, **not RP2040 GP0**): S2 supplies
GP20 circuit status; the existing CYW43 service task alone writes the wireless
GPIO. No external GPIO is reassigned and the S1 GP19 heartbeat is unchanged.

## 3. Peripherals

**Subsystems, tasks and peripherals are three different things:**

| Term | What it is | Example |
|---|---|---|
| **Subsystem** | one student's part of the product, one folder | "environment sensing", "motor control", "user interface" |
| **Task** | one thread of execution. A subsystem may have **as many tasks as it needs** | a sampling task and a filtering task, both in "environment sensing" |
| **Peripheral** | one hardware block on the RP2040 | I2C0, UART1, the ADC |

A driver is **not** a subsystem. It is code *inside* the subsystem that
needs that hardware: the "environment sensing" subsystem contains the I2C
sensor code, and owns I2C0. Write it as `drv_<name>.c` / `drv_<name>.h` in
that subsystem's folder, never under `device/`, and call it only from the
peripheral's owning task. The kernel's own drivers in `device/` (serial,
I2C, ADC) may be used instead; switching one on in `config/config_device.h`
changes shared configuration, so record it in the table below first.

**The rule: each peripheral is driven by exactly one task, its *owning
task*.** That task can be in any subsystem, and one subsystem may own
several peripherals (one owning task each, or one task owning all of them).
Every other task, in the same subsystem or another one, asks the owning task
through a kernel object (section 5) and never touches that peripheral itself.

*Why:* this port runs tasks on both cores at the same time. If two tasks
drive the same I2C or UART block at once, their transfers get mixed up, and
the drivers have no locking to stop it.

| Peripheral | Owner subsystem | Owning task | Pins |
|---|---|---|---|
| UART1 | | | |
| I2C0 | S4 | `terrain_task` (kernel driver `iica`, `DEVCNF_USE_IIC = 1`) | GP16 SDA, GP17 SCL |
| I2C1 | | | |
| SPI0 | | | |
| SPI1 | | | |
| ADC, ADC IRQ 22 | S3 | ir_task + ADC ISR on core 0; sample ADC driver disabled | GP26–GP28 |
| Hardware TIMER alarm 3, IRQ 3 | S3 | 1 kHz scan trigger on core 0; shared counter initialized by BSP | no GPIO |
| PWM slices 4 and 5 | S2 | motion_task | GP8-11; 10 kHz; no physical-timer sharing |
| IO_BANK0 IRQ 13, encoder edge capture | S2 | encoder ISR on motion task's core | GP2-5; exclusive handler, no SDK GPIO IRQ registration |
| WiFi radio, TCP/IP, MQTT | *kernel* | `cyw43_utk_task` (built in) | GP23–25, GP29 |

### WiFi and MQTT are special

The radio and the network stack belong to the kernel's built-in WiFi service
task, `cyw43_utk_task` (`lib/libwifi/sysdepend/pico_rp2040/`). **No student
task may call `cyw43_...`, lwIP or MQTT functions, not even from a
"WiFi subsystem".** The calls are not safe from any other task, on either core.

What students *can* do:

- **Read the connection state** from any task, which is always safe:
  ```c
  #include "cyw43_utk.h"
  T_CYW43_UTK_STATUS st;
  cyw43_utk_get_status(&st);   /* st.ready, st.link_status, st.ip_addr, ... */
  ```
- **Send or receive data over the network** by adding code that *runs
  inside the service task*. The MQTT client is in
  `lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c`. It publishes a heartbeat
  today. A "cloud / reporting" subsystem would add a kernel object
  (e.g. a message buffer) that application tasks fill. The MQTT code then
  reads it with `TMO_POL` (never blocking) and publishes it.
  Because this changes shared library code, **agree it with the team and
  the instructor first**, and record it as an interface in section 5.
  It needs the `NET=lwip MQTT=1` build (README section 3).

## 4. Task priorities

1 is the highest. 1–4 are used by the kernel (USB console 3, WiFi 4). Each subsystem gets its own band by default;
change it only by team agreement. Write down every task you create.

| Subsystem | Priority band | Tasks (name – priority – core) |
|---|---|---|
| 1 | 12–13 | *example:* led_task – 12 – any |
| 2 | 14–15 | motion_task – 14 – core 1; compatibility producer_task – 15 – core 1 |
| 3 | 16–17 | ir_task – 16 – core 0; compatibility consumer_task – 17 – core 0 |
| 4 | 18–19 | terrain_task – 18 – core 1 (woken every 20 ms by `cyc_terrain`; must run on the core that handles the I2C interrupt, see `S4_CORE`) |
| 5 | 20–21 | |
| dev tool | 22 | int_console – 22 – core 0 (integration test console, `app_program/int_console/`; `APP_CONSOLE 0` removes it) |

Core: `TP_PRC1` = core 0, `TP_PRC2` = core 1, or "any".

## 5. Interfaces between subsystems

Every arrow between two subsystems is listed here. The **provider** creates
the kernel object and publishes its ID in its own `subsystemN.h`; the **user** only
sends to it or waits on it. Agree on the data format before coding.

| ID | From → To | Mechanism | Object ID name | Data / meaning | Agreed by |
|---|---|---|---|---|---|
| IF-01 | S2 → S3 | mailbox (owned by S3) | `mbx_samples` | `SAMPLE_MSG` in `subsystem2.h` | *example* |
| IF-02 | S3 → S2 | semaphore + fixed memory pool (owned by S2) | `sem_slots`, `mpf_samples` | receiver returns the block, then 1 slot | *example* |
| IF-03 | S3 → S4 | message buffer + event flag (owned by S4) | `mbf_summary`, `flg_events` | text line ≤ `S4_SUMMARY_MAX` bytes, then `FLG_BATCH` | *example* (S4 keeps these objects until S3 removes its example) |
| IF-04 | S4 → S3 | shared data under a mutex (owned by S3) | `app_stats`, `mtx_stats` | `APP_STATS`, read only while locked | *example* (no longer used by S4) |
| IF-05 | S4 → S1 | task control | `tid_led` | `tk_sus_tsk` / `tk_rsm_tsk` / `tk_chg_pri` | *example* (no longer used by S4) |
| IF-06 | S4 → any (Vehicle Controller, S1 telemetry) | shared data under a mutex (owned by S4) | `s4_get_terrain()`, `mtx_terrain` | `S4_TERRAIN`: minimum-bit, 64 bits (field table in `subsystem4/terrain_pack.h`): pitch/roll 0.5°, turn rate 2°/s, peaks 1 mm, hump state, motion class, count, calibrated, IMU fault, 15-bit time; decode with `S4_PITCH_DDEG()`, `S4_VALID()`, `S4_AGE_MS()`; stale after `S4_STALE_MS` | proposed by S4 |
| IF-07 | S4 → Vehicle Controller | event flag (owned by S4) | `flg_terrain` | `S4_EVT_HUMP_ENTRY/HUMP_DONE/TILT/COLLISION` latched until the VC clears them; `S4_EVT_IMU_FAULT` level, copied by Buddy 2 into the shared fault bitmask (single writer, C06) | proposed by S4 |
| S2/S3 IF-06 | Controller/local operator -> S2 | one copied request slot + mutex, event wake | motion_submit / motion_renew | motion_request_t; id, epoch, TTL; reject busy, no replay | implemented proposal; provider review pending |
| S2/S3 IF-07 | B1/B3/B4/local -> S2 | persistent event bits + atomic generations | motion_stop / motion_set_fault | stop independent of ordinary requests; local reset only | implemented proposal; provider review pending |
| S2/S3 IF-08 | S3 -> S2 | latest line snapshot + one barcode slot under mutex | motion_publish_line / motion_publish_barcode | existing line_msg_t / bc_msg_t plus bounded acquisition timestamp and epoch | live sampler implemented; validation pending |
| S2/S3 IF-09 | S2 -> B1/B4 | coherent snapshot, nonblocking mutex copy | motion_get_status | wheel speeds mm/s, signed travel mm, counts, acquisition time, epoch and validity | consumer implementation pending |
| S2/S3 IF-10 | Local controller -> S3 | copied command slot + mutex + event | ir_runtime_calibration / ir_runtime_get_status | BEGIN/FINISH calibration, raw/normalized readings, validity and fault/timing status; no automatic motion | implemented proposal; validation pending |

**Interface numbers to agree:** Buddy 2/3 (`integration/buddy2-motion`) and
Buddy 4 (`feature/s4-imu-driver`) both used IF-06 to IF-08. Until the team
renumbers them, Buddy 2/3's rows are marked "S2/S3 IF-xx". S4's old IF-08
(wheel data for S4) is provided by S2/S3 IF-09 (`motion_get_status`).

Each interface is tested by at least one integration test
(`testing/integration/TEST_LOG.md`).

## 6. Kernel object budget

Limits are set in `config/config.h`. If the team needs more, raise the
limit there. The numbers below include the examples, B2 motion and S3 IR runtime.

| Object | Limit | S1 | S2 | S3 | S4 | S5 |
|---|---|---|---|---|---|---|
| Task | 32 | 2 | 2 | 2 | 1 | |
| Semaphore | 16 | | 1 | | | |
| Event flag | 16 | | 1 | 1 | 2 | |
| Mailbox | 12 | | | 1 | | |
| Mutex | 12 | 1 | 2 | 2 | 1 | |
| Message buffer | 12 | | | | 1 | |
| Fixed memory pool | 12 | | 1 | | | |
| Variable memory pool | 12 | | | | | |
| Cyclic handler | 12 | 1 | 1 | | 1 | |
| Alarm handler | 12 | | | | | |

(The kernel's USB and WiFi service tasks also use a few of these.)

## Telemetry integration proposal (2026-11-02)

S1 collects every 250 ms in its existing priority-12 task. One S1 mutex protects a latest-frame slot and an eight-record event FIFO. The Wi-Fi task polls this handoff and publishes at QoS 1. Application tasks do not call network APIs. S2 now has a separate nonblocking observer snapshot mutex in addition to its command mailbox mutex.

The MQTT shared-library bridge and S4 consumer changes remain proposed integration changes for team/instructor review; this record does not claim that review occurred. The user authorised implementation and software validation. Hardware validation is not implied.

Application startup releases B2 after subsystem start functions succeed. Motors stay stopped before release; a five-second timeout latches a fault. Calibration and health faults still apply. The development console defaults off. APP_CONSOLE=1 and MOTION_DEVELOPMENT=1 are explicit bench overrides.

See tools/dashboard/README.md for deployment, protocol and validation limitations. Topic base: inf2004/se08/car01/v1. Project broker port: 1884 (1883 is occupied by the installer-created Windows service).

Stationary-fix follow-up: default APP_CONSOLE=0 now selects S1's diagnostic-only USB task (priority 22, core 0). It accepts h/i/c/f and cannot drive, reset or bypass motor faults. Full bench console is still opt-in. S1 checks S4 timestamp age using the existing kernel-clock contract; no S4 code was changed in this follow-up. B2 diagnostics are copied under its existing status mutex (no additional kernel objects).

## Bounded line-tracing integration (2026-11-02)

User requested ending wheel calibration and integrating encoder speed control
with Buddy 3 line tracing. Buddy 2 now owns shared forward speed PI and the
local-only MOTION_LINE_TRACE request. LINE_DEMO=1 explicitly enables a distinct
_line image with a maximum two-second, 20%-duty line trial. Default remains
LINE_DEMO=0 with diagnostic-only USB controls. S1's console routes the opt-in
commands to Buddy 2; no new tasks/IRQs/kernel objects or B3/B4 edits were needed.

This count-domain mode does not enable metric calibration: raw counts remain
available, calibrated/valid physical odometry stays false, and existing B4 and
network contracts remain intact. Local reset/arm, healthy calibrated IR, fresh
line/encoder inputs and operator start are required; no IR bypass, automatic
barcode turn or mission renewal is enabled. Software checks and both firmware
profiles build successfully. No integration flash/hardware test has run.
Details and prepared image: testing/integration/LINE_FOLLOW_INTEGRATION.md.


## GP20 adaptive V5 integration (2026-11-04)

The current opt-in circuit trial is `LINE_DEMO=1 LINE_CIRCUIT=1
LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1`. It uses GP20 start/stop,
Pico W LED status, calibrated IR pair observations, encoder speed feedback,
bounded crossing/recovery and bounded pause settling. V5 was flashed and
stationary-verified; the operator subsequently reported a good test. This does
not establish all-course reliability or calibrated metric motion.

Buddy 3 runtime changes handle superseded-epoch handoffs and circuit calibration;
the compact line-message contract remains compatible. No Buddy 4 implementation
change is included. See [V5 release/status](testing/integration/ADAPTIVE_V5_RELEASE.md)
for supported behavior, reproducible validation and outstanding issues.

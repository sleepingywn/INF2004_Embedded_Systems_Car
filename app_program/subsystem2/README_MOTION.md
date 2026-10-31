# Buddy 2 motion integration

Implementation candidate, 29 October 2026. **Not built, tested or flashed.**
The user explicitly requested permission before any testing or flashing.
Source inspection and edits are the only verification performed so far.

Based on Buddy 3 branch `feature/s3-ir-bench-test`, commit `cc85b30`, which is
ahead of `main`. The integration branch is `integration/buddy2-motion`.
No changes have been pushed. Buddy 3's processing algorithms remain unchanged.

## Requirements and sources

Sources reviewed in the module folder (outside this Git repository):

- `Group Project/Briefings/Project Briefing.pdf`, Buddy 2 slide: motors,
  encoders, speed/distance, PID, straight correction, turns and measurement reports.
- `Group Project/Briefings/Project Write-up.pdf`: micro T-Kernel, C, Pico SDK,
  reusable motion APIs, autonomous line/barcode navigation. Its A/B/C/D mapping
  is used here as left/right/straight/U-turn; confirm the course's final mapping.
- `Group Project/Briefings/Barcode & Line Specification.pdf`: 18 mm line,
  20 mm separation and approximately 141 mm barcode length. These are NOT wheel
  calibration parameters.
- `Group Project/Briefings/Barcode Sample.pdf`: A and Z examples. Z is not an
  authorised navigation command and is rejected by this decoder.
- `Group Project/Briefings/Change Requirement - Tribunal 1/Change Requirement 1.docx`:
  minimum-bit application data, synchronized packed storage, representation/RAM/
  message-size evidence and correctness/timing measurements.
- `Group Project/barr_c_coding_standard_2018.pdf`: reviewed integer widths,
  unsigned bit-field storage, explicit signed conversion, braces, function and
  interface conventions. New B2 source has a local Allman formatting profile.
- `Group Project/Tribunal/Draft4/Week8_Tribunal1_Draft4.md`: B2 motion ownership,
  newer barcode-decoding allocation, B2-to-B4 snapshot and proposed K1-K7
  contracts. Proposed timing/accuracy values are not measured achievements.
- `Group Project/Tribunal/Draft4/evidence/tribunal-week6-verification/week6-current.txt`:
  B2-FR1 through B2-FR9.
- `Group Project/Datasheet (for Default Project)/Robo Pico Board Datasheet.pdf`,
  motor terminal table and Table 3: GP8/9=M1, GP10/11=M2, both LOW=brake.
- `HardwareChecks/buddy2_motor_test/`: the prior standalone motor test, 10 kHz,
  20% duty, 500 ms pulses. The user confirmed the original mapping works and
  confirmed M1=left, M2=right. That does not validate this RTOS implementation.
- [Zint's Code39 character assignments](https://github.com/zint/zint/blob/master/backend/code.c),
  referencing ISO/IEC 16388, cross-checked for the standard A-D/start-stop masks.
  Decoder logic is implemented locally; no Zint code/library is linked.

## What runs and what is still missing

| Requirement | Implementation | Verification prepared, not executed |
|---|---|---|
| B2-FR1 motor driver / one writer | `drv_motor.c`, `motion_task` | S2-U08 |
| B2-FR2 encoders, speed and distance | `drv_encoder.c`, odometry in `motion.c` | S2-U03/U04/U09 |
| B2-FR3 bounded PID | `motion_logic.c`, per-wheel PID in owner | S2-U05/U10 |
| B2-FR4 straight correction | equal-velocity and distance branches only | S2-U10 |
| B2-FR5 forward/backward, distance, turns/U-turn | `motion_request_init`, `motion_submit`, control state machine | S2-U11 |
| B2-FR6 stop/cancel/no replay | persistent stop generation, epoch, explicit reset | S2-U12 |
| B2-FR7 coherent motion status | `motion_get_status`, acquisition time and validity | S2-U15 / INT-06 |
| B2-FR8 command admission/expiry | bounded slot, ids, TTL, renewal | S2-U07/U12 |
| B2-FR9 fault stop/recovery | encoder/stall/range/late update/input faults | S2-U13 |
| Newer Buddy 2 barcode allocation | `motion_barcode_decode`, `motion_ir_bridge` and turn/reacquire states | S2-U06/U14 / INT-05 |
| Minimum-bit representation | bit fields, masks, checked narrowing, guarded shared storage | S2-U16 and measurement report |

`subsystem2_init()` creates the motion objects and `subsystem2_start()` starts
`motion_task` at priority 14 on core 1 (core 0 for `SMP=0`). Motor initialization
is stopped. No movement or calibration test is automatically requested, including
in `TEST=1` builds. The old sample producer remains at priority 15 because S3/S4
examples still use its pool/semaphore. It does not touch motor hardware.

The driver uses the existing BSP's GPIO helpers and RP2040 register definitions.
The repository owns clocks, interrupts and USB; it does not link the complete
Pico SDK runtime. Therefore the standalone CMake/main/stdio initialization is
not copied into this firmware. Motor PWM is released from reset explicitly,
uses slices 4/5, TOP=999 and a divider derived from the BSP's 125 MHz clock.
At this clock it gives 10 kHz. A/B duty is written in one register operation
per wheel; only the motion task calls the private driver.

Remaining dependencies are explicit, not replaced with guessed values:

1. Encoder identity/electrical levels, x4 counts per wheel revolution, signs,
   wheel circumference and track width are unverified. `motion_config.h`
   contains zero placeholders and `MOTION_CALIBRATED=0`;
   `MOTION_ENCODERS_VERIFIED=0` leaves GP2-5 untouched. Calibrated commands
   reject these defaults. A deliberate `MOTION_BENCH` request can exercise
   the confirmed motor pins without claiming calibrated motion.
2. The missing S3 live sampler and calibration task have now been implemented
   in `../subsystem3/ir_acquisition.c` and `ir_runtime.c`. They are unbuilt and
   untested. See `../subsystem3/README_RUNTIME.md` for the explicit calibration
   and recovery API. Existing hardware results record GP28 failing; retest that
   wiring. Its failed contrast check keeps the IR health fault asserted.
3. Buddy 4 and Buddy 1 consumers are not implemented on the current branches.
   Their public input is `motion_get_status()`, not raw encoder GPIO access.
4. PID gains, line/straight gains, speed/duty limits, freshness/timeout limits,
   reversal rest and position tolerance need measured calibration. Enabling
   the calibration flag alone is insufficient; the configuration validator
   requires every relevant field and bounds its representable range.
5. No live serial command parser is added. In particular `tm_getchar()` in
   this port ignores its polling argument and masks interrupts while receiving;
   it must not be used as an assumed nonblocking motor-command interface.

## Public motion interface

Include `motion.h`. Use `motion_request_init()` before `motion_submit()` so
full-width values are checked BEFORE narrowing into packed fields. Zero-initialize
data or use the constructor; C bit-field overflow cannot be detected afterwards.

| Command | value | left | right | Behaviour |
|---|---|---|---|---|
| `MOTION_VELOCITY` | 0 | signed mm/s | signed mm/s | speed-controlled pair; equal speeds receive straight-path correction |
| `MOTION_DISTANCE` | signed mm | positive speed mm/s | 0 | positive forward, negative backward; complete when both wheels reach tolerance |
| `MOTION_TURN` | signed degrees, -180..180 | positive wheel speed mm/s | 0 | positive left, negative right; +180 is U-turn |
| `MOTION_FOLLOW` | 0 | positive cruise below maximum mm/s | 0 | S3 line steering and barcode manoeuvres; renewable lease, steering headroom required |
| `MOTION_BENCH` | 0 | electrical permille | electrical permille | local only; +/-200 maximum, <=500 ms; no encoder/PID claim |
| `MOTION_RESET` | 0 | 0 | 0 | local only, stopped, sources healthy; clears fault/inhibit, starts a new epoch, never drives |

Every request has schema=1, source, monotonically increasing nonzero id within
its epoch, epoch from `motion_get_status()`, acquisition/issue time from
`motion_now_ms()`, and a 2..30000 ms TTL. All expiry comparisons are unsigned
modular differences within a less-than-half-counter-range window. Network and
terrain sources cannot directly initiate movement. The locally agreed vehicle
controller is the mission authority; no MQTT callback calls the PWM driver.

The single request slot rejects busy requests; there is no unbounded FIFO.
`E_OK` from submit means queued, not physically executed. Observe
`request_epoch`, `request_id`, `result` and `state` in a coherent status copy.
`ACCEPTED` becomes `COMPLETED`, `CANCELLED`, `EXPIRED` or `FAILED`;
admission failure is `REJECTED`. The most recent outcome remains until another
request is consumed. This is a latest-status contract, not a durable event log;
the controller must observe completion before issuing another ordinary command.

Velocity/follow callers may renew the SAME id/source/epoch with `motion_renew()`
and a newer issue time before expiry. Renewal cannot revive a stopped/expired
request. Finite moves have bounded duration and cannot be extended by renewal.
Barcode turns additionally have a manoeuvre timeout independent of follow renewal.
An application should not pass a command's old issue timestamp as a renewal.

`motion_stop()` is independent of the ordinary slot: it signals a persistent
stop bit and advances an atomic stop generation. The owner brakes, cancels and
advances the epoch. A local RESET followed by a fresh movement request is needed.
`motion_set_fault()` has a separate source cell for local, IR, network and terrain
health. Each producer only clears its own cell after recovery. Clearing a source
does not clear the owner's latched fault. Atomic generations retain assertions
that race with RESET; neither a full request slot nor a reset-bit clear loses them.
Default disconnected-network policy remains an application decision: B1 may
assert its health source for a chosen strict policy. This module does not invent
a heartbeat deadline or claim an implemented autonomous/strict mode selector.

Example for an explicitly authorised future bench harness, NOT executed:

```c
motion_status_t status;
motion_request_t request;
if (motion_get_status(&status) == E_OK) {
    /* Caller chooses a fresh nonzero id; never reuse one within an epoch. */
    ER er = motion_request_init(&request, MOTION_BENCH, MOTION_LOCAL,
                                status.epoch, 1, 500, 0, 200, 0);
    if (er == E_OK) { er = motion_submit(&request); }
    /* Check er, then status outcome for this id; physical observation is separate. */
}
```

## Encoder/PID implementation

Encoder GPIOs are proposed left A/B=GP4/5, right A/B=GP2/3. A/B voltage, polarity,
pull requirements and motor/encoder signs must be verified before enabling them.
X4 capture records both edges through IO_BANK0 IRQ 13 on the motion task's core.
ISR work is bounded: acknowledge owned bits, read one GPIO snapshot, update two
modulo counters. No per-edge allocation, console output or task notification.
Simultaneous pending edges and impossible two-bit transitions increment an invalid
counter instead of inventing a direction. High-speed counts can still be missed;
capture capacity must be measured against a reference under full system load.

The task masks interrupts on that SAME core only for its paired counter copy.
No other core reads these raw globals. Inter-task records use a priority-inheriting
mutex and polling operations. A contended owner handoff fails stopped instead of
blocking control; the real contention rate and timing must be evaluated.

Signed count deltas are computed without signed overflow across uint32 wrap.
Distance uses per-wheel integer micrometres/count with carried division remainder;
speed uses actual elapsed time (um/ms = mm/s). Published travel is in signed mm.
Range overflow, invalid edges, configured no-progress timeout and overdue control
are faults. Lack of pulses cannot distinguish every disconnected/stuck encoder
from a stationary wheel. `valid` is therefore not proof of sensor health under all
physical conditions. Wheel slip is not detectable from encoders alone.

PID is fixed point with bounded output, conditional integration and state reset
on stops/mode changes. Straight correction is separate from wheel PID and only
applies to straight commands, not intentional steering or turning. Turns use
track width and wheel arc distance; positive angle is counterclockwise viewed
from above. Signed motor mapping converts logical forward to driver polarity.
Reversal applies a configured brake interval; it is not proof the wheel stopped.

## Buddy 3 integration

The existing `line_msg_t` sign is positive when the line lies to the left.
The controller reduces left target speed and increases right target speed,
bounding the difference to maintain the requested mean within wheel limits.
Line calibration/fault/loss/staleness stops following. BOTH_BLACK is a junction
candidate, not an independently decoded instruction.

`motion_ir_bridge_sample()` is a task-context adapter that reuses Buddy 3's
`line_update()` and `bc_feed()` unchanged. It consumes normalized 0..100 samples,
calibration validity, full acquisition timestamp and epoch. It publishes a line
record every 10 ms and a barcode once capture completes. Feed it from ONE actual
1 kHz hardware-paced sampling task. Its context is task-owned; no mutex is needed
around algorithm state. Do not invoke it from an ADC interrupt.

The kernel tick is 10 ms. `tk_dly_tsk(1)` cannot implement 1 kHz. S3 now uses
hardware alarm 3 and chained ADC interrupts, with bounded buffering and
scheduled acquisition timestamps accepted only within explicit timing bounds.
See `../subsystem3/README_RUNTIME.md` for timestamp uncertainty. Missing,
duplicate or irregular millisecond samples discard the partial barcode and assert
IR health failure. After fixing/calibrating the sensor, S3 clears only
`MOTION_HEALTH_IR`; the local operator then resets B2. A slow/drifting sampler must
not label reconstructed samples as if they were measured at 1 kHz.

The live runtime asserts IR health on boot until all three channels are
calibrated. This also blocks direct motor bench commands in this integrated
firmware until the IR fault has been cleared and B2 explicitly reset. The
older standalone motor test remains a separate program.

The decoder accepts exactly *A*, *B*, *C*, *D* (29 run widths), forward or reverse.
It requires valid sentinels, narrow inter-character gaps, three distinct wide
elements per character, and rejects saturation/overflow/ambiguous patterns.
No checksum is assumed because S3's interface is a single-character Code39 frame.
Width tolerances are engineering candidates, not calibrated thresholds.
Duplicate/old acquisition timestamps, wrong epochs and barcodes outside active
FOLLOW are discarded. The producer must preserve original acquisition time when
retrying a message; it cannot re-stamp old data as new. Invalid frames do not steer.

A/B/D temporarily suspend line steering and execute encoder turns. On completion,
outputs brake and state becomes REACQUIRE. A new post-turn valid line sample must
arrive before tracing resumes; otherwise the manoeuvre times out. This is stopped
reacquisition, not a physical search manoeuvre. C continues tracing. No obstacle
avoidance/absent Buddy 5 implementation is silently added.

## Buddy 4 / Buddy 1 snapshot

Use `motion_get_status()` and handle a negative return (retry later). Copy once;
do not combine wheels from different reads. `time_ms` is the encoder acquisition
time; `updated_ms` is state publication time. They intentionally differ. Check
`valid`, `calibrated`, `epoch` and agreed maximum wheel age before using the data.
Forward-positive speeds are mm/s and each wheel's signed travel is mm. Centre
travel can be formed from the average wheel travel after calibration. B4 may
derive acceleration/yaw rate from coherent differences and verified wheel geometry.
Neither this status nor the driver claims an IMU gyro exists.

Read physical values using `motion_left_speed(&status)`,
`motion_right_speed(&status)`, `motion_left_distance(&status)` and
`motion_right_distance(&status)`. Applied electrical permille is exposed by
`motion_left_duty(&status)` / `motion_right_duty(&status)`. The underlying
`*_bits` members use explicit unsigned encoding; treating them directly as signed
measurements is incorrect. Request fields should likewise use the constructor
and `motion_request_left/right/value()` accessors.

Epochs are boot-local. Reset discards accumulated travel and changes epoch.
Consumers discard old history after reset/reboot and invalidate it after stale
input. A persistent/network session identity across power cycles belongs in B1's
transport contract; the 32-bit boot-local epoch is not a security credential.

## Resource and timing review

S2 adds one task, one event flag, one mutex and one 20 ms cyclic release.
The clock timer is read only. PWM slices 4/5 and IO_BANK0 IRQ13 are reserved in
TEAM.md. Enabling physical timers requires an explicit allocation review; the
motor module currently rejects that build. Enabling I2C with pins overlapping
GP8-11 also causes a compile-time error. Revisit GP2-5 before enabling I2C on the
Grove/Maker connectors used by the proposed encoders.

The task services stop events independently of the periodic release and does not
log, read the network, wait for ADC/I2C, or take a blocking mutex in the control
path. A delayed update above 100 ms fails stopped; 100 ms is an implementation
guard, NOT compliance with the proposed 20 ms period/2 ms jitter/20 ms stop target.
CPU execution, release jitter, lock contention, timer quantization and physical
stopping must be measured. The kernel's higher-priority USB/WiFi tasks can delay
the application; priority 14 alone proves no deadline.

## Data representation

Bit fields are native C ABI records only. Never send their object bytes over MQTT.
B1 must serialize explicit masks/shifts/endian order under an agreed wire schema.
Alignment/padding is not assumed to equal the sum of logical field widths.
New B2 signed quantities use unsigned bit fields plus `motion_bits_encode/decode`
to meet Barr 5.3a. The signed ranges below describe the decoded value, not the C
storage type. Buddy 3's existing `line_msg_t` remains its original signed-bit-field
interface; changing that shared ABI requires coordination and is not silently done.

| Application field | Declared valid range | Logical bits / implementation |
|---|---|---|
| command / state / outcome | 6 / 8 / 7 values | 3 each, bit fields |
| request source / health-source index | 4 values | 2 in stored requests; health signals use one event bit each |
| schema | 0..3, currently accepts 1 | 2 |
| validity / calibrated / task flags | false/true | 1 each, bit fields |
| faults | 8 independently meaningful conditions | 8-bit mask |
| request speed | -2047..2047 mm/s | signed 12 |
| request distance | -524287..524287 mm | signed 20 |
| angle | -180..180 degrees | signed 9-bit member of the distance/angle union |
| lease | 2..30000 ms | unsigned 15 |
| measured speed | -2047..2047 mm/s | signed 12; range fault before narrowing |
| applied/previous electrical duty | -1000..1000 permille | signed 11 per wheel |
| wheel travel | -8388607..8388607 mm | signed 24; range fault, no silent wrap |
| decoded navigation | none/A/B/C/D | 3 |
| raw counts, timestamps, identities, diagnostic counters | uint32 modulo domain | 32; signed difference requires <2^31 separation |
| quadrature previous state | 0..3 per wheel | two task/ISR-owned 2-bit fields |
| PID previous error | -4094..4094 | signed 13 |
| PID integral (Q10 permille) | -1024000..1024000 | signed 21 |
| accumulated/start travel in um | +/-8,388,607,000 | signed 34; checked before narrowing |
| target wheel arc/distance in um | +/-524,287,000 | signed 30 |
| carried division remainder | magnitude below counts/rev, <=1,000,000 | signed 21 |
| arithmetic intermediate products | full checked multiplication range | int64 expression temporaries to avoid overflow; not a wire field |

RAM and message-size comparison must be measured after permission to compile.
`testing/subsystem2/MEASUREMENTS.md` specifies evidence still required, including
any remaining storage-padding tradeoffs. No RAM savings or timing result is claimed.

## Authoring review (no execution)

Source review pass 1: mapped B2-FR1..9 and the newer barcode allocation; preserved
S3 algorithms and old S2/S3/S4 example dependencies. Checked PWM clock/reset and
I2C/IRQ ownership against local source/datasheet.

Pass 2: traced request admission, slot reservation, stop during active/pending
work, reset races, expiry/renewal, reversed barcode frames, source failure and
counter/timestamp wrap. Corrected an admission window, separated sensor time from
publication time, and added stop/fault generations around reset.

Pass 3: traced packed-field ranges, PID integrator bounds, signed count conversion,
wheel direction changes, acquisition gaps and startup failure propagation.
Corrected signed/unsigned multiplication in negative line-error steering and
checked distance range before narrowing the persistent micrometre accumulator.
Prepared executable pure-logic cases and a hardware/integration test matrix.
These are inspection findings, not build/static-analyser/test results.

Pass 4: removed new signed bit-fields, added explicit unsigned encoding and
decoding accessors, widened operands before signed arithmetic, and prepared
boundary/steering cases S2-U17/U18. Applied formatting only; no test was executed.
Formal whole-repository Barr compliance and measured packing efficiency are not
claimed; the inherited S3 ABI and kernel conventions are explicitly identified.

Next gate: ask the user to approve the build and host-only pure-logic tests first.
Ask separately before flashing or powered tests. Do not treat the old standalone
motor evidence as proof that this candidate firmware works.

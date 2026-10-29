# Live S3 integration

Implementation candidate, 29 October 2026. Added at the user's request to fill
the missing Buddy 3 runtime. **Not built, tested or flashed.** Source review is
not evidence of timing, electrical correctness or successful vehicle motion.
The user's approval is required before any build, test or flash.

## Data path and ownership

`hardware alarm 3 -> three ADC conversions -> bounded raw-frame ring -> ir_task
-> calibration/normalisation -> motion_ir_bridge -> B2 line/barcode controller
-> motion_task -> motor PWM`

Buddy 3's `sensor_line.c`, `barcode_sense.c` and message ABI remain unchanged.
`subsystem3_init/start()` now creates and starts `ir_task` at priority 16 on
core 0. The compatibility consumer moves to priority 17; its example mailbox
and S4 statistics continue to work independently of the sensor data path.

| Resource | Exclusive owner / allocation |
|---|---|
| GP26 / ADC0 | Barcode sensor A0 |
| GP27 / ADC1 | LEFT-hand line sensor A0 |
| GP28 / ADC2 | RIGHT-hand line sensor A0; VBAT-sense jumper OPEN |
| ADC block, IRQ 22 | S3; `DEVCNF_USE_ADC=0` disables the competing sample driver |
| Hardware TIMER alarm 3, IRQ 3 | S3; not a kernel alarm object or PWM physical timer |
| Shared TIMER counter | BSP initializes 1 MHz tick once at boot; S3 never resets it |
| One event flag, one mutex, one additional task | S3 runtime |
| GP8/9, GP10/11 | B2 left/right motors, unchanged |

The BSP now explicitly configures the RP2040 watchdog **tick generator** at
12 CLK_REF cycles per microsecond and releases TIMER reset at startup. This
does not enable the watchdog reset function. Previously no such initialization
was present in the repository, although B2 and the WiFi compatibility layer
read the raw timer. It must be verified in both SMP and single-core builds.

## Acquisition and timing contract

The 10 ms kernel tick cannot pace 1 kHz barcode measurements. Alarm 3 launches
ADC0 once per scheduled millisecond; the ADC FIFO interrupt chains ADC1 then
ADC2. There is no conversion polling in either ISR. Both ISRs use priority 2
on core 0 and do not preempt each other's packed state. The same pinned task
masks local interrupts while copying ring entries; other cores access only
the runtime's mutex-protected status and command slot.

The ring holds 15 complete frames. Each frame stores three 12-bit values and
the full 32-bit scheduled millisecond timestamp. The timestamp starts from the
64-bit raw microsecond clock, so the raw low-word wrap at about 71 minutes
does not become a millisecond discontinuity. No missing frames are synthesized.
Each frame's launch must occur within `IR_ACQ_MAX_JITTER_US` of its deadline,
and its full three-channel scan must complete within that bound of launch.
The default **100 us is a provisional acceptance limit, not a measurement**.
Thus individual channels can be sampled up to 200 us after the timestamp's
nominal boundary; the three channels are sequential, not simultaneous.

Late/overlapping scans, ADC errors, FIFO errors and ring overflow stop
acquisition and wake the task to assert the IR health fault. A missed alarm
write forces an interrupt instead of waiting for a 32-bit counter wrap.
The task rejects frames older than 15 ms. Its separate 20 ms wait timeout
detects loss of interrupts; this timeout is rounded by the kernel tick and
is not a guaranteed 20 ms fault response. B2 also enforces its configured
line-input maximum age while following.

`ir_runtime_get_status()` exposes raw values, 7-bit normalized values, measured
white/black endpoints, calibration sample count, failed-channel mask, hardware
fault bits, observed maximum launch lateness / scan duration, handoff errors,
motion epoch and status-update time. Hardware counters restart with BEGIN.
No timing or RAM-size result has been recorded yet; inspect actual compiler
layout and stack high-water marks after approval.

## Operator/controller API

Include `ir_runtime.h`. These are task-context APIs, not serial commands.
No calibration or movement request is issued automatically, even in TEST=1.
No blocking `tm_getchar()` loop is introduced into the sampling path.

1. Observe status with `ir_runtime_get_status(&status)`. Startup samples raw
   channels but remains UNCALIBRATED and asserts `MOTION_HEALTH_IR`.
2. Submit `ir_runtime_calibration(IR_RUNTIME_BEGIN)`. An accepted BEGIN asserts
   the IR fault and requests motion stop, clears old calibration, restarts
   acquisition, and enters CALIBRATING. Inspect status until the command is
   no longer pending; E_OK from the API means accepted, not completed.
3. With the car stationary, expose **all three sensors** to both white and
   black. The task accumulates actual min/max values; no endpoint is invented.
4. Submit `ir_runtime_calibration(IR_RUNTIME_FINISH)` and observe completion.
   A channel needs at least `IR_CAL_MIN_CONTRAST` (600 counts). The failure mask
   identifies GP26/GP27/GP28 as bits 0/1/2. Any failure enters FAULT and keeps
   motion blocked. Successful calibration enters READY; only a fresh successful
   bridge handoff clears the live IR source fault and sets `healthy`.
5. B2's fault stays latched. After observing READY, healthy, fresh status, and
   no pending command, the local controller must explicitly submit
   `MOTION_RESET` using the current epoch, then a new motion request. This code
   never automatically resets or resumes the vehicle. Motor/encoder geometry,
   polarity and control calibration in `motion_config.h` remain separate gates.

E_OBJ rejects an already pending/in-progress command or FINISH outside
CALIBRATING. Mutex contention returns a negative error for the caller to handle;
do not assume success. Status copies are coherent but can be stale, so callers
must inspect `updated_ms`. Calibration is RAM-only and must be repeated on boot.
After a hardware, timing or handoff fault, a new BEGIN is the explicit restart
path and requires recalibration. It does not replay previous barcodes.

On observing a changed B2 epoch, the task discards buffered samples, resets
line/barcode context, and accepts only samples acquired **after** observing that
epoch. Calibration completion also discards the buffer. Bridge errors are
treated as faults, including a race with epoch publication; recovery remains
explicit. Under load, mutex contention can cause conservative fault stops;
stress validation is required before treating this as deployment-ready.

## Remaining evidence and reviews

The old S3-U04 result records GP28 stuck around 147-161 counts. No new evidence
shows it fixed. Confirm sensor A0 wiring and the open VBAT jumper before the
approved calibration exercise. Software cannot replace that electrical repair.
The left/right motor mapping is user-confirmed; encoder identities, scale,
wheel geometry, signs and controller gains are still unverified.

Source reviews covered resource conflicts, startup order, ISR/task ownership,
bounded buffering, raw timer wrap, missed alarm writes, calibration failure,
stop/reset epochs, command/health races and non-replay after calibration.
These reviews found and corrected missing BSP timer initialization, a bridge
error path that reported success after a sample gap, and races between a new
calibration command and clearing the IR source fault. Planned validation is in
`testing/subsystem3/TEST_LOG.md` and `testing/integration/TEST_LOG.md`; none of
the new cases has run.

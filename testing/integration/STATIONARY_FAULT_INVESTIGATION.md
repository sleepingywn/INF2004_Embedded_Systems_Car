# Stationary validation investigation — 2026-11-02

Scope: source inspection and analysis of existing read-only serial/telemetry captures. No firmware changes, reflash, fault reset, calibration command or motor command during this investigation.

## Evidence

The flashed image boots, starts subsystem tasks and reads the IMU. Following the hotspot change to 2.4 GHz, it connects and publishes real MQTT telemetry. Analysed 1,127 real telemetry records (excluding the simulated boot 123456): all report motor fault 68, epoch 2, IR timestamp 0 and terrain age 1,159–1,169 ms. No reported event loss. Terrain serial output continues to update with zero I2C errors in the captured window.

Local ignored evidence: tools/dashboard/.local/stationary-startup.log, stationary-wifi-recheck.log, and tools/dashboard/logs/telemetry.jsonl. The observation window begins after the first faults: first received telemetry uptime is 95,135 ms. It cannot locate the original fault instant.

## Confirmed: terrain clock-domain mismatch

motion_now_ms() in subsystem2/motion.c reads the RP2040 hardware timer. S4 publish() and record_event() in subsystem4/task_terrain.c use tk_get_tim(). kernel/tkernel/timer.c initializes its own clock to zero and increments it by the configured 10 ms tick; time_calls.c also permits a system-time offset. These clocks are not interchangeable.

S1 supplies hardware-timer time to S4_AGE_MS(), which subtracts S4's packed kernel timestamp. Every observed record fails the 100 ms age gate. The nearly constant 1.16 s offset, with 10 ms variation, plus continuing sensor output support false staleness rather than an IMU read failure. The exact split between boot-origin offset and previously lost kernel ticks is not measured.

Correction: establish one shared monotonic timestamp API for cross-subsystem data and events; convert all associated consumers and test wraparound. Do not hide the mismatch by raising the stale limit or subtracting a hard-coded 1.16 seconds. This bug was introduced by the telemetry integration's age comparison.

## Confirmed: IR readiness is unreachable in the default build

ir_runtime_init()/ir_task() assert MOTION_HEALTH_IR while uncalibrated. process_frame() emits line data only in IR_RUNTIME_READY. Calibration is RAM-only. The only application caller of ir_runtime_calibration() is the integration console, and APP_CONSOLE defaults to 0 in the flashed profile. Thus no current operator path performs BEGIN/FINISH calibration.

That startup IR interlock is sufficient to explain the external-health bit 64. Source inspection finds no enabled non-IR assertion callers in this build. It is not evidence of a failed motor driver. A current per-source fault mask is not transmitted, so future assertions cannot be diagnosed from aggregate 68 alone.

Correction: add a calibration/diagnostics-only local interface with motor commands unavailable, or integrate an explicit calibration workflow in the controller. Preserve the motion interlock until real calibration and fresh handoff succeed; never simply clear the IR source bit.

IR timestamp 0 means no line frame was handed off; it does not prove ADC sampling has stopped. The existing wire format omits IR runtime state, acquisition fault bits, raw channels, sample count and calibration progress. Prior ring-overflow reports remain relevant but are not confirmed by this capture.

## Confirmed ordering gap; TIMEOUT cause not yet proven

app_main releases motion_application_ready() after task creation/start calls. It does not wait for task initialization to finish. The serial capture prints all subsystems started before the Wi-Fi task starts cyw43_init(). The startup gate therefore does not cover all asynchronous initialization.

Fault bit 4 is latched. With no motor requests and MOTION_CALIBRATED=0, relevant candidates are the five-second startup gate deadline and the motion loop's interval greater than 100 ms. The calibrated odometry timeout returns early in this profile, and command-specific timeout paths have no admitted movement request. The log does not include the triggering interval, reason or timestamp, so assigning this specifically to Wi-Fi would overstate the evidence. Wi-Fi is pinned to core 0 and motion to core 1: priority alone is not proof of a motor delay.

Next diagnostic build should capture first fault time, reason, measured interval, active request/state, live source mask, maximum loop interval, application/driver readiness milestones and IR runtime/acquisition counters. Copy these through a bounded snapshot; do not add blocking printing inside the control loop/ISR. Runtime readiness should distinguish local driver initialization from optional network connection, so an absent access point cannot silently disable timing supervision indefinitely.

## Further stationary checks

Keep the chassis level and still during IMU calibration, even with wheels lifted: the startup log requested two seconds of stationary, level calibration. Later serial readings approach 25 degrees of relative tilt; the captures cannot distinguish physical repositioning from mounting/calibration error.

After reviewed fixes and approval for a diagnostic reflash: verify terrain freshness with the common clock; observe raw IR sampling before calibration; perform explicit IR calibration; identify the first TIMEOUT rather than repeatedly resetting it; then check radio reconnect while monitoring control timing. Motion calibration remains disabled and is a separate later task.

## Fixes prepared after investigation

- Buddy 1 alone now obtains tk_get_tim() for S4 timestamp age. The hardware clock is retained only for the independent unchanged-snapshot watchdog. S4 files and its timestamp contract were left unchanged during this fix.
- The default build starts a separate local diagnostic console: h/? help, i diagnostics, c IR BEGIN, f IR FINISH. All other keys, including motor commands, reset and fault bypass, are ignored. The existing full bench console remains an explicit APP_CONSOLE=1 override.
- Buddy 2 startup release now requires application start acknowledgement and radio init_complete. It does not require successful network association. The existing five-second timeout still latches; it is not extended or suppressed.
- IR acquisition waits for that release before enabling its sampling ring. A six-second bounded wait reports IR timing failure rather than waiting forever. Calibration remains explicit and keeps the motion interlock asserted until fresh calibrated handoff succeeds.
- A polled Buddy 2 diagnostic snapshot retains first timeout reason/time/interval/state, maximum loop interval, live health source bits and readiness milestone times. Printing runs only in the low-priority diagnostic task. Reasons: 1 startup, 2 control loop, 3 encoder interval, 4 request expiry, 5 manoeuvre, 6 reacquisition. Source bits: 1 local, 2 IR, 4 network, 8 terrain.
- The i command also reports IR runtime state (0 uncalibrated, 1 calibrating, 2 ready, 3 fault), acquisition running/fault bits, raw readings, sample count, calibration count, failed channels, handoff errors and timing maxima. Acquisition fault bits: 1 hardware, 2 timing, 4 ADC, 8 overflow.

Host regression checks passed clock offset, packed timestamp wrap, unchanged-snapshot wrap rejection, hardware-time wrap, complete input-byte command allowlist, startup radio readiness and bounded startup timeout. These do not establish the hardware fault is resolved. A reviewed stationary reflash is still required.

## Approved stationary reflash results — 2026-11-02

The revised MQTT, SMP, USB diagnostic image was flashed after the user confirmed readiness with both wheels secured off the surface. The 35-second serial capture sent only `i` diagnostic requests: no calibration, reset, bypass or motor commands. Evidence is in ignored `tools/dashboard/.local/stationary-fixed-startup.log` and the telemetry log (boot ID 674882136).

- Wi-Fi and MQTT connected; status heartbeats were transmitted and received. A subsequent snapshot of 615 telemetry records from this boot reported terrain age 10–20 ms, replacing the previous false 1,159–1,169 ms age. This validates the Buddy 1 clock-domain correction in this observation window.
- All captured motion diagnostic snapshots reported timeout reason 0 and maximum control-loop interval 20 ms. Application readiness was recorded at 3 ms; radio initialization and motion release at 1,715 ms. Telemetry fault 64 remains, without the previous timeout bit 4. This run does not prove all future timing conditions are resolved.
- IR acquisition initially ran successfully, reaching 18,780 frames with zero handoff errors. Its last raw timestamp was 20,516 ms. The final diagnostic reported runtime state 3, acquisition stopped, timing fault 0x2 and maximum launch lateness 968 us. The configured acceptance bound in `ir_acquisition.h` is 800 us, so the recorded launch lateness exceeds it. The code stops acquisition on that condition; the capture does not identify which activity delayed the interrupt.
- The remaining live motion health source is IR (0x2). IR was never calibrated, and acquisition has now also faulted. Calibration should wait until sampling reliability is addressed. Do not clear the interlock or widen the timing limit merely to hide this observation.
- Terrain output continued with zero I2C errors and zero reported overruns. Changing pitch/roll readings do not establish orientation accuracy without a controlled stationary, level check. Calibrated wheel-speed validation remains outstanding.

Next investigation: identify interrupt masking/contention or long interrupt service on core 0 around IR sampling, then prepare a bounded acquisition fix and obtain approval for another hardware validation. No Buddy 4 source change was made for this validation.

## IR follow-up: console interrupt masking — 2026-11-02

Scope: read-only inspection of existing captures, firmware source and build configuration. No new device commands, builds, tests or flash were performed for this follow-up. Only this investigation record was edited.

### Confirmed failure mechanism

`ir_acquisition.c:96–103` updates maximum launch lateness in the timer interrupt and immediately stops acquisition when lateness exceeds `IR_ACQ_MAX_JITTER_US` (800 us). The observed 968 us therefore crosses a real stop condition. The last healthy snapshot and the final fault snapshot both have 18,780 samples and raw timestamp 20,516 ms; no additional complete frame was counted between these snapshots. The healthy snapshot had maximum lateness 494 us, while the final snapshot had 968 us. This points to a delayed launch after the last complete frame, rather than a sensor-contrast problem.

The final fault mask is 2 (timing), with zero handoff errors. It is not the earlier October 31 fault 8 (ring overflow after 322,239 samples). Increasing the 63-frame buffer cannot fix an interrupt that launches too late. Absence of an ADC fault is not proof of correct sensor wiring or calibration, but the captured failure gives no reason to replace the sensors.

### Strongest cause candidate: diagnostic printing

The SMP USB build uses `tm_printf_lock()` in `lib/libtm/libtm_printf.c`. It acquires an interrupt-preserving spinlock before formatting and releases it only after the complete formatted call has been emitted. `ISpinLock()` in `kernel/tkernel/smp_lock.c` disables local interrupts before waiting for the lock. Consequently both lock contention and formatting/output delay local timer and ADC interrupts. `tm_putstring()` also masks interrupts across its complete string. Low task priority does not mitigate these masked intervals.

The new Buddy 1 diagnostic task is pinned to core 0, as are IR sampling and its consumer. Its IR status is a single long `tm_printf()` with fourteen numeric fields. The last healthy snapshot is obtained before this call prints. The next observed snapshot shows the unchanged sample count and the new late-launch fault. This is strong circumstantial evidence that the status output disturbed the measurement. The diagnostic was introduced by this integration work; calling it read-only describes its command behavior, not its timing impact.

The shared print lock also couples output from the two cores. Buddy 4 runs on core 1, so its I2C execution does not by itself mask core-0 interrupts. However, core-0 printing can wait for core-1 printing while locally masked. The terrain log's approximately 1.2 ms `print max` measures the whole multi-call status routine; it must not be presented as a measured single critical-section duration.

No captured timestamp identifies the exact print or lock owner at the fault. Thus the console's interrupt-masking behavior is confirmed, while attribution of this particular 968 us delay to a specific print remains a hypothesis requiring measurement. Kernel critical sections are another candidate: the global kernel lock masks interrupts while waiting and while held, and the ADC ISR calls `tk_set_flg()` after every complete frame. Existing kernel lock hold/wait counters can help distinguish this path. The reviewed Wi-Fi port has no-op thread-enter/exit hooks and task-based polling; Wi-Fi running on the same core alone does not prove it blocked the sampling interrupt.

### Recommended focused change and validation

1. Repair the SMP USB console path so message formatting runs with interrupts enabled in ordinary task context, followed by a bounded enqueue. Preserve cross-core integrity, early-boot and interrupt-context behavior. Do not simply replace the outer spinlock with an interrupt-enabled blocking lock: an interrupt could then deadlock against the task it interrupted. Use explicit bounded buffering and count truncation/drops rather than waiting for USB or an occupied message slot.
2. Keep the Buddy 1 diagnostic snapshot compact and bounded. Add first-fault reason/time/lateness and frame counters in RAM; capture console lock wait/hold maxima and kernel lock statistics without printing inside an ISR or critical section. Include scan overlap, scan timeout, queue-age and watchdog reasons so they no longer share an unexplained timing bit.
3. Leave Buddy 4 sensor logic, the 800 us limit and motor interlocks unchanged for the first comparison. Splitting the new status print is a smaller mitigation, but leaves the shared console mechanism in place and is not a complete correction.
4. After software review/checks, request approval for a stationary reflash. Compare sustained sampling with normal MQTT traffic before diagnostic requests and during repeated requests; monitor raw sample progression, first-fault evidence, queue/handoff failures, motion timeout and telemetry freshness. Repeat with USB output backpressure/disconnection and radio reconnect before declaring integration timing reliable. Hardware tests remain pending.
5. Only after acquisition stays healthy, perform explicit black/white calibration and verify all three channels. Calibration cannot cure the recorded interrupt latency. The documentation in `subsystem3/README_RUNTIME.md` still describes the older 15-frame/100 us candidate and should be reconciled with the current 63-frame/800 us configuration when implementing the fix.

This can be addressed in the shared console and Buddy 1/Buddy 3 diagnostics without editing Buddy 4's implementation. A hardware-paced acquisition redesign is a later option if measured interrupt latency remains incompatible with the sampling contract after the console fix.

## Console correction implemented — software validated, not flashed

The approved change removes the whole-message interrupt-masking print lock from the USB formatter. Each call formats into a private 256-byte buffer with the caller's interrupt state preserved, then submits a whole record through a single software try-lock attempt. A busy/full queue drops the record and counts it. Overlong formatted records end with `...` and CRLF and increment a truncation counter. The USB consumer also yields on lock contention, and early startup no longer resets an already usable print-ring lock. USB `tm_putchar`/`tm_putstring` no longer add an outer interrupt mask. The optional polled UART mirror remains outside the USB timing guarantee.

Buddy 1 prints smaller diagnostic records and exposes console drop/truncation/contention totals and enqueue-time maximum, plus existing kernel lock hold/wait maxima. Enqueue time is a diagnostic proxy: it can include preemption before the try-lock and excludes the final unlock tail. It must not be reported as an exact IRQ-disabled interval. Buddy 3 retains first-fault reason, hardware microsecond timestamp, detail and completed-frame count, including distinct timer, ADC, queue-age and watchdog paths. Task-level fault handling checks the stopped acquisition snapshot so it does not overwrite an already latched IRQ cause. The 800 us limit, ring capacity and motor interlocks are unchanged.

Validation completed:

- Production USB formatter/queue compiled against host stubs: SMP and UP checks passed boundaries, CRLF expansion, truncation, wrap, whole-record drop behavior and caller interrupt-mask preservation. SMP checks also passed busy-lock behavior, nested output and per-core counters.
- Production IR acquisition against a register model passed the exact 800 us boundary, the recorded 968 us failure, distinct overlap/scan/ADC/overflow reasons, first-fault preservation, timer wrap and explicit restart clearing.
- Existing terrain-clock/diagnostic-key allowlist and startup-gate host regressions passed.
- Firmware profile `smp1-usb_cdc-wificyw43-netlwip-mqtt1-crash0-test0` compiled and linked without reported compiler warnings. Size: text 385,024 bytes; data 176; BSS 85,420. UF2 SHA-256: `A9C3E95AC69311E22F7E440BAEA2023A4693AC9E73C4EC5EEA795E15A77B7EE1`.
- Buddy 4 file hashes match the pre-fix baseline. No hardware commands, flash or motor testing were performed while implementing this correction.

Host checks cannot establish real interrupt latency. Next approval request: flash this image with the car secured, observe normal telemetry for one minute, then request diagnostics repeatedly during a further two minutes. No motor, reset, bypass or calibration commands. Success requires advancing IR samples, no acquisition faults or motion timeout, fresh telemetry and acceptable console loss counters. Any timing fault should now identify its first cause; USB backpressure and radio reconnect remain follow-up checks after this initial comparison.

## Approved console-fix hardware validation — 2026-11-02

The user approved the stationary flash and three-minute check. BOOTSEL identified `RPI-RP2`, and the UF2 hash matched the reviewed image above before copying. Serial capture on COM6 lasted 180.029 seconds: no manual commands for the first minute, then 24 `i` requests at roughly five-second intervals. The firmware also printed its automatic startup diagnostic. No motor, calibration, reset or bypass commands were sent.

Evidence (ignored local files): `tools/dashboard/.local/ir-console-fixed-validation.log`, `ir-console-fixed-validation.json`, `ir-console-validation-summary.json`, and receiver telemetry for boot ID 2148120774.

IR/console comparison passed within this window:

- All 25 diagnostic snapshots reported acquisition running, fault mask zero and increasing sample counts. The final snapshot recorded 178,609 completed frames at raw timestamp 180,345 ms; no first fault or handoff error was recorded.
- Maximum reported launch lateness was 143 us, and maximum scan duration was 161 us. These remain below the unchanged 800 us limit. The prior build stopped after 18,780 frames with 968 us launch lateness.
- Console counters reported zero contention drops, zero dropped records/bytes and zero truncation. Maximum enqueue time was 166 us (the proxy described above). Kernel maxima were hold 245 us and wait 66 us.
- Motion diagnostics recorded no timeout and maximum control-loop interval 20 ms. Both encoder counts remained zero in all received frames. The live IR interlock remained asserted because calibration was not performed; aggregate motion fault 64 is expected in this state.
- Terrain serial output reported zero overruns and I2C errors. Received terrain age was 10–20 ms across 618 telemetry records. Orientation stayed approximately level during this capture.

Separate unresolved observation: laptop telemetry receipt was intermittent. Sixteen receipt gaps exceeded one second, with a maximum 2,936 ms. Many gaps separated adjacent firmware sequence numbers whose publication timestamps were only 250 ms apart; some also skipped sequence numbers. This distinguishes receiver delivery timing from the fresh on-device terrain timestamp, but does not locate the delay within Wi-Fi, MQTT, broker or laptop processing. Do not call end-to-end telemetry freshness validated on this evidence.

Conclusion: the console correction survived the approved normal-traffic/repeated-diagnostic comparison and strongly supports the interrupt-masking diagnosis. This is a three-minute stationary result, not a guarantee under every load. Next work is explicit black/white IR calibration, a separate telemetry-delivery investigation, and later approved USB-backpressure/radio-reconnect checks. No additional firmware changes or extended hardware tests were performed after this capture.

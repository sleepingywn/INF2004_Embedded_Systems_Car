# Console and IR timing regression checks

These tests compile the production formatter, USB queue and IR acquisition
source against deterministic host stubs. They validate logic and interrupt-mask
preservation, not real processor timing, electrical behavior or thread scheduling.

From the repository root with GCC available:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -Wno-pointer-to-int-cast -Wno-implicit-fallthrough -Wno-sign-compare -Wno-unterminated-string-initialization -Itesting/console/mock -Iinclude -Iconfig testing/console/console_test.c -o "$env:TEMP/robocar-console-test.exe"
& "$env:TEMP/robocar-console-test.exe"
# Repeat that build/run with -DTK_SUPPORT_SMP=0 for the single-core branch.
gcc -std=c11 -Wall -Wextra -Werror -Itesting/console/mock -Iinclude -Iconfig testing/console/ir_acquisition_test.c -o "$env:TEMP/robocar-ir-test.exe"
& "$env:TEMP/robocar-ir-test.exe"
```

The warning exceptions cover the existing 32-bit formatter's pointer conversion,
switch fallthrough, signed field widths and non-NUL digit lookup table on a
64-bit host. The mock does not emulate the hardware spinlock beneath the kernel
try-lock primitive.

Coverage: formatting and CRLF, exact capacity, visible truncation, buffer wrap,
whole-record drops on full/busy queue, nested output without shared scratch
storage, per-core statistics, caller interrupt-state preservation, and startup
without lock initialization. The UP branch omits SMP-only contention scenarios.
IR checks exercise the unchanged 800 us boundary and recorded 968 us failure,
scan overlap/timeout, ADC and overflow reasons, first-fault preservation, timer
wrap and explicit restart clearing diagnostics.

USB formatted messages now have a 256-byte limit including inserted CRLF.
Truncated records end in `...` and CRLF; `tm_printf` returns -1 for truncation.
A busy/full queue drops the complete record. The `i` console command reports
approximate per-core drop/truncation/contention totals and maximum enqueue time,
plus existing kernel lock hold/wait maxima. Enqueue time includes any preemption
before lock acquisition and excludes the final unlock tail; it is not a precise
measurement of time with interrupts disabled. Single calls remain intact; a
logical line assembled from multiple calls can still interleave at call boundaries.
The optional UART mirror remains polled and is outside this USB latency guarantee.

Before claiming a hardware fix, perform an approved stationary run with normal
MQTT traffic, then repeated `i` requests, and check sample progression, first-fault
evidence and dropped console records. Follow with USB backpressure and Wi-Fi
reconnect checks. No motors or automatic calibration are part of these tests.

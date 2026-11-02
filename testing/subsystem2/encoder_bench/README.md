# Single-wheel powered encoder count test

Prepared 2026-11-02. Standalone Pico SDK firmware; no IR, network or PID.
No motion starts automatically. Only the selected wheel is powered; the other
motor remains commanded stopped. Right motor GP10/11, right encoder GP2/3;
left motor GP8/9, left encoder GP4/5. Forward signs follow motion_config.h.

`r` requests right only; `l` requests left only. Fixed 20% duty and 2000 ms pulse,
followed by 250 ms observation after commanded stop. `s`, space or USB disconnect
stops an active pulse. `h` stops and shows help when idle; `i` reads raw counts
when idle. No motor requests are accepted during acquisition/reporting. Deadline
is polled (~1 ms loop), not an independent guarantee against firmware lockup.

Both wheels must be lifted for this measurement. Count actual output-wheel turns
using tape, including the fraction between start and end positions. Video may be
needed for reliable counting. The 2774 counts/rev estimate is not used anywhere
in this test. Counts/rev = absolute recorded raw delta / observed physical turns;
a whole-turn-only report with an unknown fractional turn is insufficient for a
precise calibration. Right forward raw counts should increase; left should
decrease according to existing observations. Confirm this with actual data.

Uses the production quadrature transition and unsigned-wrap delta functions.
Ambiguous/coalesced edges increment per-wheel invalid counters rather than
inventing counts. 20 ms snapshots (actual microsecond timestamps) are buffered
in RAM and printed only after the motor is stopped. The CSV includes powered
and settling samples. RESULT includes raw deltas and invalid counts; END is a
firmware acknowledgement, not proof of physical stopping. Check that the final
samples have stable counts and invalid counts remain zero before calculating.
Do not infer physical speed/RPM before an independent counts/rev measurement.

First proposed test: user confirms lifted wheels and readiness to count right;
flash, verify identity/settings, send stop, then exactly one `r`. No left command
until the right result is reported and the user is ready for a separate run.
This replaces the main firmware temporarily; restore it after bench work.

## Software validation

Firmware compiled with Pico SDK 2.3.1 / Arm GCC 15.2, `-Wall -Wextra -Werror`.
Host simulation compiles the actual main loop and production quadrature logic;
it passed right-only power, exact nominal deadline, early disconnect and stop,
ignored queued left request, no serial printing while powered, post-stop capture,
sample buffer bounds, quadrature edge rejection and modulo counter wrap. This
model does not establish real GPIO timing, wiring, physical speed or braking.

Host command (from the repository root):

```text
gcc -std=c11 -Wall -Wextra -Werror -Itesting/subsystem2/encoder_bench/mock testing/subsystem2/encoder_bench/host_test.c app_program/subsystem2/motion_logic.c -o <temporary-executable>
```

UF2 SHA-256: `A939EBE34B18492260480FD0E8DFB7939FD278CB1FA95E805F52AFC2086F24D9`.
Built using `%TEMP%/robocar-encoder-build`; the UF2 copy is in the ignored local
`build` directory. Flashed and exercised in the approved right-wheel test below.

## Right-wheel run 1 — 2026-11-02

User confirmed wheels lifted. Verified BOOTSEL board and image hash before flash.
Sent exactly one right-wheel command at 20% for 2000 ms. Firmware reported
2,000,147 us powered; total capture including settling 2,250,954 us. Right raw
delta +3,874, left delta 0, invalid edges 0 on both sides, 115 samples, no buffer
overflow. Counts were unchanged over the last 100 ms of capture. Completion and
final stop acknowledged. Physical full/fractional turn count is pending; do not
infer counts/rev or RPM until the user supplies this independent observation.
Logs and CSV are under ignored tools/dashboard/.local/encoder-right-*.

User observation for right run 1: "a slight little over 1 and a half turn".
This is approximate. Exactly 1.5 turns would give 3874 / 1.5 = 2582.67 counts/rev;
more turns would yield a lower value. No precise endpoint angle was supplied,
so no calibration constant or RPM has been committed from this run. The left
single-wheel script is prepared with the same 20% / 2000 ms settings and awaits
operator readiness; it has not been executed.

## Left-wheel run 1 — 2026-11-02

After user readiness, sent exactly one left-wheel pulse at 20% for 2000 ms.
Firmware reported 2,000,116 us powered and 2,250,897 us total capture. Left raw
delta -3,831 (consistent with the recorded forward sign), right delta 0; zero
invalid transitions on both wheels, 115 samples and no buffer overflow. Counts
were stable over the last 100 ms; completion and final stop acknowledged.
Physical full/fractional turn count is pending. Separate unloaded runs are not
proof of equal loaded wheel speeds or straight travel. Evidence is in ignored
tools/dashboard/.local/encoder-left-* logs/JSON/CSV.

User observed left run 1 as exactly 1.5 output-wheel turns. The provisional
left calibration is 3831 / 1.5 = 2554 x4 counts per output-wheel revolution.
This is one visual measurement; repeatability and fractional-angle precision
remain unverified. Right run 1 was described only as slightly over 1.5 turns,
so it does not yet establish a precise independent right calibration. Do not
assume both wheels share the left constant. motion_config.h remains unchanged.

## Left-wheel run 2 — 2026-11-02

User-authorized repeat: 20%, 2000 ms, wheels lifted. Raw left delta -3841,
right delta 0; zero invalid transitions, 115 samples, no buffer overflow and
stable counts during the final 100 ms. Completion and final stop acknowledged.
User observed slightly more than 1.5 turns. The count magnitude differs from
run 1 by 10 / 3831 = approximately 0.26%, suggesting similar unloaded travel
under these two pulse conditions. It does not establish encoder counts/rev
without an accurate physical-turn reference. Do not divide this observation by
exactly 1.5 or average the inferred calibrations. The prior 2554 value remains
provisional; next measurement should improve angular reference accuracy using
known full turns or a clear video of the starting and ending tape positions.

## Left-wheel run 3 (video requested) - 2026-11-02

After the user confirmed readiness to record, sent exactly one left command
at 20% for 2000 ms. Firmware id 4 reported 2,000,117 us powered and 2,250,895 us
total capture; left raw delta -2680, right delta 0, zero invalid transitions,
115 samples and no overflow. Counts were stable over the final 100 ms;
completion and final stop were acknowledged. Physical turns/video review are
pending. This count magnitude is about 30.2% below the preceding left run
(3841); the cause is not established. Do not infer physical turns from the
provisional calibration or treat fixed-duty pulses as a fixed rotation.
No production calibration settings were changed. Raw evidence is saved under
ignored tools/dashboard/.local/encoder-left-* logs/JSON/CSV.

## Left-wheel run 4 - 2026-11-02

User requested another run. Sent exactly one left command at 20% for 2000 ms.
Firmware id 5: powered 2,000,125 us, total capture 2,250,901 us; left raw delta
-3835, right delta 0, zero invalid transitions, 115 samples and no overflow.
Final 100 ms counts stable; completion and final stop acknowledged. Physical
turn observation/video pending. Count magnitude is within 0.16% of the first
two left runs (3831 and 3841). The preceding 2680-count run remains unexplained;
this repeat does not establish its cause or independently verify counts/rev.
No production calibration settings changed. Raw logs/JSON/CSV saved under
ignored tools/dashboard/.local/encoder-left-*.

Video review - 2026-11-02: user supplied video_2026-11-02_17-04-08.mp4
following firmware run id 5. Inspected sampled frames spanning the entire clip
and enlarged stationary start/end frames. The orange marker starts near the
bottom, passes right/top/left/bottom once, then right/top, ending slightly past
the opposite starting position. Independent visual estimate: approximately
1.5 revolutions, slightly more; no precise fractional-angle calibration claimed.
Camera/chassis motion and perspective limit endpoint measurement accuracy.
Assuming this clip corresponds to id 5 (-3835 counts), exactly 1.5 revolutions
would imply 2556.67 x4 counts/rev; slightly more revolutions implies a lower
value. This broadly supports the earlier provisional estimate, but does not
resolve the 2680-count outlier or establish right-wheel calibration. Production
settings remain unchanged. Extracted review images are in the ignored
tools/dashboard/.local/video-analysis directory.

## Right-wheel run 2 (video requested) - 2026-11-02

User confirmed readiness for the right-wheel recording with wheels lifted.
Sent exactly one right command at 20% for 2000 ms. Firmware id 6 reported
2,000,115 us powered and 2,250,891 us total capture; right raw delta +3835,
left delta 0, zero invalid transitions, 115 samples and no overflow. Counts
were stable over the final 100 ms; completion and final stop acknowledged.
Physical turns/video review pending. The magnitude happens to match left
run id 5, but this alone does not establish equal counts/revolution or loaded
wheel speed. No production calibration settings changed. Evidence saved under
ignored tools/dashboard/.local/encoder-right-* logs/JSON/CSV.

Right-wheel video review - 2026-11-02: supplied
video_2026-11-02_17-06-51.mp4 following firmware run id 6 (+3835 counts).
Reviewed 30 sampled frames across the entire clip and enlarged stationary
start/end frames 61 and 153. Marker travels from bottom through left/top/right,
returns to bottom, then passes left and finishes near top. This independently
supports approximately 1.5 output-wheel turns; no precise endpoint angle is
claimed because camera/chassis movement and perspective affect the view.
Assuming the clip corresponds to id 6, 3835 / 1.5 = 2556.67 x4 counts/revolution
is an approximate estimate, not a verified exact constant. Both latest videos
are consistent with roughly 2550 counts/revolution on each wheel, but do not
establish exact equality, loaded speed matching or straight driving. The prior
2680-count left pulse remains unexplained. Production settings unchanged.
Review images: ignored tools/dashboard/.local/video-analysis/right-overview.jpg
and right-endpoints.jpg. No motor command or flash was performed for this review.

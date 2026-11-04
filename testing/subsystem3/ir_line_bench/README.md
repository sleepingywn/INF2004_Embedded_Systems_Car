# S3 line/barcode logic bench test

Standalone Pico SDK program that runs the **real** S3 logic from
`app_program/subsystem3/` (`sensor_line.c`, `barcode_sense.c`) outside the
micro T-Kernel image:

1. **Unit tests S3-U05..S3-U11** with synthetic readings, printed as
   `[TEST] <ID> PASS|FAIL` (press `t` to re-run).
2. **Live mode:** 1 kHz ADC sampling (repeating timer → lock-free ring), a
   10 ms loop that feeds every sample to the barcode capture and the
   averaged line readings to `line_update()`. Prints CSV
   `t_ms,nBC,nA,nB,err,state,bc_active` at 20 Hz and one `BARCODE ...` line
   per captured barcode.

Wiring is the same as `../ir_bench_test/README.md`.

## Build and flash

Open this folder with the Raspberry Pi Pico VS Code extension (*Import
Project*, board **Pico W**, *Compile Project*), or with the SDK on your PATH:

```sh
cmake -S . -B build -G Ninja -DPICO_BOARD=pico_w
cmake --build build
```

Copy `build/ir_line_bench.uf2` to the `RPI-RP2` drive (hold BOOTSEL while
plugging in USB). The program waits up to 30 s for a serial monitor, runs
the unit tests, then calibrates.

## Using live mode

1. During the 3 s **CALIBRATING** window, slide the car sideways so both line
   sensors cross the black line, and so the barcode sensor crosses a bar.
   Any channel with less than 600 counts of contrast is reported as
   `LOW CONTRAST -> IR FAULT` and reads 0. Press `c` to calibrate again.
2. Line: `err` is −100 (line far right) … 0 (centred) … +100 (line far
   left). `state` is ON, EDGE, LOST_HOLD, LOST, BOTH_BLACK or FAULT.
   In OUTSIDE mode (the default) the sensors sit on white either side of
   the line, so A ≈ B ≈ 0 with `state=ON` is the normal centred reading.
   Build with `-DLINE_MODE=0` for the EDGE-mode variant.
3. Barcode: push the car across a barcode at a steady speed. The capture
   prints the width of each bar/space in ms, bar first.

## How bar widths are measured without a speed sensor

The sensor is sampled at a fixed 1 kHz, so counting consecutive black (or
white) samples gives how long the sensor spent over that bar (or space).
At constant speed that time is proportional to the printed width. Code 39
is read from the **ratio** of wide to narrow elements (≈ 3 : 1), so the
absolute speed cancels out. The car only has to keep a roughly constant
speed while crossing the barcode (≈ 0.5 s at 0.3 m/s), which is why
`line_msg_t.bc_active` asks the Vehicle Controller to hold its speed.

If testing shows the speed is not steady enough, the fallback raised in
Tribunal 1 Version A is to count widths in encoder distance instead of time.
That needs Buddy 2's wheel-distance data and is not implemented yet.

## Barcode terminal milestone

This branch is based on `feature/gp20-adaptive-v5`. Build the standalone
terminal experiment from this folder:

```sh
cmake -S . -B build-terminal -G Ninja -DPICO_BOARD=pico_w -DBARCODE_TERMINAL=ON
cmake --build build-terminal
```

Set `PICO_SDK_PATH` to your SDK first. Output:
`build-terminal/ir_line_bench.uf2`. This image runs the sensor bench, not the
GP20 driving application; move the car by hand for this first milestone.
The GP20 application and its line-following settings are unchanged.

Open USB serial at 115200 baud. Terminal mode loads the shared saved
calibration from `config/line_boot_calibration.h`: GP26 white/black 183/2985,
GP27 173/2623 and GP28 160/2533. There is **no startup calibration sweep**.
After `Ready`, scan
single-character **Code 39** barcodes with start/stop markers: `*A*` prints
`qr3` and `*Z*` prints `qr2`, matching qr3.jpg (A) and qr2.jpg (Z). Most barcode generators add
the stars automatically: enter A or Z, disable the optional checksum,
and retain white quiet space around the barcode. These are printed ID
labels, not QR codes; the single IR sensor reads one-dimensional stripes.
qr1.jpg is excluded: it encodes the same A as qr3.jpg with a damaged first bar.

Scan at roughly constant speed in either direction. `p` toggles live sensor
status (off initially), `c` optionally recalibrates for 3 seconds and `t` reruns the existing capture
tests. Invalid patterns and unused B/C/D report `barcode rejected` rather than an ID.
Startup/calibration/test messages still appear; each accepted scan emits
its ID on a separate line. No route instruction is dispatched by this image.
Physical recognition still needs verification on the car.

### Host regression check

From the repository root with host GCC installed:

```sh
gcc -std=c11 -Wall -Wextra -Werror -Iapp_program/subsystem3 -Iapp_program/subsystem2 testing/subsystem3/barcode_terminal_test.c app_program/subsystem3/barcode_sense.c app_program/subsystem2/motion_logic.c -o barcode_terminal_test
./barcode_terminal_test
```

On 4 November 2026 this passed synthetic A-D, Z and malformed patterns in both
scan directions, plus overflow rejection, through the actual capture and
decoder functions. It does not establish physical scan accuracy.

### Tribunal compatibility

Reviewed `project/SE08_ImmTri1.docx`, `SE08_PostTri1.docx`, and
`Change Requirement 1.docx` (stored outside this repository) on 4 November 2026.
No reviewed requirement prescribes A-D as the printed barcode alphabet.
PostTri1 Version C C01/C12 assigns decoding to Buddy 2 and sensing to Buddy 3;
the bench reuses the decoder in subsystem2 rather than adding a second decoder.
C06 specifies 3 bits for four route instructions plus no instruction, with
codes 5-7 invalid. The change requirement covers minimum-bit application data,
including messages and MQTT payloads, with safe shared access and measurement
evidence. Printed Z is an identity, not an extra route instruction.
`motion_barcode_decode()` still returns only A-D or zero, preserving the
existing route/packed-status interface. `motion_code39_decode()` also recognises
Z for diagnostics. Local USB text labels are diagnostic output; this change
adds no inter-task fields or MQTT payloads and does not establish complete
firmware compliance with the minimum-bit requirement.

The terminal bench is an intermediate recognition test. Final navigation
still needs an agreed mapping, freshness/duplicate handling and physical
validation at the intended speed; the tribunal's navigation requirements
are not fulfilled merely by printing IDs.

Saved references are compiled into the firmware, not learned or written to
flash during this test. A manual `c` calibration changes only the current
session; reboot restores the compiled references. Recalibrate when sensor
height, mounting, lighting or surface changes enough to affect recognition.
Use `p` to check normalized readings: white should be near 0 and black near
100. The original non-terminal bench still calibrates at startup.

### Capture diagnostics v3

The startup banner identifies `Barcode terminal diagnostics v3`.
Every ended capture, including short patterns normally discarded as noise,
prints `SCAN` with element count (expected 29), decoded character, overflow,
and `widths_ms` (B=black bar, S=white space). An incomplete capture ends on
150 ms of white; 255 means a width saturated and cannot be decoded reliably.
Terminal mode uses black-on **40** / white-off **20** as a provisional trial
for the lowered sensor (reported narrow black >=60, wide black >=80).
Readings between 20 and 40 retain the previous black/white state to limit
edge chatter. Verify narrow white gaps fall below 20 at the same height.
Production and the original bench retain defaults of 60/35; the terminal
build alone supplies overrides. The 150 ms quiet timeout and width limits
are unchanged. Flashing/reboot restores the compiled calibration, so use
`c` again for the current mounting before comparing with the latest readings.
Diagnostic events are buffered (four per drain; excess reported) and printed
after draining samples. Watch for ring-overrun warnings; captures spanning
lost samples are unreliable. This diagnostic bench is not evidence of
worst-case production acquisition timing.

For the host test under the terminal thresholds, add
`-DBC_BAR_ON=40u -DBC_BAR_OFF=20u` to the GCC command above.
The test covers threshold boundary hysteresis as well as capture/decoding.

### Slow-scan experiment v4

Terminal mode now uses `BARCODE_SLOW_SCAN`: **1500 ms continuous white** ends
an incomplete capture, compared with 150 ms in production. At 29 elements,
the existing early-finish rule still sends the capture immediately after the
last black bar. A longer timeout does not delay a complete valid capture.
A partial scan may now wait 1.5 seconds before reporting its result.

Its local duration storage is widened to 16 bits (maximum 65535 ms) so slow
bars/spaces no longer saturate at 255 ms. Capture and decoding use the same
width type; no resampling or loss of duration precision is introduced.
Production still uses 8-bit widths and the unchanged compact message format.
This larger test-only representation is not proposed as the final minimum-bit
inter-task format. Slow-scan is guarded against use outside terminal mode.
The 40/20 thresholds remain unchanged. Keep white under the sensor for at
least 1.5 seconds between attempts to discard any unfinished prior scan.

For a host slow-scan test add `-DBARCODE_TERMINAL=1 -DBARCODE_SLOW_SCAN=1`
alongside the 40/20 flags. Tests cover 200/600 ms narrow/wide elements in both
directions (including >255 ms widths), exact timeout, invalid patterns and
preservation of the A-D navigation interface. A longer timeout can combine
unrelated marks; complete Code39 validation remains required for an ID.

### GP20 eight-second straight scan trial v6

The new terminal image **can drive the motors**. GP20 was unused in this
standalone bench (the production line follower already uses it). Power on
stopped; press and release GP20 to request a forward run, press it again to
stop. Automatic motor stop is eight seconds after the request; no auto-repeat.
Calibration/test keys stop movement first. A 5 ms timer enforces the deadline
and button stop independently of serial printing. Lost encoder progress for
400 ms, invalid encoder transitions/control timing, or sampling overruns stop
the run. This is a standalone bench, not the production safety/controller.

Wiring and verified polarity come from subsystem2: motors GP8/9 and GP10/11;
encoders GP4/5 and GP2/3; active-low button GP20. It reuses the existing wheel
speed regulator, requesting 450 counts/s on each wheel, with bounded travel
matching trim and at most 20% PWM. Count-based speed is provisional, not a
calibrated physical velocity or guarantee of straight travel. Validate on the
actual robot; motor power must be available from its normal supply.

Keep sensor height fixed, recalibrate after flashing if needed, and position
the car on white just before the barcode so the GP26 sensor will cross all
stripes. Leave white space between attempts for 1.5 seconds. The run may not
cover an entire large barcode at this speed; report an incomplete capture
rather than assuming the decoder failed. No barcode steering/route action is
executed. Serial IDs remain Z=qr2 and A=qr3.

This supersedes earlier manual-only descriptions for the terminal image.
Hardware direction, speed stability and scanning have not yet been tested
with this v5 image. Existing production GP20 line following is unchanged.

Host check (repository root):

```sh
gcc -std=c11 -Wall -Wextra -Werror -Itesting/subsystem3/scan_drive_mock testing/subsystem3/scan_drive_test.c app_program/subsystem2/motion_logic.c -o scan_drive_test
./scan_drive_test
```

Passed 4 November 2026: initial stopped outputs, debounced press/release,
configured forward polarity, <=20% duty, eight-second timer stop, immediate
button stop, no restart from a held button, stall and invalid-encoder stop.
This uses mocked GPIO/PWM/time and the actual controller header. It validates
software decisions, not wiring, wheel direction, real-time interrupt behavior,
physical speed or straight travel. Pico SDK firmware build also passed.

V6 doubles the requested run duration to 8000 ms. Speed, thresholds and GP20 stop behavior are unchanged.

### White-gap threshold trial v7

The standalone terminal build now enters black at **60 or above** and leaves
black at **45 or below**. Between 45 and 60 it retains its previous state.
Raising white-off above the former black-on 40 would reverse the hysteresis,
so black-on is raised to 60, consistent with the latest reported narrow-bar
levels. Verify both narrow black and narrow white at the fixed scan height;
this is a trial, not a confirmed fix. The eight-second drive, 1.5-second quiet
timeout, slow-scan durations and production thresholds are unchanged.
For host testing, substitute `-DBC_BAR_ON=60u -DBC_BAR_OFF=45u` for 40/20.

### Narrow/wide diagnostic output v8

After each capture, `black_bars` and `white_gaps` list N(narrow), W(wide),
?(unclassified) or G(character separator), with durations in ms. For complete
29-element captures, each of the three nine-element groups is split into
six short and three long elements using the same ordering as the decoder.
`widths valid` means the production width-tolerance check passes; it does not
mean the group matches a supported Code39 character or start/stop marker.
Rejected width groups show explicitly marked estimates if the clusters are
separable, or question marks otherwise. Partial captures are not assigned
N/W labels because the character alignment cannot be established.

White spaces inside each character encode data just as black bars do. The
white separators between characters (positions 10 and 20, counting from one)
should be narrow, while outside white quiet areas frame the barcode. G only
identifies the expected separator position, not a validation of its width.
This change only adds reporting; capture, decoding, thresholds and driving
behavior are unchanged.

### Ordered sequence v9

`sequence:` preserves every element in crossing order, for example
`B:N(100ms) -> S:W(300ms) -> B:W(300ms)`. B is a black bar and S a white
space; N/W/? follow the classification rules above. S:G marks the expected
inter-character separator position in a complete capture, not a verified
width; its exact duration remains visible. This line is printed after capture,
including ? labels and durations for incomplete scans. No detection, motor
or decoding behavior changes.

### Rank-only width experiment v10

The terminal build now defines `BARCODE_RELAXED_WIDTHS`. All width-ratio
consistency checks, including the relative inter-character gap limit, are
bypassed. Each nine-element group uses its three longest elements as wide
and six shortest as narrow. A tie at the narrow/wide boundary is rejected.
Exactly 29 elements, valid nonsaturated widths, no overflow, correct start
and stop markers and a supported character remain mandatory. Reverse scans
are still supported. No barcode navigation action is dispatched.

This intentionally trades noise rejection for recognition of distorted
widths and may increase false IDs. The banner labels it RANK-ONLY and group
output says ratios unchecked. Production keeps its strict checks; compilation
rejects relaxed mode outside the standalone test. Motor behavior, 60/45
thresholds and the 1.5-second quiet timeout are unchanged.

The actual recorded reverse-A scan (B71 S145 ... B220) is now a regression
fixture: strict mode must reject it; rank-only must recognise A in both
directions. Uniform/missing start markers, incomplete captures, overflow
and ambiguous boundary ties must still reject. Test both profiles using the
slow-scan host flags, adding `-DBARCODE_RELAXED_WIDTHS=1` only for this profile.

### Letter output and retry cycle v11

Successful decoding prints the character itself (A, B, C, D or Z), replacing
qr2/qr3. GP20 starts a scan session. A forward pass lasts up to eight seconds.
An incomplete capture containing 5-28 elements requests a retry: brake for
300 ms, reverse by each wheel's measured encoder travel back to the start
of that pass, brake for another 300 ms, then start another forward scan.
This avoids assuming an uncalibrated fixed centimetre distance. Capture is
ignored throughout reverse and the direction-change pauses, and reset before
each forward attempt. Counts are not calibrated distances; floor slip can
prevent exact return to the physical starting point.

Retries continue until a 29-element non-overflow capture or operator/fault
stop. A 29-element capture stops the session even if it fails character
validation; only a valid character prints a letter. Captures shorter than five
never trigger reverse. After the forward duration expires, capture gets two
seconds to finish; no qualifying result stops the session. Reverse has a
10-second ceiling plus existing stall/encoder checks. GP20 cancels during
forward, reverse and pauses. Calibration/test keys and sampling faults also
cancel; a fresh button press/release is required to start again.

Each leg's encoder count is bounded; there is no overall retry count limit.
Automatic reverse assumes a clear return path; this standalone bench does
not add obstacle detection. Real return accuracy and repeated physical scans
still need verification. Existing production motion and message formats are
unchanged. Rank-only decoding remains experimental.

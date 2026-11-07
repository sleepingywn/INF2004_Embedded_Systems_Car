# Bounded per-wheel speed-control bench

Current source and flashed image: **v7 speed PI**.
The robot is stopped; physical trials are recorded below. This remains
standalone Pico SDK firmware: no RTOS, Wi-Fi/dashboard, IR or terrain task.
No production motion configuration or Buddy 4 code was changed for v5.

## Current controller

Motor mapping/forward polarity unchanged: left GP8/9 B-input PWM; right GP10/11
A-input PWM. Encoders left GP4/5 (negative forward raw counts), right GP2/3
(positive). Production quadrature and wrap-safe count helpers are reused.

Each wheel has independent feed-forward plus speed PI feedback. Speeds come
from incremental encoder counts and actual elapsed microseconds, updated about
every 20 ms, with a 40 ms first-order low-pass filter. Units are counts/second
normalised to 2550 x4 counts/output-wheel revolution. The left/right 2550
calibration values remain provisional video estimates, not exact measurements.
No calibrated physical speed or distance is claimed.

Initial targets are 1200 normalised counts/s each, below observed loaded wheel
speeds. To recover an accumulated rotation lead, the leading wheel's target
is reduced by 2 counts/s per normalised count of lead, capped at 120 counts/s.
The other wheel retains 1200 counts/s; neither target is raised above 1200.
This position correction is secondary to the two continuous speed controllers.

Current bench tuning (not measured production gains): feed-forward left 15.5%,
right 18.5% (v7; earlier v5/v6 used 18% on both);
Kp=0.03 permille per count/s; Ki=0.24 permille per (count/s)*second (v6; v5 used 0.12); no derivative.
Integral correction bounded to +/-4 percentage points. Conditional integration
prevents accumulation into both duty saturation and duty slew limits, while
allowing unwinding. State resets on every stop and new run. Both wheels start
at the previously exercised 20% duty, then change by at most 0.5 percentage
point per 20 ms (scaled for actual dt). Active duty stays within 14..20%,
with no reversal. Startup dynamics and gains still need physical validation.

## Motion and fault limits

`f` runs exactly one two-second forward pulse. No motion at boot/reconnection.
`s`, space, `h` or `?` stops; `i` reads raw counts while idle. Movement commands
received during capture are ignored and queued input is drained after reporting.

Both motors stop on the deadline, operator stop, USB disconnect, invalid encoder
transition, forward total below -4, either total reaching 5600 counts, no forward
count progress for 200 ms, main-loop gap above 60 ms, or invalid speed sample.
Speed samples reject dt outside 20..60 ms, negative count increments, and rates
above 10000 counts/s before either controller updates. A 100 ms hardware watchdog
is enabled only during movement. Its physical reset behavior has not been
fault-injected. Stop commands driver inputs LOW; physical coasting is separate.

The 14..20% duty range limits controllable speed. If the target is unreachable
under load, the controller cannot guarantee matched wheel speeds; logged limits
must be reviewed. No added fault silently disables the existing stop checks.

## Buffered diagnostics

No printing while powered. 128 RAM slots cover the two-second run and 250 ms
settling; overflow is reported. Each CSV row contains elapsed microseconds,
raw count deltas, invalid counts, powered flag, left/right duty, filtered speeds,
targets, integral corrections (milli-permille), limit flags and speed_valid.
Limit bits: 1=lower duty, 2=upper duty, 4=output outside permitted slew range.
Speed fields are valid only when speed_valid=1; initial and stopped rows have
invalid zeroed speed-controller fields. Counts continue through settling.

## Software validation

Host simulation executes the actual main loop and verifies deadline, disconnect,
operator stop, left/right stall and reverse faults, invalid edges, travel cap,
late loop, invalid speed/rate/timing, no automatic or queued run, count wrap,
PWM polarity/limits, reset, watchdog arming, no powered printing, buffer bounds,
and the 17-field telemetry schema. All passed for v5, v6 and v7.

Controller tests cover actual-dt conversion, invalid/extreme inputs without
partial state mutation, reset, saturation recovery, integral/output limits and
slew. Five illustrative 30-second motor models cover asymmetric motors, a
left speed change after one second, increased right load after one second,
mirrored asymmetry with timing jitter, and longer mechanical response time.
They converge near 1200 counts/s with bounded rotation mismatch. These are
synthetic plants, not a measured robot model or proof of real-world stability.
Firmware remains capped at two seconds despite the longer host simulations.

From the repository root with host GCC:

```text
gcc -std=c11 -Wall -Wextra -Werror -Itesting/subsystem2/feedback_bench/mock -Itesting/subsystem2/encoder_bench/mock testing/subsystem2/feedback_bench/host_test.c app_program/subsystem2/motion_logic.c -o <host-test-executable>
gcc -std=c11 -Wall -Wextra -Werror testing/subsystem2/feedback_bench/controller_test.c -lm -o <controller-test-executable>
```

Run each executable. Firmware built cleanly with Pico SDK 2.3.1 / Arm GCC 15.2,
`-Wall -Wextra -Werror`, using CMake/Ninja at `%TEMP%/robocar-feedback-build`.
Prepared v5 UF2 SHA-256:
`D2756E5F1C22DE0AAF462D902AB678360BF5242CC6F85140D5963ECD7CE0F82D`.
Local copy: `build/buddy2_feedback_bench_v5_speed_pi.uf2`.
Matching local runner: ignored `tools/dashboard/.local/run_feedback_floor_2s_speed_pi.py`.
It verifies v5 identity/settings before one command, captures the expanded CSV,
checks bounds/completion and sends stop on exit. No automatic movement retry.

## First v5 hardware test protocol (subsequently exercised)

The implementation initially received software validation only. The user then
explicitly authorised flashing and running. Protocol: one two-second forward
pulse on the clear flat floor, cable slack, maximum 20% duty. Compare observed
path with speed targets, filtered speeds, mismatch, limits and stop behavior.
Do not lengthen the physical run or promote gains to production from simulations.

## Earlier bench revisions and physical observations

The following sections describe the earlier rotation-P implementation, not v5.
Initial v1 (500 ms) UF2 SHA-256:
`67C2359A78CCBA294E1FFEF726FDD072B699ACC0DE545A59EC01694822FB6530`.

## Floor run 1 - 2026-11-02

User reported the car was on a flat straight floor. Firmware id 1 completed
after 500707 us powered, with 751508 us total capture. Left raw delta -699,
right +667; zero invalid transitions on both, 40 samples and no overflow.
Final 100 ms counts were stable. Completion and final stop were acknowledged.
Sampled active left duty ranged 19.8% down to 17.0%; right stayed at 20.0%.
Both were initially commanded to 20% before the first periodic sample.

User observation: **Stayed straight**. Left total rotation still exceeded
right by 32 counts. This one short visual result does not establish long-run
straightness, exact speed/distance calibration, or performance under integrated
firmware load. Do not adopt 17% as a fixed trim; it was a feedback output.

Evidence: ignored `tools/dashboard/.local/feedback-floor-20261102-171254`
JSON, CSV and raw log. The local runner sends one motion command, verifies
identity/settings and result, and sends stop on exit; no automatic retry.
This was the initial 500 ms revision. The subsequent v2 test is recorded below.

## Floor run 2: one-second revision - 2026-11-02

User explicitly requested a 1-second check. v2 changes only fixed duration to
1000 ms, associated banner, and travel cap to 2800 counts to accommodate the
longer pulse. Controller gains, 14..20% duty limits, watchdog and other stop
conditions are unchanged. Updated host scenarios and firmware build passed.
UF2 SHA-256: `F70CE4A0B8FBFBC91DB742E50A1A732395E10BD714621FE9D7CFF91914F00A9C`.
Software reboot to BOOTSEL, verified RP2 identity and hash, flashed, verified
v2 banner/settings and sent exactly one forward command on COM5.

Firmware id 1 (reset by reflash): powered 1,000,250 us, total 1,251,048 us.
Left raw delta -1417, right +1391; zero invalid transitions, 65 samples,
no overflow, stable final counts, completion and final stop acknowledged.
Sampled active duty: left 17.2..20.0%, right 19.8..20.0%. Final rotation
mismatch is 26 counts, approximately 1.85% of mean wheel count magnitude.
User observed a tiny rightward veer. This is consistent with the left wheel
recording more rotation; tyre geometry, slip and cable forces remain possible
contributors. Counts alone do not measure the physical path.
Current v2 remains flashed and idle; no repeated movement was sent.
Evidence: ignored `tools/dashboard/.local/feedback-floor-20261102-171541` JSON/CSV/log.

## One-second feedback repeat - 2026-11-02

After operator readiness, repeated the unchanged v2 settings with exactly
one forward command. Firmware id 2: powered 1000164 us, total capture
1250949 us; left raw delta -1466, right +1439, zero invalid transitions,
65 samples and no overflow. Final counts stable; completion and final stop
acknowledged. Sampled active duty left 17.7..20%, right 18.9..20%.
Left final lead 27 counts (~1.86% of mean count magnitude), similar to the
prior 26-count lead. Physical path observation pending. No further run sent.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-171705
JSON/CSV/log.

## Tiny right drift confirmed; v3 prepared - 2026-11-02

User confirmed the second one-second run also veered right by a tiny amount.
Two runs ended with left leads of 26 and 27 counts. Prepared a conservative
25% increase in rotation-error proportional gain (5/4, from 1 permille/count
to 1.25); duration, PWM bounds, slew and fault limits unchanged. This may reduce
residual rotation mismatch; it is not proven to remove physical drift.
No integral action or fixed feed-forward trim was added.

Host scenarios, explicit 20-count -> 25-permille correction check, clamp and
recovery checks passed. Firmware rebuilt successfully with warnings as errors.
Prepared v3 UF2 SHA-256:
`3770130616652F8DA1F09D9D8CD4D40C16187CA28FF7056B894529AED666279D`.
Local copy: build/buddy2_feedback_bench_v3_gain125.uf2. Matching one-command
runner: ignored tools/dashboard/.local/run_feedback_floor_1s_gain125.py.
This revision was subsequently flashed and tested after operator readiness,
as recorded below; production settings remain unchanged.

## v3 gain 1.25 floor run - 2026-11-02

User confirmed readiness. Verified prepared UF2 hash, rebooted the identified
Pico to BOOTSEL, verified RP2 drive identity and flashed v3. The host verified
v3 identity, 1000 ms duration and gain 1.25 before sending exactly one run.
Firmware id 1: powered 1000250 us, total capture 1251050 us; left raw delta
-1466, right +1439, zero invalid transitions, 65 samples, no overflow.
Final counts stable, completion and final stop acknowledged. Sampled active
left duty 17.4..20%, right 20%. The final 27-count lead equals the previous
v2 repeat; no improved final count agreement was demonstrated in this trial.
User physical-path observation pending. v3 remains flashed and stopped.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-171917 JSON/CSV/log.

## v4 two-second floor run - 2026-11-02

User explicitly requested a 2-second run. Changed fixed duration to 2000 ms,
version/banner and travel cap to 5600 counts; gain 1.25, duty bounds, slew,
watchdog and other fault limits unchanged. Updated host checks passed,
including completion timing and sample capacity, and firmware build passed.
UF2 SHA-256: `A885D3AA5569F48C66547D51A30715CDC1368FB660F47732CF619CC4B729E41D`.
Rebooted identified Pico into BOOTSEL, checked image hash/RP2 identity, flashed,
verified v4 banner/settings and sent exactly one forward command.

Firmware id 1: powered 2000304 us, total capture 2251104 us; left raw delta
-2782, right +2739; zero invalid transitions, 115 samples and no overflow.
Final counts stable; completion and final stop acknowledged. Sampled active
left duty 15.5..20%, right 19.5..20%. Final left lead 43 counts, about 1.56%
of mean wheel count magnitude. Physical-path observation pending; no inference
of straightness from count agreement alone. Robot remains stopped on v4.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-172034 JSON/CSV/log.

## Second-second rightward drift analysis - 2026-11-02

User observed rightward veering after the first second of the v4 run.
CSV confirms left lead was 21 counts at 1.005232 s, 36 at powered stop
(2.000304 s), and 43 after settling. Coasting added 7 counts of mismatch;
most mismatch developed while powered. At 1.950037 s, lead was 33 counts
with left duty 15.9%, right 20%. No invalid encoder transitions were reported.
The controller is P-only accumulated-rotation feedback; any nonzero steady
power correction requires a nonzero rotation error. A carefully bounded
integral correction is a candidate for persistent bias, but has not been
implemented or validated. Its benefit/stability must not be assumed. Wheel
calibration uncertainty, slip and tether forces remain unresolved contributors.
User observation is stored with the local capture. No settings changed or
further hardware run sent during this analysis.

## First v5 physical trial - 2026-11-02

User explicitly instructed "run it". Verified v5 image hash, rebooted the
identified Pico into BOOTSEL, verified RP2 identity and flashed. The host
verified v5 banner/settings and initial stop, then sent exactly one command.
Firmware id 1: powered 2000030 us, total 2250854 us; left -2469 / right +2378
raw counts, zero invalid transitions, 114 samples and no overflow. Final
counts stable; completion and final stop acknowledged. Sampled active duty
left 15..20%, right 17.7..20%. Last powered filtered speeds 1169/1138 cps,
targets 1080/1200 cps; integral corrections -26867/+4043 milli-permille.
Each wheel reported an output/slew limit on 3 powered valid samples.

Final left lead 91 counts (~3.75% of mean count magnitude), larger than v4's
43-count lead. This first trial does not demonstrate improved rotation tracking;
the speed controller is not yet tuned to the physical robot. User path
observation pending. No automatic repeat or further parameter change made.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-173230 JSON/CSV/log.

## V5 right drift confirmed; v6 prepared - 2026-11-02

User reported "drift right". Powered data: left lead 29 at 0.503 s, 59 at
1.006 s, 84 at motor stop, 91 after coasting. At 1.006 s left filtered speed
1244 exceeded its 1082 target; right was 1180 against 1200. Thus persistent
speed error during the powered run, not just coasting, contributed to drift.
The integral response was slow over this trial; motor gain estimates remain
uncertain. Prepared one tuning change: Ki 0.12 -> 0.24. Kp, feed-forward,
targets, limits, slew, duration, stop logic and calibration remain unchanged.

Host main-loop checks and five 30-second model scenarios passed. The host
mismatch assertion inherited from rotation-P assumed the left wheel could
never finish behind the right. V6 produced -1 count in that model; corrected
the assertion to bound absolute mismatch by the existing 100-count threshold.
No magnitude tolerance was increased. Two-second model leads were between
-2.2 and +12.7 counts; these synthetic results do not predict the exact robot.
Build passed with warnings as errors. Prepared v6 image SHA-256:
`E52E7BFD2C8A84E64A62D8D8C8F9EE70673D60A68A7FFAD7C9CFFCD8A726EBC0`.
Image: build/buddy2_feedback_bench_v6_speed_pi.uf2. Matching local runner:
tools/dashboard/.local/run_feedback_floor_2s_speed_pi_v6.py.
Not flashed or physically run; awaiting operator readiness.

## V6 first physical trial - 2026-11-02

After operator readiness, verified image hash, rebooted identified Pico into
BOOTSEL, checked RP2 identity and flashed v6. Host verified banner/settings
including Ki=0.24, then sent exactly one two-second command.
Firmware id 1: powered 2000037 us, total 2250862 us; left -2435 / right +2379
raw counts, zero invalid transitions, 114 samples, no overflow. Final counts
stable; completion and final stop acknowledged. Sampled active duty left
14.8..20%, right 17.1..20%. Last powered filtered speeds 1162/1135 cps versus
targets 1094/1200; integral corrections -29733/+5426 milli-permille.
Each wheel had 3 output/slew-limit samples. Final left lead 56 counts, down
from v5's 91 in the preceding trial, but not eliminated. Physical path
observation pending; one comparison does not establish repeatable improvement.
V6 remains flashed and stopped. No automatic repeat sent.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-173534 JSON/CSV/log.

V6 path observation (2026-11-02): user reported "tiny bit of right veering".
This agrees in direction with the remaining 56-count left lead. Compared with
the preceding v5 trial, the count mismatch is lower, but repeatability remains
unverified. Recommend one identical two-second v6 repeat before further tuning.
No controller settings, flash or additional movement changed during this update.

## V6 unchanged two-second repeat - 2026-11-02

After operator readiness, sent exactly one command with unchanged v6 settings;
no reflash. Firmware id 2: powered 2000895 us, total capture 2251707 us;
left raw delta -2453, right +2392, zero invalid transitions, 114 samples,
no overflow. Final counts stable; completion and final stop acknowledged.
Sampled active duty left 14.8..20%, right 16.9..20%. Last powered filtered
speeds 1144/1111 cps, targets 1086/1200; integral -29586/+3846 milli-permille.
Each wheel had 3 output/slew-limit samples. Final left lead 61 counts versus
56 on the prior v6 trial; the residual lead is similar across these two runs.
Physical-path observation pending. Robot stopped; no automatic repeat sent.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-173745 JSON/CSV/log.

## V6 repeat right drift confirmed; v7 prepared - 2026-11-02

User reported the repeat still veered right by a bit. The two v6 runs ended
with left leads 56/61 counts and integral corrections left -29.733/-29.586
permille, right +5.426/+3.846. The common 180-permille feed-forward plus those
learned offsets is about 150 left and 184..185 right. These are transient
endpoint estimates, not measured steady-state motor curves.

Prepared independent provisional feed-forward values: left 155, right 185
permille. The left value retains some correction for PI to learn. This changes
the bias estimate, not the speed targets or gains. Initial energisation still
uses the established 200-permille start, then slews toward the controller's
requested power. PI continues to adjust both outputs within 140..200, resets
between runs, and retains all stop conditions. No fixed motor trim is promoted
to production. No claim that this removes geometric/traction-induced drift.

Actual-main-loop safety checks, reset/feed-forward checks, saturation recovery,
and all five 30-second illustrative models passed. Firmware build passed.
Prepared v7 UF2 SHA-256:
`1C7619A5D6C180FE46E901BF150FCA89C1FB89FA8AF2909E80C8D2636E672A77`.
Image: build/buddy2_feedback_bench_v7_speed_pi.uf2. Matching local runner:
tools/dashboard/.local/run_feedback_floor_2s_speed_pi_v7.py.
Not flashed or run. Await operator readiness for one two-second floor check.

## V7 first physical trial - 2026-11-02

After operator readiness, verified image hash, rebooted identified Pico into
BOOTSEL, checked RP2 identity and flashed v7. Host verified banner/settings
including feed-forward 15.5/18.5%, then sent exactly one two-second command.
Firmware id 1: powered 2000175 us, total 2251000 us; left -2437 / right +2377
raw counts, zero invalid transitions, 114 samples and no overflow. Final
counts stable; completion and final stop acknowledged. Sampled active duty
left 14.6..19.5%, right 16.7..20% (initial commanded startup duty was 20% each).
Last powered filtered speeds 1176/1138 cps, targets 1088/1200; integral
corrections -5602/-1750 milli-permille. Output/slew-limit samples: left 1,
right 4. Final left lead 60 counts, essentially unchanged from v6's 56/61.
This trial does not demonstrate improved final rotation agreement. Physical
path observation pending. Robot stopped on v7; no automatic repeat sent.
Evidence: ignored tools/dashboard/.local/feedback-floor-20261102-174037 JSON/CSV/log.

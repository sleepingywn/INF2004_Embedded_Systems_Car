# One-count settling allowance during a deliberate wheel pause

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

Status: **104EFF... track-tested; no encoder fault in this run, but search
timed out at 9319 ms after losing directional line evidence.**

After the user's flash approval, the sole RPI-RP2 volume and Board-ID were
verified and the exact SHA256 below was checked before copying. Stationary
evidence: `tools/dashboard/.local/gp20-line-search-20261103-220404.{log,json}`.
Only `h` and six `i` commands were sent. All six motion snapshots reported
zero outputs, no faults, and GP20 released/armed. IR samples advanced with no
acquisition or handoff errors. No motor-start command was sent.

## Observed failure

After the user moved the line sensors closer, five centered readings still
reported both-white / LINE_LOST. Separate left-black and right-black checks
correctly reported +83 and -100 respectively, with healthy acquisition.
The subsequent track run stopped at 8209 ms during a tight left turn.

Saved evidence: `tools/dashboard/.local/gp20-closer-curve-stop-20261103-215801.log`
and its JSON metadata. All 64 history rows were recovered. The failure was
ENCODER (0x1), step 4, reason LEFT_RECEDING (0x10), not search timeout or IR loss.
At rejection, the live line error was +97, age 3 ms, invalid-edge count remained
zero, and the left target/output were already zero. Left raw changed from
4294963932 to 4294963933: one backward count under the verified left polarity.
The right wheel continued forward. Final status confirmed both outputs zero.
Mechanical settling and a valid-looking encoder bounce cannot be distinguished
from this record; neither is claimed proven.

## Change and bounds

The line follower permits at most **one total backward count per continuous
intentional inside-wheel pause**. It checks the previous interval's tight-turn
direction, zero target, and zero duty before permitting the sample. Pending
pause entry, ordinary rolling steering, search arcs and the powered outside
wheel retain strict reverse checks. Negative displacement relative to the run
origin remains rejected.

Forward movement or oscillation does not replenish the allowance. A second
backward count, or two counts in one sample, faults normally. The allowance
resets when rolling resumes or a new run begins. A settling sample on the exit
tick may be accepted because the preceding interval was stopped; subsequent
powered intervals are strict immediately.

For the permitted sample only, a local copy of the speed controller uses zero
measured speed for that wheel, then stores the actual signed count. Both wheels
must pass before this copy and the allowance are committed. This avoids a
deferred negative delta on restart without accumulating synthetic distance.
The shared speed controller, encoder ISR, S3 detector and Buddy 4 are unchanged.
Invalid-edge, outside-wheel rate/stall, stale IR, epoch, search bounds, 800 ms
tight-turn timeout, STOP and overall 30-second limits remain active.

## Validation and artifact

All 30 host executables passed using
`tools/dashboard/.local/validate_gp20_pause_settling.py`; output is retained in
`gp20-pause-settling-validation.log` beside it. New tests cover both sides,
one-count acceptance, two-count rejection, bounce without budget renewal,
strict powered/invalid/rate checks, exit/restart, a fresh later pause, reset,
and raw/time wrap. Actual motion-owner fixtures verify single-count acceptance
through manual STOP and second-count fault stops under normal and search builds.

Firmware build succeeded with no compiler warnings/errors:
SMP=1, USB CDC, CYW43/LWIP/MQTT, LINE_DEMO=1, LINE_CIRCUIT=1,
LINE_AUTOSTART=0, LINE_SEARCH=1. Text/data/BSS: 396004/184/90068.

Artifact: `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_search.uf2`

SHA256: `104EFF6B82B7B133DEAC04F36F988A8E2FA218C89CA7452F76159A14B7D5DEEF`

Previous installed image 82ED61... is backed up at
`tools/dashboard/.local/gp20-search-installed-82ed61.uf2`.

## Next physical check

Prompt the user before flashing. With wheels lifted and BOOTSEL ready, install
the verified artifact and read stationary diagnostics only. Then let the user
place the robot on the same track and press/release GP20. Retain the existing
30-second maximum. Preserve power after stopping and collect `i` / `t` evidence
before resetting. Passing this one-count regression does not establish curve
tracking success or justify increasing the allowance if another fault occurs.

## Subsequent track result

User video: `C:/Users/joell/Downloads/Telegram Desktop/video_2026-11-03_22-11-07.mp4`
(13.95 seconds). Sampled frames show progress up the right straight and around
the top left-turn bend, then movement off the line and a stop. Video timestamps
are not firmware elapsed time.

Evidence: `tools/dashboard/.local/gp20-settling-track-20261103-221154.{log,json}`.
Read-only `i` / `t` calls recovered all 64 rows; trace epoch/id 3/2 consistent,
duplicate rows identical. Fault status advanced epoch to 4 after the run.
Final outputs zero; fault IR 0x8, step 11 SEARCH_TIMEOUT, elapsed 9319 ms.
Failure input was fresh LINE_ON / error 0 / age 3 ms, flags 0x7. No encoder
failure record or acquisition/handoff errors. This does not prove the one-count
allowance was exercised; accepted settling counts are not separately logged.

At 6819..7119 ms the left wheel was deliberately paused (target/output zero),
while right target was 500. Actual left speed decayed from 137 to zero.
By 7219 ms a search transition had restarted the left wheel; at 7319 ms its
filtered speed was 716 against a target of 250, while line error was +92.
Normal tracking briefly resumed, but sampled actual errors were zero from
7519 ms onward. Search ran first-left from 7819 ms and switched right at
8319 ms; no further directional evidence appeared before the 9319 ms timeout.

The timeout behaved as designed. The loaded restart overspeed and early
pause-to-search transition are the next investigation targets, not evidence
that increasing the search timeout would solve tracking. The measured speed
surge is a plausible contributor to losing the curve; the trace alone does not
prove causation. No further firmware modification or flashing accompanied this
result review.

# Persistent curve error did not activate tight-turn fallback

> Historical trial record. For current V5 behavior and validation status, see
> [the V5 release guide](ADAPTIVE_V5_RELEASE.md).

## Physical run and captured evidence

Video: `C:/Users/joell/Downloads/Telegram Desktop/video_2026-11-03_23-24-26.mp4`
(14.723 s). The car still runs wide around the upper right bend and stops.
Installed image during this run:
`97415642C22D17D696BB630646F1CD643B9A1F26B5A009F9986A1FDA7DF25CA3`.

The user preserved power and connected USB. Read-only `i`/`t` capture:
`tools/dashboard/.local/gp20-curve-recovery-result-20261103-232544.{log,json}`.
All 64 retained samples were collected, epoch/id 3/2, with no inconsistent
duplicates. The initial buffered startup status is not the final run status.
Fresh diagnostics show final fault 0x8, step 11 SEARCH_TIMEOUT, elapsed
11146 ms, fresh error zero and both motor outputs zero. No new motion, reset,
calibration or flash commands were sent while collecting evidence.

The retained trace starts at 4906 ms, so initial startup is not available.
Across nine samples at 8306..9106 ms, the requested speeds were 250/600 cps;
average filtered measured speeds were 262.3/595.8 cps. Thus the wheels were
close to their requested speeds while the trajectory still ran wide. Motor
feedback tuning alone is insufficient; the requested rolling differential
did not produce enough turning on this particular track. This does not prove
exact wheel radius, floor grip, motor health under all loads or track geometry.

The highest retained error magnitude was 76, below the old tight-turn entry
threshold of 85. Every retained row has tight mode zero. Errors around the bend
persisted in the 45..76 range. The 100 ms trace does not rule out brief unseen
peaks, but there is no evidence of the fallback engaging in this interval.
At 9406..9606 ms, 250/500 continuation was actually commanded, followed by
250/550 leftward search and then 550/250 rightward search. All retained errors
from 9406 ms onward were zero. Recovery expired at 11146 ms.

Fresh post-stop GP28 readings were 213..241, not the earlier near-40 failure
level; acquisition and handoff faults were absent. These are not a new
black/white contrast test. The evidence points to insufficient turn command
and an overly high fallback trigger, not recurrence of the demonstrated stuck
GP28 condition. Exact physical sensor footprints remain partially occluded.

## Targeted correction

Search-enabled GP20 profile only:

- Arm tight-turn confirmation at magnitude 55 instead of 85.
- Maintain the pending confirmation through same-side readings down to 45;
  require the existing 120 ms dwell. A lower reading cancels it; an opposite
  reading must start its own confirmation if it meets the entry threshold.
- This invokes the existing inside-wheel zero-output pause and outside-wheel
  500 cps target. It does not increase the cruise speed or motor power limit.
- Preserve 800 ms maximum tight-turn duration, 100 ms exit dwell at magnitude
  at most 45, immediate exit on opposite evidence, 300 ms cooldown, bounded
  ambiguity handling, soft restart, pause encoder settling allowance and all
  stop/stall/freshness guards. Other profiles retain their original 85 trigger.

This addresses the missed fallback; it is not proof of successful physical
cornering. Lower entry can cause more deliberate pauses if real error remains
large. Short spikes and alternating signs cannot accumulate the entry dwell.
The next trial must check both curve retention and unwanted stop/start motion.

## Validation and candidate

All 34 software test executables passed in this turn: existing 32-profile suite,
updated rolling/curve-recovery regression and new `line_curve_trigger_test.c`.
The new regression holds the recorded 6106..6906 ms errors between their
100 ms samples as a test input, not a reconstruction of missing samples.
It verifies entry 120 ms after the first qualifying 58 reading, zero output on
the correct inside wheel, both directions, noise rejection, confirmation
hysteresis, exit/cooldown, opposite evidence, timeout, reset and timer wrap.
Rolling-only model checks use error 54, below the new entry threshold; sustained
stronger readings now deliberately invoke the pause and have separate tests.

Build completed without compiler warnings/errors. Text/data/BSS:
397392/184/90084 bytes. Profile SMP1 / USB CDC / CYW43 / LWIP / MQTT /
LINE_DEMO=1 / LINE_CIRCUIT=1 / LINE_AUTOSTART=0 / LINE_SEARCH=1.

Candidate artifact:
`build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_search.uf2`

SHA256: `EFC9109BDB1EBC2C6FD1D3DB90A32B15D1163723499BDB9456EE8E2F79FFB429`

Current installed image was hash-verified and backed up before the build:
`tools/dashboard/.local/gp20-curve-recovery-installed-974156.uf2`.

Build log: `tools/dashboard/.local/gp20-curve-trigger-build.log`.
The user subsequently approved flashing. The sole RPI-RP2 volume on E:,
Board-ID RPI-RP2 and the candidate hash above were verified before copying.
USB serial returned and the updated tight-turn, startup, curve and search
banners matched. Evidence:
`tools/dashboard/.local/gp20-curve-trigger-stationary-20261103-233127.{log,json}`.
Six snapshots showed zero motor outputs, no motion faults and GP20
unpressed/armed. IR samples increased with no reported acquisition fault;
stationary GP28 raw was 213..244 (not a contrast calibration). Five LED samples
were armed and one transient observer-busy, without an associated fault.
Only help/diagnostic commands were sent; no motor-start commands or driving test.
GP20 remains the explicit physical start trigger; no automatic motor start.

## First physical trial after the trigger change

Video: `C:/Users/joell/Downloads/Telegram Desktop/video_2026-11-03_23-34-42.mp4`
(14.308 s). The car makes more progress around the upper bend and turns farther
left before stopping. Exact sensor footprints are obscured by the wiring, so
the video alone cannot establish both sensors' optical coverage at the stop.

The user confirmed continuous power and USB connection. Read-only evidence:
`tools/dashboard/.local/gp20-curve-recovery-result-20261103-233533.{log,json}`.
All 64 rows, epoch/id 3/2, collected with identical duplicate rows. Retained
history starts at 5418 ms; buffered early boot status is not the final fault.
No motion/reset/calibration/flash command was sent during this investigation.

The terminal sample at 11638 ms is state 5 (LINE_BOTH_BLACK), error 0,
step 3 (LINE_STEP_IR), input age 3 ms. Fresh diagnostics confirm fault 0x8 and
final motor outputs zero. Search phase remains zero throughout the retained
run. This is neither SEARCH_TIMEOUT nor a motor-stall, encoder, tight-turn
timeout or 30-second run-expiry stop. Acquisition and handoff errors are absent.

Tight turns now activate repeatedly. For example, at 6518 ms the targets are
0/500, duty 0/103 and tight direction +1; later pauses also slow the left wheel
to near zero while the right wheel continues. Those left-wheel stops are
intentional turning commands. Several pauses end on opposite real errors,
followed by renewed positive error and another pause: tracking still oscillates
and is not established as smooth or successful over the whole circuit.

Near the end:

| Firmware elapsed ms | Line state / error | Target L/R | Filtered speed L/R |
| --- | --- | --- | --- |
| 11218 | EDGE_ONLY / +98 | 0 / 500 | 82 / 514 |
| 11418 | EDGE_ONLY / +96 | 0 / 500 | 1 / 458 |
| 11518 | EDGE_ONLY / +67 | 0 / 500 | 0 / 474 |
| 11618 | EDGE_ONLY / +11 | 0 / 500 | 0 / 450 |
| 11638 | BOTH_BLACK / 0, rejected | 0 / 500 (previous target) | 0 / 450 (previous speed) |

The last trace preserves prior targets/speeds because input rejection precedes
the next speed update. It is not evidence of powered motors after the stop;
fresh motor diagnostics show duty 0/0.

`sensor_line.c` classifies both normalized readings >=75 as BOTH_BLACK
immediately, returning error zero. `line_follow_usable()` excludes this state,
so the motor owner stops at the first observed candidate without a junction
confirmation dwell. A curve crossing or short overlap can therefore trigger
the same stop as a junction. Raw readings at that precise instant are not
retained; geometry versus a transient reading cannot be isolated from this
trace. Later idle readings do not replace the missing simultaneous raw values.

Next recommendation: distinguish a brief both-black transition during tracking
from a sustained junction, using an explicitly bounded confirmation policy and
retaining real sensor-fault/stale-input/STOP protection. Do not simply remove
the both-black guard or increase the run timer. No firmware change was made
as part of this video/stop-reason investigation; EFC9109B remains installed.

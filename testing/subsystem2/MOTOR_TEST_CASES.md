# Buddy 2 motor test cases

Use the current A0 adaptive build as the baseline for the faster-robot branch. Wheel targets are in encoder counts per second (counts/s). A commanded target and a measured wheel speed are separate observations.

## Repeatable software tests

Run these from the repository root with Python and GCC on PATH. The host tests exercise firmware logic with simulated inputs.

| ID | Stimulus | Pass criterion | Runner |
|---|---|---|---|
| M01 | Feed equal visible IR levels (20, 20) at 20 ms intervals until settled. | Left and right targets both equal the configured cruise speed (450 counts/s on the baseline). | Adaptive |
| M02 | Hold left-minus-right errors of ±20, ±30, ±40 and ±60 points, one case at a time. | Settled steering magnitude is within 2 counts/s of 70, 105, 150 and 240 respectively. Swapping sensors swaps the slower wheel. | Adaptive |
| M03 | Hold a strong one-sided error, then return to balanced visible readings. | Inside wheel pauses after confirmation, outside target stays at or below 600 counts/s, and both resume rolling after centering. | Adaptive |
| M04 | After a confirmed bend, feed white to both sensors. Then repeat with the line reappearing. | Last correction persists through 40 counts, then fades. At 120 counts of continued uncertainty the bounded search begins. Visible line input can steer the wheels again. | Adaptive |
| M05 | Supply persistent overspeed, then underspeed, then a zero target to the wheel controller. | Duty falls for overspeed, recovers for underspeed within its cap, and zero target clears duty and integral. | Regression |
| M06 | Feed a small backward encoder movement during a pivot, then separately replay stalled powered-wheel feedback. | Rollback alone is tolerated in the adaptive profile. Sustained lack of progress still triggers the stall guard and stops the drive. | Adaptive |
| M07 | Replay a remote start with heartbeats, then interrupt heartbeats. Separately replay a local GP20 run. | Remote motion expires on heartbeat loss and both outputs go to zero. A leftover remote guard cannot stop a local run. | Remote |
| M08 | Run the unlimited profile beyond 60 seconds with valid input and a healthy lease for a remote start. | Local and remote runs continue beyond the former duration limit. Removing the remote heartbeat still stops the remote run. | Remote |

~~~sh
python testing/subsystem2/run_adaptive_tests.py
python testing/subsystem2/run_regression_tests.py
python testing/subsystem2/run_remote_tests.py
python testing/subsystem2/run_route_tests.py
~~~

## Physical tests to record

Record the firmware commit, cruise setting, battery condition, track layout and run ID. Use the same layout and battery conditions for the baseline and candidate speed. Capture left/right targets, encoder counts, duty and stop reason from the motor trace where available.

| ID | Setup and action | Pass criterion and evidence |
|---|---|---|
| M09 · Wheel identity | With wheels clear and drive stopped, rotate one wheel forward by hand, then the other. Mark one revolution per wheel. | Only the corresponding wheel count changes substantially. Record raw sign and counts per marked revolution separately for each wheel. |
| M10 · Loaded straight | Start from the same mark on a straight section. Record a settled interval and its duration. | Both wheels advance and the car stays on the line without a fault. Compute each wheel’s measured counts/s from its count change and elapsed time, accounting for the left encoder’s sign. Report deviations from target. |
| M11 · Tight bend at speed | Traverse the same tight bend three times in each direction at baseline, then repeat at the candidate speed. | For each direction, record successful exits out of three. A pass follows the bend onto the exit straight without manual correction, an unintended stop or leaving the track. Record elapsed time to compare speed. |
| M12 · Barcode U-turn | Approach a mapped U-turn and log wheel counts and both IR readings. Repeat on a test layout without a return line. | On the marked layout the robot pivots, detects the return line and resumes following it. On the missing-line layout it reaches the bounded search limit and stops both motors. |
| M13 · Stop and restart | Issue dashboard Stop during a run, wait for rest, then start a new run using GP20. | Both drive outputs reach zero after Stop. Motion resumes only on the new start. Record stop-command time, zero-duty time and any coast distance separately. |

For each physical case record the actual result, pass/fail and log or video reference. The three repetitions per direction are a proposed comparison protocol. Physical cases are pending until exercised on the car.

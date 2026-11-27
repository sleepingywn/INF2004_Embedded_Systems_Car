# Buddy 3 test cases for line tracing and barcode sensing

Use the A0 route build with the barcode sensor on GP26, left line sensor on GP27 and right line sensor on GP28. The circuit image loads stored calibration references at startup. The calibration test below exercises the calibration routine separately.

Buddy 3 supplies line state, signed error, sensor levels and alternating barcode widths. Buddy 2 consumes those observations to steer, decode a letter and select a mapped action. The dashboard shows the resulting barcode capture count and letter. Record these stages separately when diagnosing a failed run.

## Software cases

| ID | Input or action | Pass criterion |
|---|---|---|
| S3-01 Calibration boundary | Feed ADC endpoints 200 and 800 on GP26/27 and 200 and 799 on GP28. | Only GP28 fails the 600-count minimum contrast. GP27 midpoint 500 normalises to 50. The failed channel normalises to zero. |
| S3-02 Left and right edges | Feed normalised left/right levels (75,15), then (15,75). | Both report an edge. Signed error changes from +60 to −60, distinguishing the two steering directions. |
| S3-03 Junction | Feed (75,75). | State is BOTH_BLACK with error zero. The state distinguishes a junction candidate from centred travel. |
| S3-04 Brief white, line loss and reacquisition | With outside-mounted sensors, feed (15,15) for 1,999 ms, then 1 ms more. Follow with (70,20) and another 10 ms of white. | Zero error and ON through 1,999 ms, then LOST at 2,000 ms. The visible sample restores ON with +50 error and resets the white timer. This checks the sensor report; motor recovery has separate travel limits. |
| S3-05 Complete A0 capture | Feed raw white 177, narrow black 307 and wide black 1309 in a known Code 39 *A* pattern. Use 30 samples per narrow element and 90 per wide. | No early message. The transition out of the final bar publishes once, with all 29 widths in order, overflow clear and the time-width decoder returning A. |
| S3-06 Full count with no letter | Immediately follow S3-05 with 29 equal-width alternating elements. | Capture sequence advances once and count is still 29. Decoder returns no letter. A valid element count alone cannot select a route. |
| S3-07 Flicker and blurred gaps | Replay the recorded 6 ms dip in a bar, shallow 30 ms sag and gap rebound from the analogue level test. | Flicker and shallow sag preserve the bar; the rebound stays a space. A genuine subsequent bar is detected. |
| S3-08 Recorded track scans | Run the 21 recorded pairs of time and encoder widths through the route decoder. | Each accepted letter matches the printed label. Report rejected scans separately. Current replay produced 14 strict decodes, 2 additional decodes with agreement and 5 unreadable scans; zero wrong letters. |
| S3-09 Handoff to dashboard | Replay the owner snapshot and binary wire fixtures for successful, short and ignored captures. | A and Z reach their expected snapshots. A reportable failed capture clears the old letter and records its own count. An ignored three-element blip does not advance the snapshot sequence. The wire decoder preserves the letter and sample timestamp. |

From the repo root with GCC, Python and Node.js available:

~~~sh
python testing/subsystem2/run_route_tests.py
python testing/subsystem1/run_barcode_telemetry_tests.py
~~~

S3-01 through S3-06 are in line_barcode_handoff_test.c beside this document. S3-07 and S3-08 use barcode_level_test.c and barcode_replay_test.c. S3-09 runs in the barcode telemetry suite.

## Physical cases to run

| ID | Procedure | Pass criterion and evidence |
|---|---|---|
| S3-10 Channel identity | At normal ride height, move a black strip under GP27 alone, then GP28 alone, leaving GP26 on white. | Left-only gives positive error and right-only negative. Record both raw and normalised readings. No new barcode scan appears. |
| S3-11 Bend and recovery | Drive the same tight bend left and right, then repeat across a short white break. Record both sensor levels and wheel targets around the break. | Before the break, the darker side agrees with the signed error. On reacquisition the levels and error follow the visible edge again. Record separately whether the wheels followed the requested correction and the car exited the bend. |
| S3-12 Printed A and Z | Make five passes over each known label at the baseline speed and repeat at the candidate speed. Keep sensor height and direction fixed. Start and end on white margins. | For each labelled pass, a full success is one new scan with the expected letter. Count correct, wrong, unreadable and missed passes separately. Any wrong letter fails the identity check. Record the count, time widths, encoder widths when available and dashboard sequence. |
| S3-13 Partial read followed by good read | Stop a card swipe after five complete elements ending in a bar, hold the trailing white for at least 1.5 s, then swipe a complete A. | First report has five elements and no letter. The next scan has a new sequence and decodes A independently. Record the sequence and decoded value for each. |

For each physical run record firmware commit, run ID, surface, sensor height, speed setting, actual observation and pass/fail. Use the raw ADC trace when progress stays at zero and the element widths when the count reaches 29 without a letter. Physical cases are pending until run on the robot.

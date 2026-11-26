# Videos

Short videos of tests and demos running on the real hardware. A video shows
what a log cannot: the LED blinking, the motor turning, the robot reaching
the line. List **every** video in the table at the bottom of this page.

## What to record

- **Short:** 15–60 seconds. One test or one feature per video.
- **Say what it is.** Show or say the test ID (e.g. `S2-U03`, `INT-01`) or
  the demo name at the start.
- **Show the commit.** Before recording, run `git rev-parse --short HEAD` and
  keep the terminal in shot for a moment, or put it in the table below.
- **Show the result.** Keep both the hardware and, if it matters, the
  console output in view.
- **Landscape**, steady, good light. No music needed.

## How to share it: unlisted YouTube only

1. Upload the video to YouTube with visibility set to **Unlisted**: anyone
   with the link can watch it, but it does not show up in search. Not
   *Private* (only invited accounts can watch, so the link won't work for
   your team or instructor), and not *Public*.
2. Copy the video's link (e.g. `https://youtu.be/...`).

**Do not commit video files to this repo or attach them on GitHub.** A video
in the repo stays in the git history for good, even if deleted later, and
slows every clone for the whole team. Video files are ignored by
`.gitignore` for this reason.

Then add a row to the table below. If the video is evidence for a test, also
put its link in the *Log / evidence* column of that test's row in
`testing/.../TEST_LOG.md`.

## Video index

Newest at the bottom. Never delete a row.

| Date | Subsystem | Test ID / demo | Commit | Student ID | Link | Notes |
|---|---|---|---|---|---|---|
| YYYY-MM-DD | S1 | S1-U03 | `abc1234` | | <https://youtu.be/...> | LED blinks at 2 Hz |
| 2026-11-26 (indexed) | S1–S4 | SE08 test case compilation | Decoder `ee763d4`; physical builds unverified | Unrecorded | [Watch the unlisted compilation](https://youtu.be/IBY-M7DeD0M) | 4:01. Wireless startup [0:00](https://youtu.be/IBY-M7DeD0M?t=0), decoder [0:20](https://youtu.be/IBY-M7DeD0M?t=20), stop/restart [0:31](https://youtu.be/IBY-M7DeD0M?t=31), bends [0:48](https://youtu.be/IBY-M7DeD0M?t=48), Z decode [1:43](https://youtu.be/IBY-M7DeD0M?t=103), U-turn [2:00](https://youtu.be/IBY-M7DeD0M?t=120), route setting [2:25](https://youtu.be/IBY-M7DeD0M?t=145), hump height [2:46](https://youtu.be/IBY-M7DeD0M?t=166), lift [3:29](https://youtu.be/IBY-M7DeD0M?t=209), heading [3:41](https://youtu.be/IBY-M7DeD0M?t=221), overturn [3:54](https://youtu.be/IBY-M7DeD0M?t=234). |

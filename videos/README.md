# Videos

## Video index

| Date indexed | Team | Test-case video | Duration | Build recorded |
|---|---|---|---|---|
| 2026-11-26 | SE08, Buddies 1–4 | [SE08 PicoCar Test Cases](https://youtu.be/IBY-M7DeD0M) | 4:01 | Dashboard decoder `ee763d4`. The hardware clips do not identify a firmware commit. |

## Chapters

| Start | Test ID | What the recording shows |
|---|---|---|
| [0:00](https://youtu.be/IBY-M7DeD0M?t=0) | B1-01 / B1-10 | Wireless startup and dashboard connection |
| [0:20](https://youtu.be/IBY-M7DeD0M?t=20) | B1-07 | Dashboard decoder passes four host checks |
| [0:31](https://youtu.be/IBY-M7DeD0M?t=31) | M13 | Dashboard stop and onboard restart in separate runs |
| [0:48](https://youtu.be/IBY-M7DeD0M?t=48) | M11 / S3-11 | Line tracking through left and right bends |
| [1:43](https://youtu.be/IBY-M7DeD0M?t=103) | S3-12 | Dashboard shows 29/29 and the decoded letter Z |
| [2:00](https://youtu.be/IBY-M7DeD0M?t=120) | M12 | Junction U-turn and return along the incoming line |
| [2:25](https://youtu.be/IBY-M7DeD0M?t=145) | — | Barcode route setting on the dashboard |
| [2:46](https://youtu.be/IBY-M7DeD0M?t=166) | S4-U12 | Hump detection and height measurement |
| [3:29](https://youtu.be/IBY-M7DeD0M?t=209) | S4-U10 | Lift detection and stop report |
| [3:41](https://youtu.be/IBY-M7DeD0M?t=221) | S4-U19 | Serial heading reading during a turn |
| [3:54](https://youtu.be/IBY-M7DeD0M?t=234) | S4-U10 | Overturn detection and stop report |

## What to record

- **Short:** 15–60 seconds. One test or one feature per video.
- **Say what it is.** Show or say the test ID (e.g. `S2-U03`, `INT-01`) or
  the demo name at the start.
- **Show the commit.** Before recording, run `git rev-parse --short HEAD` and
  keep the terminal in shot for a moment, or put it in the video index above.
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

Then add a row to the video index above. If the video is evidence for a test, also
put its link in the *Log / evidence* column of that test's row in
`testing/.../TEST_LOG.md`.

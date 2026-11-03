"""Software-only checks; never opens a serial port or flashes a device.

Run from any directory with GCC on PATH:
    python testing/subsystem2/run_adaptive_tests.py
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-DMOTION_LINE_DEMO=1",
         "-DMOTION_LINE_CIRCUIT=1", "-DMOTION_LINE_ADAPTIVE=1"]
OWNER = ["-Wno-type-limits", "-Wno-sign-compare", "-Iinclude"]
LOGIC = ["app_program/subsystem2/motion_logic.c"]
CHECKS = {
    "pause-settling": ["testing/subsystem2/line_pause_settling_test.c", *LOGIC],
    "crossing": ["testing/subsystem2/line_crossing_test.c", *LOGIC],
    "v2-steering": ["testing/subsystem2/line_adaptive_baseline_test.c", *LOGIC],
    "continuity": ["testing/subsystem2/line_adaptive_continuity_test.c"],
    "controller": ["testing/subsystem2/line_adaptive_test.c", *LOGIC],
    "owner": [*OWNER, "testing/subsystem2/line_adaptive_owner_test.c", *LOGIC],
    "handoff": [*OWNER, "testing/subsystem2/ir_handoff_integration_test.c", *LOGIC,
                "app_program/subsystem3/sensor_line.c", "app_program/subsystem3/barcode_sense.c"],
    "console": ["-Iinclude", "testing/subsystem2/line_console_test.c"],
}

def main():
    with tempfile.TemporaryDirectory(prefix="robocar-adaptive-") as temp:
        for name, sources in CHECKS.items():
            executable = str(Path(temp) / (name + ".exe"))
            subprocess.run(["gcc", *FLAGS, *sources, "-o", executable], cwd=ROOT, check=True)
            subprocess.run([executable], cwd=ROOT, check=True)
        for invalid in ["-DMOTION_LINE_SEARCH=1", "-DMOTION_LINE_AUTOSTART=1"]:
            result = subprocess.run(["gcc", *FLAGS, invalid, "-I.", "-x", "c", "-fsyntax-only", "-"],
                                    cwd=ROOT, input='#include "app_program/subsystem2/motion_config.h"\n',
                                    text=True, capture_output=True)
            assert result.returncode and "Adaptive control requires" in result.stderr, result.stderr
        print("PASS: eight adaptive test executables and two invalid-profile checks; no hardware accessed")

if __name__ == "__main__":
    main()

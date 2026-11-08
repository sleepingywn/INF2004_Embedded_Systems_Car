"""Host checks for the LINE_ROUTE profile (barcode -> junction turn -> V5).

Run from any directory with GCC on PATH:
    python testing/subsystem2/run_route_tests.py
Also runs the existing adaptive (V5) and regression suites, which must stay
unchanged with LINE_ROUTE off. No hardware access.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-DMOTION_LINE_DEMO=1", "-DMOTION_LINE_CIRCUIT=1",
         "-DMOTION_LINE_ADAPTIVE=1", "-DMOTION_LINE_ROUTE=1", "-DBARCODE_SLOW_SCAN=1", "-DBARCODE_WIDE=1"]
OWNER = ["-Wno-type-limits", "-Wno-sign-compare", "-Iinclude"]


def main():
    with tempfile.TemporaryDirectory(prefix="robocar-route-") as temp:
        exe = str(Path(temp) / "route.exe")
        subprocess.run(["gcc", *FLAGS, *OWNER, "testing/subsystem2/line_route_test.c",
                        "app_program/subsystem2/motion_logic.c", "-o", exe], cwd=ROOT, check=True)
        subprocess.run([exe], cwd=ROOT, check=True)
        # Barcode capture with 16-bit widths and the 1.5 s quiet time.
        exe = str(Path(temp) / "handoff.exe")
        subprocess.run(["gcc", *FLAGS, *OWNER, "testing/subsystem2/ir_handoff_integration_test.c",
                        "app_program/subsystem2/motion_logic.c", "app_program/subsystem3/sensor_line.c",
                        "app_program/subsystem3/barcode_sense.c", "-o", exe], cwd=ROOT, check=True)
        subprocess.run([exe], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        print("PASS route-profile IR handoff (16-bit barcode widths)")
        # LINE_ROUTE requires LINE_ADAPTIVE.
        result = subprocess.run(["gcc", "-std=c11", "-DMOTION_LINE_ROUTE=1", "-I.", "-x", "c", "-fsyntax-only", "-"],
                                cwd=ROOT, input='#include "app_program/subsystem2/motion_config.h"\n',
                                text=True, capture_output=True)
        assert result.returncode and "requires MOTION_LINE_ADAPTIVE" in result.stderr, result.stderr
    for suite in ["run_adaptive_tests.py", "run_regression_tests.py"]:
        subprocess.run([sys.executable, str(ROOT / "testing/subsystem2" / suite)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        print("PASS", suite)
    print("PASS: route logic + owner scenarios, route-profile handoff, profile guard, V5 adaptive and regression suites")


if __name__ == "__main__":
    main()

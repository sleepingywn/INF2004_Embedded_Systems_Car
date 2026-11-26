"""Build and run the IMU simulations with the current host's native GCC."""

from pathlib import Path
import os
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / "app_program" / "subsystem4"


def main():
    with tempfile.TemporaryDirectory(prefix="se08-terrain-") as directory:
        for name, implementation in (
            ("math_check", None),
            ("pack_check", "terrain_pack.c"),
            ("lift_check", "imu_logic.c"),
            ("imu_sim", "imu_logic.c"),
        ):
            binary = Path(directory) / (name + (".exe" if os.name == "nt" else ""))
            sources = [HERE / "imu_sim" / (name + ".c")]
            if implementation:
                sources.append(SOURCE / implementation)
            subprocess.run(
                ["gcc", "-O2", "-Wall", "-I", SOURCE, *sources, "-lm", "-o", binary],
                check=True,
            )
            if name != "imu_sim":
                subprocess.run([binary], cwd=directory, check=True)
                continue
            replay = HERE / "logs" / "2026-10-31_A8_hump_pass.csv"
            trials = [["--fake"], *[["--fake1", str(h)] for h in (4, 5, 10, 13)],
                      ["--g", str(replay)]]
            for args in trials:
                result = subprocess.run([binary, *args], cwd=directory, check=True,
                                        capture_output=True, text=True)
                print(result.stdout, end="")
                if "LIFTED" in result.stdout:
                    raise RuntimeError(f"Unexpected lift event during {args}")


if __name__ == "__main__":
    main()

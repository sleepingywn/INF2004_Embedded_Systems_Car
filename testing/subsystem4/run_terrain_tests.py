"""Build and run the IMU unit tests with the current host's native GCC."""

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
        ):
            binary = Path(directory) / (name + (".exe" if os.name == "nt" else ""))
            sources = [HERE / "checks" / (name + ".c")]
            if implementation:
                sources.append(SOURCE / implementation)
            subprocess.run(
                ["gcc", "-O2", "-Wall", "-I", SOURCE, *sources, "-lm", "-o", binary],
                check=True,
            )
            subprocess.run([binary], cwd=directory, check=True)


if __name__ == "__main__":
    main()

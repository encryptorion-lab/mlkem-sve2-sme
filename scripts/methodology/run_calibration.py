#!/usr/bin/env python3
"""Run independent wall-clock timer-calibration processes."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from output_dir import ROOT, ensure_writable


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument(
        "--binary",
        type=Path,
        default=Path(__file__).with_name("build") / "calibrate_timers",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "results" / "methodology" / "calibration",
    )
    args = parser.parse_args()

    if not args.binary.is_file():
        raise SystemExit(f"missing calibration binary: {args.binary}")

    output = ensure_writable(args.output)
    raw = ensure_writable(output / "raw")

    for block in range(1, args.runs + 1):
        destination = raw / f"block-{block:02d}.csv"
        print(f"[calibrate {block:02d}/{args.runs}]", flush=True)
        with destination.open("w") as handle:
            completed = subprocess.run(
                [str(args.binary)],
                stdout=handle,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            if completed.returncode != 0:
                destination.unlink(missing_ok=True)
                raise SystemExit(
                    f"calibration block {block} failed "
                    f"(exit {completed.returncode}):\n"
                    f"{completed.stderr or '<no stderr>'}"
                )

    subprocess.run(
        [
            "python3",
            str(Path(__file__).with_name("aggregate_calibration.py")),
            "--input",
            str(raw),
            "--output",
            str(output),
        ],
        check=True,
    )


if __name__ == "__main__":
    main()

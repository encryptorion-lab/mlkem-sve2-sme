#!/usr/bin/env python3
"""Run randomized independent ablation measurements sequentially.

No two benchmark processes are ever active at the same time.
"""

from __future__ import annotations

import argparse
import csv
import random
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def _can_update(path: Path) -> bool:
    if not path.exists():
        return True
    try:
        with path.open("a"):
            return True
    except PermissionError:
        return False


def ensure_writable(path: Path) -> Path:
    """Use results-local when the requested output tree is not writable.

    Creating a probe file can succeed while overwriting an existing output
    file still fails, so both cases are checked.
    """
    path = path.resolve()
    fallback = (path.parent / "results-local").resolve()

    def use_fallback() -> Path:
        fallback.mkdir(parents=True, exist_ok=True)
        print(
            f"warning: {path} is not writable. "
            f"Writing to {fallback}.",
            flush=True,
        )
        return fallback

    try:
        path.mkdir(parents=True, exist_ok=True)
        probe = path / ".write_probe"
        probe.write_text("ok")
        probe.unlink()
    except PermissionError:
        return use_fallback()
    for item in path.rglob("*"):
        if item.is_file() and not _can_update(item):
            return use_fallback()
    return path


def command_output(command: list[str]) -> str:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        return f"$ {' '.join(command)}\n<unavailable: {completed.stderr.strip()}>\n"
    return f"$ {' '.join(command)}\n{completed.stdout.strip()}\n"


def capture_environment(output: Path) -> None:
    commands = [
        ["date", "-u"],
        ["uname", "-srm"],
        ["sw_vers"],
        ["sysctl", "-n", "machdep.cpu.brand_string"],
        ["sysctl", "-n", "hw.model"],
        ["sysctl", "-n", "hw.perflevel0.name"],
        ["sysctl", "-n", "hw.perflevel0.physicalcpu"],
        ["sysctl", "-n", "hw.cpufrequency"],
        ["sysctl", "-n", "hw.cpufrequency_max"],
        ["printenv", "BENCH_CPU_HZ"],
        ["clang", "--version"],
    ]
    text = "".join(command_output(command) + "\n" for command in commands)
    (output / "environment.txt").write_text(text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--paramset", type=int, default=768)
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--seed", type=int, default=20260921)
    parser.add_argument("--output", type=Path, default=Path("results"))
    args = parser.parse_args()

    build = args.build_dir.resolve()
    output = ensure_writable(args.output)
    raw = output / "raw"
    raw.mkdir(parents=True, exist_ok=True)
    capture_environment(output)
    rng = random.Random(args.seed)
    manifest_rows: list[dict[str, object]] = []
    timer_log = (output / "timer.log").open("w")

    for block in range(1, args.runs + 1):
        stages = list(range(1, 6))
        rng.shuffle(stages)
        for order, stage in enumerate(stages, 1):
            binary = build / f"s{stage}-{args.paramset}" / "bench_ablation"
            if not binary.is_file():
                timer_log.close()
                raise SystemExit(f"missing benchmark binary: {binary}")
            destination = raw / f"block-{block:02d}-s{stage}.csv"
            started = datetime.now(timezone.utc).isoformat()
            print(
                f"[block {block:02d}/{args.runs}  {order}/{len(stages)}] S{stage}",
                flush=True,
            )
            with destination.open("w") as handle:
                completed = subprocess.run(
                    [str(binary), "--speed"],
                    stdout=handle,
                    stderr=subprocess.PIPE,
                    text=True,
                    check=False,
                )
            if completed.stderr:
                timer_log.write(
                    f"{destination.name}\n{completed.stderr.rstrip()}\n\n"
                )
                timer_log.flush()
            if completed.returncode != 0:
                destination.unlink(missing_ok=True)
                timer_log.close()
                raise SystemExit(
                    f"S{stage} block {block} failed: {completed.stderr}"
                )
            manifest_rows.append(
                {
                    "block": block,
                    "order": order,
                    "stage": stage,
                    "paramset": args.paramset,
                    "started_utc": started,
                    "file": destination.name,
                }
            )

    timer_log.close()
    manifest = output / "manifest.csv"
    with manifest.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(manifest_rows[0]))
        writer.writeheader()
        writer.writerows(manifest_rows)

    subprocess.run(
        [
            "python3",
            str(Path(__file__).with_name("aggregate.py")),
            "--input",
            str(raw),
            "--output",
            str(output),
        ],
        check=True,
    )


if __name__ == "__main__":
    main()

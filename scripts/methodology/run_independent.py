#!/usr/bin/env python3
"""Independent sequential speed runs for the production backends."""

from __future__ import annotations

import argparse
import csv
import random
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from output_dir import ROOT, ensure_writable

BACKENDS = (
    ("mlkem", "ref", "kem/mlkem/build/ref-{p}/bench_mlkem"),
    ("mlkem", "neon", "kem/mlkem/build/aarch64-{p}/bench_mlkem"),
    ("mlkem", "sve", "kem/mlkem/build/aarch64_sve-{p}/bench_mlkem"),
    ("mlkem", "sme", "kem/mlkem/build/aarch64_sme-{p}/bench_mlkem"),
    ("kyber", "ref", "kem/kyber/ref/build/kyber_ref-{p}/bench_kyber"),
    ("kyber", "neon", "kem/kyber/neon/build/kyber_neon-{p}/bench_kyber_neon"),
    ("kyber", "sve", "kem/kyber/sve/build/kyber_sve-{p}/bench"),
    ("kyber", "sme", "kem/kyber/sme/build/kyber_sme-{p}/bench"),
)


def command_output(command: list[str]) -> str:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        return f"$ {' '.join(command)}\n<unavailable: {completed.stderr.strip()}>\n"
    return f"$ {' '.join(command)}\n{completed.stdout.strip()}\n"


def capture_environment(output: Path) -> None:
    commands = [
        ["date", "-u"],
        ["uname", "-a"],
        ["sw_vers"],
        ["sysctl", "-n", "machdep.cpu.brand_string"],
        ["sysctl", "-n", "hw.model"],
        ["sysctl", "-n", "hw.perflevel0.name"],
        ["sysctl", "-n", "hw.perflevel0.physicalcpu"],
        ["sysctl", "-n", "hw.cpufrequency"],
        ["sysctl", "-n", "hw.cpufrequency_max"],
        ["sysctl", "-n", "hw.tbfrequency"],
        ["printenv", "BENCH_CPU_HZ"],
        ["clang", "--version"],
    ]
    (output / "environment.txt").write_text(
        "".join(command_output(command) + "\n" for command in commands)
    )


def jobs(paramsets: list[int]) -> list[tuple[str, str, int, Path]]:
    out: list[tuple[str, str, int, Path]] = []
    for scheme, backend, template in BACKENDS:
        for param in paramsets:
            binary = ROOT / template.format(p=param)
            out.append((scheme, backend, param, binary))
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--seed", type=int, default=20260926)
    parser.add_argument(
        "--paramset",
        type=int,
        action="append",
        choices=(512, 768, 1024),
        help="Repeat to select several; default is all three.",
    )
    parser.add_argument(
        "--scheme",
        action="append",
        choices=("mlkem", "kyber"),
        help="Repeat to select several; default is both.",
    )
    parser.add_argument(
        "--backend",
        action="append",
        choices=("ref", "neon", "sve", "sme"),
        help="Repeat to select several; default is all four.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "results" / "methodology",
    )
    args = parser.parse_args()

    paramsets = args.paramset or [512, 768, 1024]
    work = jobs(paramsets)
    if args.scheme:
        work = [job for job in work if job[0] in args.scheme]
    if args.backend:
        work = [job for job in work if job[1] in args.backend]
    if not work:
        raise SystemExit("no jobs match the selected --scheme/--backend/--paramset")
    missing = [str(binary) for _, _, _, binary in work if not binary.is_file()]
    if missing:
        raise SystemExit(
            "missing binaries (build with NTESTS=10000 first):\n  "
            + "\n  ".join(missing)
        )

    output = ensure_writable(args.output)
    raw = ensure_writable(output / "raw")
    capture_environment(output)
    rng = random.Random(args.seed)
    manifest_path = output / "manifest.csv"
    manifest: list[dict[str, object]] = []
    if manifest_path.is_file():
        with manifest_path.open(newline="") as handle:
            manifest = list(csv.DictReader(handle))
    timer_log = (output / "timer.log").open("a")

    for block in range(1, args.runs + 1):
        order = work[:]
        rng.shuffle(order)
        for step, (scheme, backend, param, binary) in enumerate(order, 1):
            destination = raw / (
                f"block-{block:02d}-{scheme}-{backend}-{param}.csv"
            )
            started = datetime.now(timezone.utc).isoformat()
            print(
                f"[block {block:02d}/{args.runs}  {step}/{len(order)}] "
                f"{scheme} {backend} {param}",
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
                    f"{scheme} {backend} {param} block {block} failed:\n"
                    f"{completed.stderr}"
                )
            rec = {
                "block": str(block),
                "order": str(step),
                "scheme": scheme,
                "backend": backend,
                "paramset": str(param),
                "started_utc": started,
                "file": destination.name,
            }
            manifest = [
                row
                for row in manifest
                if not (
                    str(row.get("block")) == rec["block"]
                    and row.get("scheme") == rec["scheme"]
                    and row.get("backend") == rec["backend"]
                    and str(row.get("paramset")) == rec["paramset"]
                )
            ]
            manifest.append(rec)

    timer_log.close()
    fieldnames = [
        "block",
        "order",
        "scheme",
        "backend",
        "paramset",
        "started_utc",
        "file",
    ]
    with manifest_path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(manifest)

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

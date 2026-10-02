#!/usr/bin/env python3
"""Count selected instructions in each linked NTT function."""

from __future__ import annotations

import argparse
import csv
import re
import subprocess
from collections import Counter
from pathlib import Path

OPS = (
    "ld1h", "ld2h", "st1h", "za_move", "mul", "sqrdmulh", "mls",
    "add", "sub", "zip1", "zip2", "trn1", "trn2", "uzp1", "uzp2",
    "smstart", "smstop",
)


def count_function(binary: Path, symbol: str) -> Counter[str]:
    completed = subprocess.run(
        [
            "xcrun",
            "llvm-objdump",
            f"--disassemble-symbols=_{symbol}",
            str(binary),
        ],
        text=True,
        capture_output=True,
    )
    if completed.returncode != 0:
        raise SystemExit(
            f"disassembly failed for {binary} {symbol}:\n{completed.stderr}"
        )
    counts: Counter[str] = Counter()
    for line in completed.stdout.splitlines():
        match = re.match(
            r"^\s*[0-9a-f]+:\s+[0-9a-f]+\s+([A-Za-z0-9_.]+)\b", line
        )
        if match:
            mnemonic = match.group(1).lower()
            counts["_total"] += 1
            counts[mnemonic] += 1
            if mnemonic in {"mov", "mova"} and "za" in line.lower():
                counts["za_move"] += 1
    return counts


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--paramset", type=int, default=768)
    parser.add_argument(
        "--output", type=Path, default=Path("results/static_counts.csv")
    )
    args = parser.parse_args()

    rows: list[dict[str, object]] = []
    for stage in range(1, 6):
        binary = (
            args.build_dir.resolve()
            / f"s{stage}-{args.paramset}"
            / "bench_ablation"
        )
        if not binary.is_file():
            raise SystemExit(f"missing binary: {binary}")
        for symbol in ("forward_ntt_asm", "inverse_ntt_asm"):
            counts = count_function(binary, symbol)
            row: dict[str, object] = {
                "stage": f"S{stage}",
                "paramset": args.paramset,
                "function": symbol,
                "total": counts["_total"],
            }
            row.update({op: counts[op] for op in OPS})
            rows.append(row)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = ["stage", "paramset", "function", "total", *OPS]
    with args.output.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {args.output}")


if __name__ == "__main__":
    main()

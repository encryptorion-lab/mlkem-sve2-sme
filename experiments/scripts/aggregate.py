#!/usr/bin/env python3
"""Aggregate independent-process medians from module-style --speed CSVs.

Each process already reports the median of its 10,000 iterations. This
script takes the median and IQR across those process medians.
"""

from __future__ import annotations

import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path


def quartiles(values: list[float]) -> tuple[float, float]:
    if len(values) < 2:
        return values[0], values[0]
    cuts = statistics.quantiles(values, n=4, method="inclusive")
    return cuts[0], cuts[2]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    per_run: list[dict[str, object]] = []
    run_medians: dict[tuple[str, str, str], list[float]] = defaultdict(list)
    for path in sorted(args.input.glob("block-*-s*.csv")):
        block = int(path.stem.split("-")[1])
        stage = "S" + path.stem.split("-s")[-1]
        with path.open(newline="") as handle:
            reader = csv.DictReader(handle)
            if reader.fieldnames is None or "median" not in reader.fieldnames:
                raise SystemExit(
                    f"{path.name} is not a --speed CSV "
                    "(expected a median column)"
                )
            for row in reader:
                if row.get("operation") in (None, "correctness"):
                    continue
                key = (stage, row["paramset"], row["operation"])
                median = float(row["median"])
                per_run.append({
                    "block": block,
                    "stage": stage,
                    "paramset": row["paramset"],
                    "operation": row["operation"],
                    "median": median,
                    "p25": float(row["p25"]),
                    "p75": float(row["p75"]),
                    "iqr": float(row["iqr"]),
                    "n": int(float(row["n"])),
                    "source": path.name,
                })
                run_medians[key].append(median)

    if not per_run:
        raise SystemExit(f"no raw CSV files found under {args.input}")
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "per_run.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(per_run[0]))
        writer.writeheader()
        writer.writerows(per_run)

    summary: list[dict[str, object]] = []
    for (stage, paramset, operation), values in sorted(run_medians.items()):
        q1, q3 = quartiles(values)
        summary.append({
            "stage": stage, "paramset": paramset, "operation": operation,
            "median_of_run_medians": statistics.median(values),
            "q1_of_run_medians": q1, "q3_of_run_medians": q3,
            "iqr_of_run_medians": q3 - q1,
            "independent_runs": len(values),
        })
    with (args.output / "summary.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)


if __name__ == "__main__":
    main()

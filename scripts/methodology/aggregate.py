#!/usr/bin/env python3
"""Aggregate per-process --speed CSVs into median-of-medians + IQR."""

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

    for path in sorted(args.input.glob("block-*.csv")):
        parts = path.stem.split("-")
        # block-XX-scheme-backend-param
        block = int(parts[1])
        with path.open(newline="") as handle:
            for row in csv.DictReader(handle):
                if "median" not in row or not row["median"]:
                    continue
                key = (row["variant"], row["paramset"], row["operation"])
                median = float(row["median"])
                per_run.append(
                    {
                        "block": block,
                        "variant": row["variant"],
                        "paramset": row["paramset"],
                        "operation": row["operation"],
                        "median": median,
                        "p25": float(row.get("p25") or 0),
                        "p75": float(row.get("p75") or 0),
                        "iqr": float(row.get("iqr") or 0),
                        "stddev": float(row.get("stddev") or 0),
                        "n": int(float(row.get("n") or 0)),
                        "source": path.name,
                    }
                )
                run_medians[key].append(median)

    if not per_run:
        raise SystemExit(f"no --speed CSVs found under {args.input}")

    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "per_run.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(per_run[0]))
        writer.writeheader()
        writer.writerows(per_run)

    summary: list[dict[str, object]] = []
    for (variant, paramset, operation), values in sorted(run_medians.items()):
        q1, q3 = quartiles(values)
        summary.append(
            {
                "variant": variant,
                "paramset": paramset,
                "operation": operation,
                "median_of_run_medians": statistics.median(values),
                "q1_of_run_medians": q1,
                "q3_of_run_medians": q3,
                "iqr_of_run_medians": q3 - q1,
                "independent_runs": len(values),
            }
        )
    with (args.output / "summary.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)


if __name__ == "__main__":
    main()

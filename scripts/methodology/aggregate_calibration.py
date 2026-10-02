#!/usr/bin/env python3
"""Summarize independent wall-clock calibration processes."""

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
    by_kind_ns: dict[str, list[float]] = defaultdict(list)
    by_kind_cyc: dict[str, list[float]] = defaultdict(list)
    frequencies: list[float] = []
    timers: set[str] = set()
    sources: set[str] = set()

    for path in sorted(args.input.glob("block-*.csv")):
        block = int(path.stem.split("-")[1])
        grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
        with path.open(newline="") as handle:
            for row in csv.DictReader(handle):
                grouped[row["kind"]].append(row)
                frequencies.append(float(row["cpu_hz"]))
                timers.add(row["timer"])
                sources.add(row["hz_source"])
        for kind, rows in grouped.items():
            units = int(float(rows[0]["work_units"] or 1))
            if units <= 0:
                units = 1
            elapsed = [float(r["elapsed_ns"]) / units for r in rows]
            cycles = [float(r["est_cycles"]) / units for r in rows]
            rec = {
                "block": block,
                "kind": kind,
                "median_elapsed_ns": statistics.median(elapsed),
                "median_est_cycles": statistics.median(cycles),
                "work_units": units,
                "samples": len(rows),
                "cpu_hz": float(rows[0]["cpu_hz"]),
                "timer": rows[0]["timer"],
                "hz_source": rows[0]["hz_source"],
                "source": path.name,
            }
            per_run.append(rec)
            by_kind_ns[kind].append(float(rec["median_elapsed_ns"]))
            by_kind_cyc[kind].append(float(rec["median_est_cycles"]))

    if not per_run:
        raise SystemExit(f"no calibration CSVs found under {args.input}")

    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "per_run.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(per_run[0]))
        writer.writeheader()
        writer.writerows(per_run)

    summary: list[dict[str, object]] = []
    for kind in sorted(by_kind_ns):
        ns_values = by_kind_ns[kind]
        cyc_values = by_kind_cyc[kind]
        q1_ns, q3_ns = quartiles(ns_values)
        q1_c, q3_c = quartiles(cyc_values)
        summary.append(
            {
                "kind": kind,
                "median_elapsed_ns": statistics.median(ns_values),
                "q1_elapsed_ns": q1_ns,
                "q3_elapsed_ns": q3_ns,
                "iqr_elapsed_ns": q3_ns - q1_ns,
                "median_est_cycles": statistics.median(cyc_values),
                "q1_est_cycles": q1_c,
                "q3_est_cycles": q3_c,
                "iqr_est_cycles": q3_c - q1_c,
                "cpu_hz": statistics.median(frequencies),
                "timer": ",".join(sorted(timers)),
                "hz_source": ",".join(sorted(sources)),
                "independent_runs": len(ns_values),
            }
        )
    with (args.output / "summary.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)


if __name__ == "__main__":
    main()

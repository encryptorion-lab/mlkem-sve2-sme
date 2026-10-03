#!/usr/bin/env python3
"""Summarize independent Table 1 instruction microbenchmarks."""

from __future__ import annotations

import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

TABLE_ROWS = [
    ("add", "Add", "add"),
    ("sub", "Sub", "sub"),
    ("mul", "Low multiply", "mul"),
    ("sqrdmulh", "High-half multiply", "sqrdmulh"),
    ("mls", "Mul-sub", "mls"),
    ("smull", "Widening multiply", "smull/smullb,t"),
    ("smlsl", "Widening mul-sub", "smlsl/smlslb,t"),
    ("zip", "Permute", "zip1/zip2"),
    ("trn", "Permute", "trn1/trn2"),
    ("uzp", "Permute", "uzp1/uzp2"),
    ("ld1", "Load", "ld1/ld1h"),
    ("ld2", "2-way load", "ld2/ld2h"),
    ("st1", "Store", "st1/st1h"),
    ("mova_h", "ZA transfer", "mova h-store"),
    ("mova_v", "ZA transfer", "mova v-load"),
    ("mode_switch", "Mode switch", "smstart+smstop"),
]


def quartiles(values: list[float]) -> tuple[float, float]:
    if len(values) < 2:
        return values[0], values[0]
    cuts = statistics.quantiles(values, n=4, method="inclusive")
    return cuts[0], cuts[2]


def fmt(value: float | None, digits: int = 3) -> str:
    if value is None:
        return "--"
    return f"{value:.{digits}f}"


def ratio(sve: float | None, neon: float | None) -> str:
    if sve is None or neon is None or neon == 0.0:
        return "--"
    return f"{sve / neon:.2f}x"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    per_run: list[dict[str, object]] = []
    by_key: dict[tuple[str, str, str], list[float]] = defaultdict(list)
    by_key_coeff: dict[tuple[str, str, str], list[float]] = defaultdict(list)
    frequencies: list[float] = []
    timers: set[str] = set()
    sources: set[str] = set()

    for path in sorted(args.input.glob("block-*.csv")):
        block = int(path.stem.split("-")[1])
        grouped: dict[tuple[str, str, str], list[dict[str, str]]] = defaultdict(list)
        with path.open(newline="") as handle:
            for row in csv.DictReader(handle):
                key = (row["isa"], row["kind"], row["op"])
                grouped[key].append(row)
                frequencies.append(float(row["cpu_hz"]))
                timers.add(row["timer"])
                sources.add(row["hz_source"])
        for key, rows in grouped.items():
            insn = [float(r["cyc_per_insn"]) for r in rows]
            coeff = [float(r["cyc_per_coeff"]) for r in rows]
            rec = {
                "block": block,
                "isa": key[0],
                "kind": key[1],
                "op": key[2],
                "mnemonic": rows[0]["mnemonic"],
                "lanes": int(rows[0]["lanes"]),
                "work_units": int(rows[0]["work_units"]),
                "median_cyc_per_insn": statistics.median(insn),
                "median_cyc_per_coeff": statistics.median(coeff),
                "samples": len(rows),
                "cpu_hz": float(rows[0]["cpu_hz"]),
                "timer": rows[0]["timer"],
                "hz_source": rows[0]["hz_source"],
                "source": path.name,
            }
            per_run.append(rec)
            by_key[key].append(float(rec["median_cyc_per_insn"]))
            by_key_coeff[key].append(float(rec["median_cyc_per_coeff"]))

    if not per_run:
        raise SystemExit(f"no instruction CSVs found under {args.input}")

    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "per_run.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(per_run[0]))
        writer.writeheader()
        writer.writerows(per_run)

    summary: list[dict[str, object]] = []
    lookup: dict[tuple[str, str, str], dict[str, float]] = {}
    for key in sorted(by_key):
        insn_values = by_key[key]
        coeff_values = by_key_coeff[key]
        q1, q3 = quartiles(insn_values)
        q1c, q3c = quartiles(coeff_values)
        rec = {
            "isa": key[0],
            "kind": key[1],
            "op": key[2],
            "median_cyc_per_insn": statistics.median(insn_values),
            "q1_cyc_per_insn": q1,
            "q3_cyc_per_insn": q3,
            "iqr_cyc_per_insn": q3 - q1,
            "median_cyc_per_coeff": statistics.median(coeff_values),
            "q1_cyc_per_coeff": q1c,
            "q3_cyc_per_coeff": q3c,
            "iqr_cyc_per_coeff": q3c - q1c,
            "cpu_hz": statistics.median(frequencies),
            "timer": ",".join(sorted(timers)),
            "hz_source": ",".join(sorted(sources)),
            "independent_runs": len(insn_values),
        }
        summary.append(rec)
        lookup[key] = {
            "insn": float(rec["median_cyc_per_insn"]),
            "coeff": float(rec["median_cyc_per_coeff"]),
        }
    with (args.output / "summary.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)

    table: list[dict[str, object]] = []
    for op, klass, mnemonic in TABLE_ROWS:
        neon_first = lookup.get(("neon", "lat", op), {}).get("insn")
        sve_first = lookup.get(("sve", "lat", op), {}).get("insn")
        neon_tp = lookup.get(("neon", "tp", op), {}).get("insn")
        sve_tp = lookup.get(("sve", "tp", op), {}).get("insn")
        neon_coeff = lookup.get(("neon", "tp", op), {}).get("coeff")
        sve_coeff = lookup.get(("sve", "tp", op), {}).get("coeff")
        if op == "mode_switch":
            neon_coeff = None
            sve_coeff = None
        table.append(
            {
                "class": klass,
                "instruction": mnemonic,
                "neon_first_measurement_ce": fmt(neon_first),
                "sve_first_measurement_ce": fmt(sve_first),
                "neon_reciprocal_throughput_ce": fmt(neon_tp),
                "sve_reciprocal_throughput_ce": fmt(sve_tp),
                "neon_reciprocal_throughput_ce_per_coeff": fmt(neon_coeff, 4),
                "sve_reciprocal_throughput_ce_per_coeff": fmt(sve_coeff, 4),
                "sve_over_neon_reciprocal_throughput_per_coeff": ratio(
                    sve_coeff, neon_coeff
                ),
            }
        )
    with (args.output / "table.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(table[0]))
        writer.writeheader()
        writer.writerows(table)

    print("Table 1 (estimated CE = elapsed time x assumed P-core frequency)")
    print(
        f"{'class':<16} {'insn':<20} "
        f"{'N first':>8} {'S first':>8} {'N rt':>8} {'S rt':>8} "
        f"{'N/c':>8} {'S/c':>8} {'S/N':>8}"
    )
    for row in table:
        print(
            f"{row['class']:<16} {row['instruction']:<20} "
            f"{row['neon_first_measurement_ce']:>8} "
            f"{row['sve_first_measurement_ce']:>8} "
            f"{row['neon_reciprocal_throughput_ce']:>8} "
            f"{row['sve_reciprocal_throughput_ce']:>8} "
            f"{row['neon_reciprocal_throughput_ce_per_coeff']:>8} "
            f"{row['sve_reciprocal_throughput_ce_per_coeff']:>8} "
            f"{row['sve_over_neon_reciprocal_throughput_per_coeff']:>8}"
        )


if __name__ == "__main__":
    main()

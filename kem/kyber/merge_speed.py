#!/usr/bin/env python3
"""Merge per-backend Kyber speed CSVs into one ref|neon|sve|sme table.

Reads <backend>/results/speed/kyber_<backend>-<param>.csv and writes
results/kyber-speed.csv (paramset,operation,ref,neon,sve,sme).
Missing backends/params are tolerated (cells shown as '-').
"""
import csv
import os

HERE = os.path.dirname(os.path.abspath(__file__))  # kem/kyber/

BACKENDS = [
    ("ref", "ref", "kyber_ref"),
    ("neon", "neon", "kyber_neon"),
    ("sve", "sve", "kyber_sve"),
    ("sme", "sme", "kyber_sme"),
]
PARAMS = ["512", "768", "1024"]
OP_ORDER = [
    "KeyGen", "Encaps", "Decaps", "NTT", "INVNTT", "BaseMulAcc",
    "MatrixVectorMul", "InnerProdEnc", "InnerProdDec",
]


def load_medians(path):
    out = {}
    if not os.path.exists(path):
        return out
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            op = row.get("operation")
            med = row.get("median")
            if op and med:
                out[op] = med
    return out


def main():
    os.makedirs(os.path.join(HERE, "results"), exist_ok=True)
    combined = []
    for p in PARAMS:
        data = {}
        for tag, subdir, prefix in BACKENDS:
            path = os.path.join(HERE, subdir, "results", "speed",
                                f"{prefix}-{p}.csv")
            data[tag] = load_medians(path)

        ops = list(OP_ORDER)
        for tag, _, _ in BACKENDS:
            for op in data[tag]:
                if op not in ops:
                    ops.append(op)
        ops = [op for op in ops if any(op in data[t] for t, _, _ in BACKENDS)]
        for op in ops:
            cells = [data[tag].get(op, "-") for tag, _, _ in BACKENDS]
            combined.append([f"Kyber-{p}", op] + cells)

    out_path = os.path.join(HERE, "results", "kyber-speed.csv")
    with open(out_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["paramset", "operation", "ref", "neon", "sve", "sme"])
        w.writerows(combined)

    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()

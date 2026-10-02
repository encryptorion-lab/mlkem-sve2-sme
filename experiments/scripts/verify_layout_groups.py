#!/usr/bin/env python3
"""Compare intermediates only among stages sharing the same NTT layout."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


def dump(build: Path, stage: int, paramset: int) -> bytes:
    binary = build / f"s{stage}-{paramset}" / "bench_ablation"
    return subprocess.run(
        [str(binary), "--dump-ntt"], check=True, capture_output=True
    ).stdout


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--paramset", type=int, required=True)
    args = parser.parse_args()
    build = args.build_dir.resolve()

    for name, stages in (("Vec layout", (1, 2, 3)),
                         ("Mat layout", (4, 5))):
        reference = dump(build, stages[0], args.paramset)
        for stage in stages[1:]:
            if dump(build, stage, args.paramset) != reference:
                raise SystemExit(
                    f"{name}: S{stage} differs from S{stages[0]} "
                    f"for ML-KEM-{args.paramset}"
                )
        print(f"{name}: S{stages[0]}-S{stages[-1]} identical "
              f"for ML-KEM-{args.paramset}")


if __name__ == "__main__":
    main()

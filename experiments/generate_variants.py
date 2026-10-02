#!/usr/bin/env python3
"""Build the five-stage ablation from the production NTT.

Production sources are copied, not edited. Ablation stages remove or replace
one factor at a time:

  S1 base   Production VecNTT with a spill after every layer, so each layer
            touches both registers and memory.
  S2 pair   Frozen pair-major VecNTT (two_reg_ntt_4_layer on one register
            pair through layers 4-7 before the next pair). Coefficients stay
            in registers.
  S3 vec    Unmodified production VecNTT: layer-major, all four pairs of a
            layer occupy the issue window before the next layer.
  S4 matgrp Mat layout. A group's mova loads all finish before its zips,
            and zips and butterflies then run last-loaded pair first.
  S5 mat    Unmodified production MatNTT.

Twiddle reuse is not a separate timed kernel. S5 keeps the production Mat
table; its benefit relative to S3 is the smaller table, written to
generated/twiddle_bytes.txt.
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "experiments" / "generated"
SVE = ROOT / "kem" / "mlkem" / "variants" / "sve"
SME = ROOT / "kem" / "mlkem" / "variants" / "sme"
PAIR_MAJOR = ROOT / "experiments" / "baselines" / "pair_major_ntt.S"

SPILL = """    st1h z0.h, p0, [x0]
    st1h z1.h, p0, [x0, #1, mul vl]
    st1h z2.h, p0, [x0, #2, mul vl]
    st1h z3.h, p0, [x0, #3, mul vl]
    st1h z4.h, p0, [x0, #4, mul vl]
    st1h z5.h, p0, [x0, #5, mul vl]
    st1h z6.h, p0, [x0, #6, mul vl]
    st1h z7.h, p0, [x0, #7, mul vl]
    ld1h z0.h, p0/z, [x0]
    ld1h z1.h, p0/z, [x0, #1, mul vl]
    ld1h z2.h, p0/z, [x0, #2, mul vl]
    ld1h z3.h, p0/z, [x0, #3, mul vl]
    ld1h z4.h, p0/z, [x0, #4, mul vl]
    ld1h z5.h, p0/z, [x0, #5, mul vl]
    ld1h z6.h, p0/z, [x0, #6, mul vl]
    ld1h z7.h, p0/z, [x0, #7, mul vl]
"""


def insert_spills(ntt: str) -> str:
    """Keep production layer-major order, but spill after layers 1-6."""
    start = ntt.index("    // layer 1\n")
    end = ntt.index("    barrett_vec z0, z30, z26, z27, .h\n")
    head, body, tail = ntt[:start], ntt[start:end], ntt[end:]
    lines = body.splitlines()
    layer_at = [i for i, line in enumerate(lines) if line.startswith("    // layer ")]
    if len(layer_at) != 7:
        raise RuntimeError(f"expected 7 layers, found {len(layer_at)}")
    out: list[str] = []
    for k, idx in enumerate(layer_at):
        nxt = layer_at[k + 1] if k + 1 < len(layer_at) else len(lines)
        chunk = lines[idx:nxt]
        suffix: list[str] = []
        while chunk and (
            chunk[-1].strip() == ""
            or chunk[-1].startswith("    add x")
            or chunk[-1].startswith("    mov x")
        ):
            suffix.insert(0, chunk.pop())
        out.extend(chunk)
        if k < 6:
            out.append(SPILL.rstrip())
        out.extend(suffix)
    return head + "\n".join(out) + "\n" + tail


def _reg(operand: str) -> str:
    return operand.split(".")[0]


def _pair_mova_then_zip(mova_lines: list[str], zip_lines: list[str]) -> list[str]:
    """Load every pair, then zip in reverse: load a,b,c,d and zip d,c,b,a."""
    if len(mova_lines) != 8 or len(zip_lines) != 4:
        raise RuntimeError("expected 8 vertical mova and 4 zip1.h")
    for index, zip_line in enumerate(zip_lines):
        parts = zip_line.replace(",", " ").split()
        used = {_reg(parts[2]), _reg(parts[3])}
        pair = mova_lines[2 * index:2 * index + 2]
        produced = {_reg(line.split()[1]) for line in pair}
        if used != produced:
            raise RuntimeError(f"zip does not match mova pair: {zip_line}")
    return mova_lines + list(reversed(zip_lines))


def _regroup_vertical_prep(lines: list[str]) -> list[str]:
    stripped = [line.rstrip() for line in lines]
    index = 0
    found = 0
    while index < len(stripped):
        if not stripped[index].startswith("    mova z"):
            index += 1
            continue
        mova_at = []
        cursor = index
        while cursor < len(stripped) and stripped[cursor].startswith("    mova z"):
            mova_at.append(cursor)
            cursor += 1
        if len(mova_at) != 8:
            index = cursor
            continue
        zip_at = []
        look = cursor
        while look < len(stripped) and len(zip_at) < 4:
            if stripped[look].startswith("    zip1 ") and ".h" in stripped[look]:
                zip_at.append(look)
            elif stripped[look].strip() == "":
                pass
            else:
                break
            look += 1
        if len(zip_at) != 4:
            index = cursor
            continue
        mixed = _pair_mova_then_zip(
            [stripped[k] for k in mova_at],
            [stripped[k] for k in zip_at],
        )
        start, end = mova_at[0], zip_at[-1] + 1
        stripped[start:end] = mixed
        index = start + len(mixed)
        found += 1
    if found != 4:
        raise RuntimeError(f"expected 4 mova/zip groups, found {found}")
    return stripped


def make_mat_grouped() -> str:
    lines = _regroup_vertical_prep(
        (SME / "__asm_sme_ntt.S").read_text().splitlines()
    )
    groups = [
        (["z0", "z1", "z2", "z3"], ["z4", "z5", "z6", "z7"]),
        (["z4", "z5", "z6", "z7"], ["z8", "z9", "z10", "z11"]),
    ]
    for left, right in groups:
        butterflies = [
            f"    butterfly_vec {a}, {b}, z22, z23, z30, z31, z26, .h"
            for a, b in zip(left, right)
        ]
        zips = [f"    zip1 {a}.s, {a}.s, {b}.s" for a, b in zip(left, right)]
        start = None
        for i in range(len(lines) - 7):
            window = [lines[i + k].rstrip() for k in range(8)]
            if window == butterflies + zips:
                start = i
                break
        if start is None:
            raise RuntimeError(f"layer-4 group not found: {left[0]}")
        mixed = []
        for butterfly, zip_line in zip(reversed(butterflies), reversed(zips)):
            mixed.extend([butterfly, zip_line])
        lines[start:start + 8] = mixed
    lines = _reverse_butterfly_runs(lines)
    return "\n".join(lines) + "\n"


def _reverse_butterfly_runs(lines: list[str]) -> list[str]:
    """Within each straight butterfly run, compute the last-loaded pair first."""
    out: list[str] = []
    index = 0
    while index < len(lines):
        if lines[index].rstrip().startswith("    butterfly_vec "):
            run: list[str] = []
            while index < len(lines) and lines[index].rstrip().startswith(
                "    butterfly_vec "
            ):
                run.append(lines[index])
                index += 1
            if len(run) >= 2:
                run.reverse()
            out.extend(run)
        else:
            out.append(lines[index])
            index += 1
    return out


def write_stage(stage: int, ntt: str, inv: str, table: str) -> None:
    dst = OUT / f"s{stage}"
    dst.mkdir(parents=True, exist_ok=True)
    banner = f"/* GENERATED by experiments/generate_variants.py: S{stage}. */\n"
    (dst / "ntt.S").write_text(banner + ntt)
    (dst / "invntt.S").write_text(banner + inv)
    (dst / "ntt.c").write_text(banner + table)


def twiddle_report() -> None:
    sve = (SVE / "ntt.c").read_text()
    sme = (SME / "ntt.c").read_text()

    def count(text: str, name: str) -> int:
        token = f"{name}["
        start = text.index(token) + len(token)
        end = text.index("]", start)
        expr = text[start:end].replace(" ", "")
        if not expr or any(c not in "0123456789*" for c in expr):
            raise RuntimeError(f"unexpected bound for {name}: {expr}")
        value = 1
        for part in expr.split("*"):
            value *= int(part)
        return value

    rows = []
    for label, text in (("VecNTT", sve), ("MatNTT", sme)):
        forward = count(text, "zetas_asm")
        inverse = count(text, "invzetas_asm")
        rows.append(
            f"{label},forward_int16,{forward},forward_bytes,{forward * 2},"
            f"inverse_int16,{inverse},inverse_bytes,{inverse * 2}"
        )
    (OUT / "twiddle_bytes.txt").write_text("\n".join(rows) + "\n")


def main() -> None:
    sve_ntt = (SVE / "__asm_sve_ntt.S").read_text()
    sve_inv = (SVE / "__asm_sve_invntt.S").read_text()
    sve_table = (SVE / "ntt.c").read_text()
    sme_inv = (SME / "__asm_sme_invntt.S").read_text()
    sme_table = (SME / "ntt.c").read_text()
    pair_major = PAIR_MAJOR.read_text()
    write_stage(1, insert_spills(sve_ntt), sve_inv, sve_table)
    write_stage(2, pair_major, sve_inv, sve_table)
    write_stage(3, sve_ntt, sve_inv, sve_table)
    write_stage(4, make_mat_grouped(), sme_inv, sme_table)
    write_stage(5, (SME / "__asm_sme_ntt.S").read_text(), sme_inv, sme_table)
    stale = OUT / "s6"
    if stale.exists():
        for path in stale.iterdir():
            path.unlink()
        stale.rmdir()
    twiddle_report()
    print(f"generated S1-S5 in {OUT}")


if __name__ == "__main__":
    main()

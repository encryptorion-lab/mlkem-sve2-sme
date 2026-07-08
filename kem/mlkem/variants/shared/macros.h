/*******************************************************************************
 * Copyright (C) 2025 Hanyu Wei, Wenqian Li, Shiyu Shen, Hao Yang, Yunlei Zhao
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *******************************************************************************/

// Streaming-mode entry/exit for the SVE/SME kernels.
//
// On Apple silicon SVE is only reachable inside SME streaming mode, so every
// kernel brackets its body with smstart/smstop. The PSTATE.SM transition zeroes
// Z0-Z31 and P0-P15, which also destroys the caller's callee-saved FP registers
// (the low 64 bits of v8-v15). We therefore preserve d8-d15 on the stack BEFORE
// smstart and restore them AFTER smstop.
//
// Nothing else needs saving:
//   * the kernels use only caller-saved scratch GPRs (x9, x12-x16) and never
//     touch x19-x28 / x29 / x30, so no GPR spill is required;
//   * z8-z15 are used purely as scratch and are zeroed by smstart on entry, so
//     the old 512-byte Z spill buffer was redundant and is gone.
// p0 must be re-established after smstart (smstart zeroes the predicates).
.macro sme_init
    stp  d8,  d9, [sp, #-16]!
    stp d10, d11, [sp, #-16]!
    stp d12, d13, [sp, #-16]!
    stp d14, d15, [sp, #-16]!
    smstart
    ptrue p0.h
.endm

.macro sme_done
    smstop
    ldp d14, d15, [sp], #16
    ldp d12, d13, [sp], #16
    ldp d10, d11, [sp], #16
    ldp  d8,  d9, [sp], #16
.endm

.macro montgomery_mul a, b, tmp, tmp2, tmp3, qinv, q, wX, nX
    smullb \tmp\nX, \a\wX, \b\wX
    smullt \tmp2\nX, \a\wX, \b\wX
    trn1 \tmp3\wX, \tmp\wX, \tmp2\wX
    mul \tmp3\wX, \tmp3\wX, \qinv\wX
    smlslb \tmp\nX, \tmp3\wX, \q\wX
    smlslt \tmp2\nX, \tmp3\wX, \q\wX
    trn2 \tmp\wX, \tmp\wX, \tmp2\wX
.endm

.macro montgomery_accumulate2_mul a0, b0, a1, b1, out, lo, hi, lo2, hi2, tmp, qinv, q, wX, nX
    smullb \lo\nX, \a0\wX, \b0\wX
    smullt \hi\nX, \a0\wX, \b0\wX
    smullb \lo2\nX, \a1\wX, \b1\wX
    smullt \hi2\nX, \a1\wX, \b1\wX
    add \lo\nX, \lo\nX, \lo2\nX
    add \hi\nX, \hi\nX, \hi2\nX
    trn1 \tmp\wX, \lo\wX, \hi\wX
    mul \tmp\wX, \tmp\wX, \qinv\wX
    smlslb \lo\nX, \tmp\wX, \q\wX
    smlslt \hi\nX, \tmp\wX, \q\wX
    trn2 \out\wX, \lo\wX, \hi\wX
.endm

.macro barrett_vec a, tmp, q, v, wX
    sqdmulh \tmp\wX, \a\wX, \v\wX
    srshr \tmp\wX, p0/m, \tmp\wX, #11
    mls \a\wX, p0/m, \tmp\wX, \q\wX
.endm

.macro barrett_mul a, b, bR, q, tmp, tmp2, wX
    mul \tmp\wX, \a\wX, \b\wX
    sqrdmulh \tmp2\wX, \a\wX, \bR\wX
    mls \tmp\wX, p0/m, \tmp2\wX, \q\wX
.endm

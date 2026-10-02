#include "instr_params.h"

    .text
    .align 2

.macro EXPORT name
    .global _\name
    .global \name
_\name:
\name:
.endm

.macro SAVE_D8_D15
    stp  d8,  d9, [sp, #-16]!
    stp d10, d11, [sp, #-16]!
    stp d12, d13, [sp, #-16]!
    stp d14, d15, [sp, #-16]!
.endm

.macro RESTORE_D8_D15
    ldp d14, d15, [sp], #16
    ldp d12, d13, [sp], #16
    ldp d10, d11, [sp], #16
    ldp  d8,  d9, [sp], #16
.endm

.macro SVE_ENTER
    SAVE_D8_D15
    smstart
    ptrue p0.h
.endm

.macro SVE_LEAVE
    smstop
    RESTORE_D8_D15
    ret
.endm

.macro NEON_LAT_HH name, insn
EXPORT \name
    movi v0.8h, #1
    movi v1.8h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    \insn v0.8h, v0.8h, v1.8h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret
.endm

.macro NEON_TP_HH name, insn
EXPORT \name
    movi v16.8h, #1
    movi v17.8h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    \insn v0.8h, v16.8h, v17.8h
    \insn v1.8h, v16.8h, v17.8h
    \insn v2.8h, v16.8h, v17.8h
    \insn v3.8h, v16.8h, v17.8h
    \insn v4.8h, v16.8h, v17.8h
    \insn v5.8h, v16.8h, v17.8h
    \insn v6.8h, v16.8h, v17.8h
    \insn v7.8h, v16.8h, v17.8h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret
.endm

.macro SVE_LAT_HH name, insn
EXPORT \name
    SVE_ENTER
    mov  z0.h, #1
    mov  z1.h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    \insn z0.h, z0.h, z1.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE
.endm

.macro SVE_TP_HH name, insn
EXPORT \name
    SVE_ENTER
    mov  z16.h, #1
    mov  z17.h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    \insn z0.h, z16.h, z17.h
    \insn z1.h, z16.h, z17.h
    \insn z2.h, z16.h, z17.h
    \insn z3.h, z16.h, z17.h
    \insn z4.h, z16.h, z17.h
    \insn z5.h, z16.h, z17.h
    \insn z6.h, z16.h, z17.h
    \insn z7.h, z16.h, z17.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE
.endm

NEON_LAT_HH neon_lat_add, add
NEON_TP_HH  neon_tp_add,  add
NEON_LAT_HH neon_lat_sub, sub
NEON_TP_HH  neon_tp_sub,  sub
NEON_LAT_HH neon_lat_mul, mul
NEON_TP_HH  neon_tp_mul,  mul
NEON_LAT_HH neon_lat_sqrdmulh, sqrdmulh
NEON_TP_HH  neon_tp_sqrdmulh,  sqrdmulh
NEON_LAT_HH neon_lat_zip1, zip1
NEON_TP_HH  neon_tp_zip1,  zip1
NEON_LAT_HH neon_lat_trn1, trn1
NEON_TP_HH  neon_tp_trn1,  trn1
NEON_LAT_HH neon_lat_uzp1, uzp1
NEON_TP_HH  neon_tp_uzp1,  uzp1

SVE_LAT_HH sve_lat_add, add
SVE_TP_HH  sve_tp_add,  add
SVE_LAT_HH sve_lat_sub, sub
SVE_TP_HH  sve_tp_sub,  sub
SVE_LAT_HH sve_lat_mul, mul
SVE_TP_HH  sve_tp_mul,  mul
SVE_LAT_HH sve_lat_sqrdmulh, sqrdmulh
SVE_TP_HH  sve_tp_sqrdmulh,  sqrdmulh
SVE_LAT_HH sve_lat_zip1, zip1
SVE_TP_HH  sve_tp_zip1,  zip1
SVE_LAT_HH sve_lat_trn1, trn1
SVE_TP_HH  sve_tp_trn1,  trn1
SVE_LAT_HH sve_lat_uzp1, uzp1
SVE_TP_HH  sve_tp_uzp1,  uzp1

EXPORT neon_lat_mls
    movi v0.8h, #1
    movi v1.8h, #1
    movi v2.8h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    mls v0.8h, v1.8h, v2.8h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT neon_tp_mls
    movi v16.8h, #1
    movi v17.8h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    mls v0.8h, v16.8h, v17.8h
    mls v1.8h, v16.8h, v17.8h
    mls v2.8h, v16.8h, v17.8h
    mls v3.8h, v16.8h, v17.8h
    mls v4.8h, v16.8h, v17.8h
    mls v5.8h, v16.8h, v17.8h
    mls v6.8h, v16.8h, v17.8h
    mls v7.8h, v16.8h, v17.8h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT sve_lat_mls
    SVE_ENTER
    mov  z0.h, #1
    mov  z1.h, #1
    mov  z2.h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    mls z0.h, p0/m, z1.h, z2.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_mls
    SVE_ENTER
    mov  z16.h, #1
    mov  z17.h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    mls z0.h, p0/m, z16.h, z17.h
    mls z1.h, p0/m, z16.h, z17.h
    mls z2.h, p0/m, z16.h, z17.h
    mls z3.h, p0/m, z16.h, z17.h
    mls z4.h, p0/m, z16.h, z17.h
    mls z5.h, p0/m, z16.h, z17.h
    mls z6.h, p0/m, z16.h, z17.h
    mls z7.h, p0/m, z16.h, z17.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT neon_lat_smull
    movi v0.8h, #1
    movi v1.8h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    smull v0.4s, v0.4h, v1.4h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT neon_tp_smull
    movi v16.8h, #1
    movi v17.8h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    smull v0.4s, v16.4h, v17.4h
    smull v1.4s, v16.4h, v17.4h
    smull v2.4s, v16.4h, v17.4h
    smull v3.4s, v16.4h, v17.4h
    smull v4.4s, v16.4h, v17.4h
    smull v5.4s, v16.4h, v17.4h
    smull v6.4s, v16.4h, v17.4h
    smull v7.4s, v16.4h, v17.4h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT sve_lat_smullb
    SVE_ENTER
    mov  z0.h, #1
    mov  z1.h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    smullb z0.s, z0.h, z1.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_smullb
    SVE_ENTER
    mov  z16.h, #1
    mov  z17.h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    smullb z0.s, z16.h, z17.h
    smullb z1.s, z16.h, z17.h
    smullb z2.s, z16.h, z17.h
    smullb z3.s, z16.h, z17.h
    smullb z4.s, z16.h, z17.h
    smullb z5.s, z16.h, z17.h
    smullb z6.s, z16.h, z17.h
    smullb z7.s, z16.h, z17.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT neon_lat_smlsl
    movi v0.4s, #1
    movi v1.8h, #1
    movi v2.8h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    smlsl v0.4s, v1.4h, v2.4h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT neon_tp_smlsl
    movi v16.8h, #1
    movi v17.8h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    smlsl v0.4s, v16.4h, v17.4h
    smlsl v1.4s, v16.4h, v17.4h
    smlsl v2.4s, v16.4h, v17.4h
    smlsl v3.4s, v16.4h, v17.4h
    smlsl v4.4s, v16.4h, v17.4h
    smlsl v5.4s, v16.4h, v17.4h
    smlsl v6.4s, v16.4h, v17.4h
    smlsl v7.4s, v16.4h, v17.4h
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT sve_lat_smlslb
    SVE_ENTER
    mov  z0.s, #1
    mov  z1.h, #1
    mov  z2.h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    smlslb z0.s, z1.h, z2.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_smlslb
    SVE_ENTER
    mov  z16.h, #1
    mov  z17.h, #3
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    smlslb z0.s, z16.h, z17.h
    smlslb z1.s, z16.h, z17.h
    smlslb z2.s, z16.h, z17.h
    smlslb z3.s, z16.h, z17.h
    smlslb z4.s, z16.h, z17.h
    smlslb z5.s, z16.h, z17.h
    smlslb z6.s, z16.h, z17.h
    smlslb z7.s, z16.h, z17.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT neon_lat_ld1
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    ld1 {v0.8h}, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT neon_tp_ld1
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    ld1 {v0.8h}, [x1]
    ld1 {v1.8h}, [x1]
    ld1 {v2.8h}, [x1]
    ld1 {v3.8h}, [x1]
    ld1 {v4.8h}, [x1]
    ld1 {v5.8h}, [x1]
    ld1 {v6.8h}, [x1]
    ld1 {v7.8h}, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT sve_lat_ld1h
    SVE_ENTER
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    ld1h z0.h, p0/z, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_ld1h
    SVE_ENTER
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 8
    ld1h z0.h, p0/z, [x1]
    ld1h z1.h, p0/z, [x1]
    ld1h z2.h, p0/z, [x1]
    ld1h z3.h, p0/z, [x1]
    ld1h z4.h, p0/z, [x1]
    ld1h z5.h, p0/z, [x1]
    ld1h z6.h, p0/z, [x1]
    ld1h z7.h, p0/z, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT neon_lat_ld2
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    ld2 {v0.8h, v1.8h}, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT neon_tp_ld2
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 4
    ld2 {v0.8h, v1.8h}, [x1]
    ld2 {v2.8h, v3.8h}, [x1]
    ld2 {v4.8h, v5.8h}, [x1]
    ld2 {v6.8h, v7.8h}, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT sve_lat_ld2h
    SVE_ENTER
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    ld2h {z0.h, z1.h}, p0/z, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_ld2h
    SVE_ENTER
    mov  x1, x0
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 4
    ld2h {z0.h, z1.h}, p0/z, [x1]
    ld2h {z2.h, z3.h}, p0/z, [x1]
    ld2h {z4.h, z5.h}, p0/z, [x1]
    ld2h {z6.h, z7.h}, p0/z, [x1]
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT neon_lat_st1
    mov  x1, x0
    movi v0.8h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    st1 {v0.8h}, [x1], #16
    .endr
    mov  x1, x0
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT neon_tp_st1
    mov  x1, x0
    movi v0.8h, #1
    movi v1.8h, #1
    movi v2.8h, #1
    movi v3.8h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 4
    st1 {v0.8h}, [x1], #16
    st1 {v1.8h}, [x1], #16
    st1 {v2.8h}, [x1], #16
    st1 {v3.8h}, [x1], #16
    .endr
    mov  x1, x0
    subs x8, x8, #1
    b.ne 1b
    ret

EXPORT sve_lat_st1h
    SVE_ENTER
    mov  x1, x0
    mov  z0.h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    st1h z0.h, p0, [x1]
    inch x1
    .endr
    mov  x1, x0
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_st1h
    SVE_ENTER
    mov  x1, x0
    mov  z0.h, #1
    mov  z1.h, #1
    mov  z2.h, #1
    mov  z3.h, #1
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 4
    st1h z0.h, p0, [x1]
    inch x1
    st1h z1.h, p0, [x1]
    inch x1
    st1h z2.h, p0, [x1]
    inch x1
    st1h z3.h, p0, [x1]
    inch x1
    .endr
    mov  x1, x0
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_lat_mova_h
    SVE_ENTER
    mov  z0.h, #1
    mov  w12, wzr
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    mova za0h.h[w12, 0], p0/m, z0.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_mova_h
    SVE_ENTER
    mov  z0.h, #1
    mov  z1.h, #1
    mov  z2.h, #1
    mov  z3.h, #1
    mov  w12, wzr
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 4
    mova za0h.h[w12, 0], p0/m, z0.h
    mova za0h.h[w12, 1], p0/m, z1.h
    mova za0h.h[w12, 2], p0/m, z2.h
    mova za0h.h[w12, 3], p0/m, z3.h
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_lat_mova_v
    SVE_ENTER
    mov  w12, wzr
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL
    mova z0.h, p0/m, za0v.h[w12, 0]
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_tp_mova_v
    SVE_ENTER
    mov  w12, wzr
    mov  x8, #INSTR_REPS
1:
    .rept INSTR_UNROLL / 4
    mova z0.h, p0/m, za0v.h[w12, 0]
    mova z1.h, p0/m, za0v.h[w12, 1]
    mova z2.h, p0/m, za0v.h[w12, 2]
    mova z3.h, p0/m, za0v.h[w12, 3]
    .endr
    subs x8, x8, #1
    b.ne 1b
    SVE_LEAVE

EXPORT sve_mode_switch
    SAVE_D8_D15
    mov  x8, #INSTR_REPS
1:
    smstart
    smstop
    subs x8, x8, #1
    b.ne 1b
    RESTORE_D8_D15
    ret

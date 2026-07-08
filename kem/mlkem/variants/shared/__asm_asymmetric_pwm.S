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

#include "macros.h"
#include "params.h"

.align 4
.global asymmetric_preprocess_asm
.global _asymmetric_preprocess_asm
asymmetric_preprocess_asm:
_asymmetric_preprocess_asm:

    sme_init

    adrp x14, _const_q@PAGE
    add x14, x14, :lo12:_const_q@PAGEOFF
    ld1h z26.h, p0/z, [x14]

    ld2h {z0.h, z1.h}, p0/z, [x1]
    ld2h {z2.h, z3.h}, p0/z, [x1, #2, mul vl]
    ld2h {z4.h, z5.h}, p0/z, [x1, #4, mul vl]
    ld2h {z6.h, z7.h}, p0/z, [x1, #6, mul vl]

    add x3, x2, #128*2
    ld1h z16.h, p0/z, [x2]
    ld1h z17.h, p0/z, [x3]
    ld1h z18.h, p0/z, [x2, #1, mul vl]
    ld1h z19.h, p0/z, [x3, #1, mul vl]
    ld1h z20.h, p0/z, [x2, #2, mul vl]
    ld1h z21.h, p0/z, [x3, #2, mul vl]
    ld1h z22.h, p0/z, [x2, #3, mul vl]
    ld1h z23.h, p0/z, [x3, #3, mul vl]
    barrett_mul z1, z16, z17, z26, z0, z31, .h
    barrett_mul z3, z18, z19, z26, z1, z31, .h
    barrett_mul z5, z20, z21, z26, z2, z31, .h
    barrett_mul z7, z22, z23, z26, z3, z31, .h

    st1h z0.h, p0, [x0]
    st1h z1.h, p0, [x0, #1, mul vl]
    st1h z2.h, p0, [x0, #2, mul vl]
    st1h z3.h, p0, [x0, #3, mul vl]

    sme_done
    ret


// asymmetric_preprocess_polyvec_asm(int16_t *bprime, const int16_t *b,
//                                   const int16_t *zeta)
//
// Same computation as calling asymmetric_preprocess_asm() MLKEM_K times, but
// wrapped in a SINGLE streaming-mode region (one sme_init/sme_done) instead of
// MLKEM_K of them. The b-prime multiplier table (zeta) is identical for every
// polynomial, so it is loaded once and hoisted out of the loop.
//   x0 = bprime out  (MLKEM_K * 128 int16, contiguous)
//   x1 = b           (MLKEM_K * 256 int16, contiguous)
//   x2 = zeta table  (pre_asymmetric_table_sve, 128*2 int16)
.align 4
.global asymmetric_preprocess_polyvec_asm
.global _asymmetric_preprocess_polyvec_asm
asymmetric_preprocess_polyvec_asm:
_asymmetric_preprocess_polyvec_asm:

    sme_init

    adrp x14, _const_q@PAGE
    add x14, x14, :lo12:_const_q@PAGEOFF
    ld1h z26.h, p0/z, [x14]

    // zeta table is constant across all polynomials -> load once
    add x3, x2, #128*2
    ld1h z16.h, p0/z, [x2]
    ld1h z17.h, p0/z, [x3]
    ld1h z18.h, p0/z, [x2, #1, mul vl]
    ld1h z19.h, p0/z, [x3, #1, mul vl]
    ld1h z20.h, p0/z, [x2, #2, mul vl]
    ld1h z21.h, p0/z, [x3, #2, mul vl]
    ld1h z22.h, p0/z, [x2, #3, mul vl]
    ld1h z23.h, p0/z, [x3, #3, mul vl]

    mov x4, #MLKEM_K
_preprocess_polyvec_loop:
    ld2h {z0.h, z1.h}, p0/z, [x1]
    ld2h {z2.h, z3.h}, p0/z, [x1, #2, mul vl]
    ld2h {z4.h, z5.h}, p0/z, [x1, #4, mul vl]
    ld2h {z6.h, z7.h}, p0/z, [x1, #6, mul vl]

    barrett_mul z1, z16, z17, z26, z0, z31, .h
    barrett_mul z3, z18, z19, z26, z1, z31, .h
    barrett_mul z5, z20, z21, z26, z2, z31, .h
    barrett_mul z7, z22, z23, z26, z3, z31, .h

    st1h z0.h, p0, [x0]
    st1h z1.h, p0, [x0, #1, mul vl]
    st1h z2.h, p0, [x0, #2, mul vl]
    st1h z3.h, p0, [x0, #3, mul vl]

    add x1, x1, #512        // advance b by one polynomial (256 int16)
    add x0, x0, #256        // advance bprime by 128 int16
    sub x4, x4, #1
    cbnz x4, _preprocess_polyvec_loop

    sme_done
    ret


.align 4
.global asymmetric_pwm_montgomery_acc
.global _asymmetric_pwm_montgomery_acc
asymmetric_pwm_montgomery_acc:
_asymmetric_pwm_montgomery_acc:
    sme_init

    adrp x14, _constants_q_qinv@PAGE
    add x14, x14, :lo12:_constants_q_qinv@PAGEOFF
    ld2h {z26.h, z27.h}, p0/z, [x14]
    adrp x15, _constants_q_v@PAGE
    add x15, x15, :lo12:_constants_q_v@PAGEOFF
    ld2h {z28.h, z29.h}, p0/z, [x15]

    ld2h {z10.h, z11.h}, p0/z, [x1]             // a0, a1
    ld2h {z12.h, z13.h}, p0/z, [x1, #2, mul vl]

    ld2h {z14.h, z15.h}, p0/z, [x2]             // b0, b1
    ld2h {z16.h, z17.h}, p0/z, [x2, #2, mul vl]

    ld1h z8.h, p0/z, [x3]             // bprime
    ld1h z9.h, p0/z, [x3, #1, mul vl]

    montgomery_accumulate2_mul z12, z8, z10, z14, z18, z18, z19, z20, z21, z30, z27, z26, .h, .s   // a1bprime + a0b0
    montgomery_accumulate2_mul z13, z9, z11, z15, z19, z20, z21, z22, z23, z30, z27, z26, .h, .s
    montgomery_accumulate2_mul z12, z14, z10, z16, z20, z20, z21, z22, z23, z30, z27, z26, .h, .s  // a1b0 + a0b1
    montgomery_accumulate2_mul z13, z15, z11, z17, z21, z22, z23, z24, z25, z30, z27, z26, .h, .s

    zip1 z0.h, z18.h, z20.h
    zip2 z1.h, z18.h, z20.h
    zip1 z2.h, z19.h, z21.h
    zip2 z3.h, z19.h, z21.h
    st1h z0.h, p0, [x0]
    st1h z1.h, p0, [x0, #1, mul vl]
    st1h z2.h, p0, [x0, #2, mul vl]
    st1h z3.h, p0, [x0, #3, mul vl]

    ld2h {z10.h, z11.h}, p0/z, [x1, #4, mul vl]             // a0, a1
    ld2h {z12.h, z13.h}, p0/z, [x1, #6, mul vl]

    ld2h {z14.h, z15.h}, p0/z, [x2, #4, mul vl]             // b0, b1
    ld2h {z16.h, z17.h}, p0/z, [x2, #6, mul vl]

    ld1h z8.h, p0/z, [x3, #2, mul vl] // bprime
    ld1h z9.h, p0/z, [x3, #3, mul vl]                                  
                                                                                   
    montgomery_accumulate2_mul z12, z8, z10, z14, z18, z18, z19, z20, z21, z30, z27, z26, .h, .s   // a1bprime + a0b0
    montgomery_accumulate2_mul z13, z9, z11, z15, z19, z20, z21, z22, z23, z30, z27, z26, .h, .s
    montgomery_accumulate2_mul z12, z14, z10, z16, z20, z20, z21, z22, z23, z30, z27, z26, .h, .s  // a1b0 + a0b1
    montgomery_accumulate2_mul z13, z15, z11, z17, z21, z22, z23, z24, z25, z30, z27, z26, .h, .s

    zip1 z0.h, z18.h, z20.h   
    zip2 z1.h, z18.h, z20.h   
    zip1 z2.h, z19.h, z21.h   
    zip2 z3.h, z19.h, z21.h   
    st1h z0.h, p0, [x0, #4, mul vl]
    st1h z1.h, p0, [x0, #5, mul vl]
    st1h z2.h, p0, [x0, #6, mul vl]
    st1h z3.h, p0, [x0, #7, mul vl]

    sme_done
    ret


.align 4
.global asymmetric_pwm_montgomery_acc_polyvec
.global _asymmetric_pwm_montgomery_acc_polyvec
asymmetric_pwm_montgomery_acc_polyvec:
_asymmetric_pwm_montgomery_acc_polyvec:
    sme_init

    adrp x14, _constants_q_qinv@PAGE
    add x14, x14, :lo12:_constants_q_qinv@PAGEOFF
    ld2h {z26.h,z27.h}, p0/z, [x14]
    adrp x15, _constants_q_v@PAGE
    add x15, x15, :lo12:_constants_q_v@PAGEOFF
    ld2h {z28.h, z29.h}, p0/z, [x15]

    add  x5,  x1, #512*1 // vec[1]
    add  x6,  x2, #512*1
    add  x7,  x3, #256*1
#if MLKEM_K > 2
    add  x8,  x1, #512*2 // vec[2]
    add  x9,  x2, #512*2
    add  x10, x3, #256*2
#endif
#if MLKEM_K > 3
    add x11,  x1, #512*3 // vec[3]
    add x12,  x2, #512*3
    add x13,  x3, #256*3
#endif

    ld2h {z0.h, z1.h}, p0/z, [x1]     // vec[0].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x2]     // vec[0].b[0..63]
    ld1h z4.h, p0/z, [x3]             // vec[0].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z16, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    montgomery_accumulate2_mul z1, z2, z0, z3, z17, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1

    ld2h {z0.h, z1.h}, p0/z, [x5]     // vec[1].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x6]     // vec[1].b[0..63]
    ld1h z4.h, p0/z, [x7]             // vec[1].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h

#if MLKEM_K > 2
    ld2h {z0.h, z1.h}, p0/z, [x8]     // vec[2].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x9]     // vec[2].b[0..63]
    ld1h z4.h, p0/z, [x10]             // vec[2].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif

#if MLKEM_K > 3
    ld2h {z0.h, z1.h}, p0/z, [x11]     // vec[3].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x12]     // vec[3].b[0..63]
    ld1h z4.h, p0/z, [x13]             // vec[3].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif
    barrett_vec z16, z30, z26, z29, .h
    barrett_vec z17, z30, z26, z29, .h
    zip1 z0.h, z16.h, z17.h
    zip2 z1.h, z16.h, z17.h
    st1h z0.h, p0, [x0]
    st1h z1.h, p0, [x0, #1, mul vl]


    mov x4, #64
    mov x14, #32
    mov x15, #96
    mov x16, #3
    _pointwise_acc_montgomery_polyvec_loop:
    ld2h {z0.h, z1.h}, p0/z, [x1, x4, lsl#1]     // vec[0]
    ld2h {z2.h, z3.h}, p0/z, [x2, x4, lsl#1]     // vec[0]
    ld1h z4.h, p0/z, [x3, x14, lsl#1]             // vec[0].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z16, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    montgomery_accumulate2_mul z1, z2, z0, z3, z17, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1

    ld2h {z0.h, z1.h}, p0/z, [x5, x4, lsl#1]     // vec[1]
    ld2h {z2.h, z3.h}, p0/z, [x6, x4, lsl#1]     // vec[1]
    ld1h z4.h, p0/z, [x7, x14, lsl#1]             // vec[1].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h

#if MLKEM_K > 2
    ld2h {z0.h, z1.h}, p0/z, [x8, x4, lsl#1]     // vec[2].a
    ld2h {z2.h, z3.h}, p0/z, [x9, x4, lsl#1]     // vec[2].b
    ld1h z4.h, p0/z, [x10, x14, lsl#1]            // vec[2].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif

#if MLKEM_K > 3
    ld2h {z0.h, z1.h}, p0/z, [x11, x4, lsl#1]     // vec[3].a
    ld2h {z2.h, z3.h}, p0/z, [x12, x4, lsl#1]     // vec[3].b
    ld1h z4.h, p0/z, [x13, x14, lsl#1]             // vec[3].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif
    barrett_vec z16, z30, z26, z29, .h
    barrett_vec z17, z30, z26, z29, .h
    zip1 z0.h, z16.h, z17.h
    zip2 z1.h, z16.h, z17.h
    st1h z0.h, p0, [x0, x4, lsl#1]
    st1h z1.h, p0, [x0, x15, lsl#1]

    add x4, x4, #64
    add x14, x14, #32
    add x15, x15, #64
    sub x16, x16, #1
    cbnz x16, _pointwise_acc_montgomery_polyvec_loop

    sme_done
    ret



.align 4
.global asymmetric_pwm_montgomery_acc_polyvec_tomont
.global _asymmetric_pwm_montgomery_acc_polyvec_tomont
asymmetric_pwm_montgomery_acc_polyvec_tomont:
_asymmetric_pwm_montgomery_acc_polyvec_tomont:
    sme_init

    adrp x14, _constants_q_qinv@PAGE
    add x14, x14, :lo12:_constants_q_qinv@PAGEOFF
    ld2h {z26.h,z27.h}, p0/z, [x14]
    adrp x15, _constants_mont2_v@PAGE
    add x15, x15, :lo12:_constants_mont2_v@PAGEOFF
    ld2h {z28.h, z29.h}, p0/z, [x15]

    add  x5,  x1, #512*1 // vec[1]
    add  x6,  x2, #512*1
    add  x7,  x3, #256*1
#if MLKEM_K > 2
    add  x8,  x1, #512*2 // vec[2]
    add  x9,  x2, #512*2
    add  x10, x3, #256*2
#endif
#if MLKEM_K > 3
    add x11,  x1, #512*3 // vec[3]
    add x12,  x2, #512*3
    add x13,  x3, #256*3
#endif

    ld2h {z0.h, z1.h}, p0/z, [x1]     // vec[0].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x2]     // vec[0].b[0..63]
    ld1h z4.h, p0/z, [x3]             // vec[0].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z16, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    montgomery_accumulate2_mul z1, z2, z0, z3, z17, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1

    ld2h {z0.h, z1.h}, p0/z, [x5]     // vec[1].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x6]     // vec[1].b[0..63]
    ld1h z4.h, p0/z, [x7]             // vec[1].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h

#if MLKEM_K > 2
    ld2h {z0.h, z1.h}, p0/z, [x8]     // vec[2].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x9]     // vec[2].b[0..63]
    ld1h z4.h, p0/z, [x10]             // vec[2].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif

#if MLKEM_K > 3
    ld2h {z0.h, z1.h}, p0/z, [x11]     // vec[3].a[0..63]
    ld2h {z2.h, z3.h}, p0/z, [x12]     // vec[3].b[0..63]
    ld1h z4.h, p0/z, [x13]             // vec[3].bprime[0..31]
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif
    montgomery_mul z16, z28, z18, z30, z31, z27, z26, .h, .s
    montgomery_mul z17, z28, z19, z30, z31, z27, z26, .h, .s
    zip1 z0.h, z18.h, z19.h
    zip2 z1.h, z18.h, z19.h
    st1h z0.h, p0, [x0]
    st1h z1.h, p0, [x0, #1, mul vl]


    mov x4, #64
    mov x14, #32
    mov x15, #96
    mov x16, #3
    _pointwise_acc_montgomery_polyvec_tomont_loop:
    ld2h {z0.h, z1.h}, p0/z, [x1, x4, lsl#1]     // vec[0]
    ld2h {z2.h, z3.h}, p0/z, [x2, x4, lsl#1]     // vec[0]
    ld1h z4.h, p0/z, [x3, x14, lsl#1]             // vec[0].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z16, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    montgomery_accumulate2_mul z1, z2, z0, z3, z17, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1

    ld2h {z0.h, z1.h}, p0/z, [x5, x4, lsl#1]     // vec[1]
    ld2h {z2.h, z3.h}, p0/z, [x6, x4, lsl#1]     // vec[1]
    ld1h z4.h, p0/z, [x7, x14, lsl#1]             // vec[1].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h

#if MLKEM_K > 2
    ld2h {z0.h, z1.h}, p0/z, [x8, x4, lsl#1]     // vec[2].a
    ld2h {z2.h, z3.h}, p0/z, [x9, x4, lsl#1]     // vec[2].b
    ld1h z4.h, p0/z, [x10, x14, lsl#1]            // vec[2].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif

#if MLKEM_K > 3
    ld2h {z0.h, z1.h}, p0/z, [x11, x4, lsl#1]     // vec[3].a
    ld2h {z2.h, z3.h}, p0/z, [x12, x4, lsl#1]     // vec[3].b
    ld1h z4.h, p0/z, [x13, x14, lsl#1]             // vec[3].bprime
    montgomery_accumulate2_mul z1, z4, z0, z2, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1bprime + a0b0
    add z16.h, z16.h, z5.h
    montgomery_accumulate2_mul z1, z2, z0, z3, z5, z5, z6, z7, z18, z30, z27, z26, .h, .s   // a1b0 + a0b1
    add z17.h, z17.h, z5.h
#endif
    montgomery_mul z16, z28, z18, z30, z31, z27, z26, .h, .s
    montgomery_mul z17, z28, z19, z30, z31, z27, z26, .h, .s
    zip1 z0.h, z18.h, z19.h
    zip2 z1.h, z18.h, z19.h
    st1h z0.h, p0, [x0, x4, lsl#1]
    st1h z1.h, p0, [x0, x15, lsl#1]

    add x4, x4, #64
    add x14, x14, #32
    add x15, x15, #64
    sub x16, x16, #1
    cbnz x16, _pointwise_acc_montgomery_polyvec_tomont_loop

    sme_done
    ret

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

#ifndef KYBER_AARCH64_NTT_H
#define KYBER_AARCH64_NTT_H

#include <stdint.h>

/* Constant/twiddle tables defined in ntt.c and consumed by the assembly. */
extern const int16_t constants_q_v[64];
extern const int16_t const_q[32];
extern const int16_t constants_q_qinv[64];
extern const int16_t constants_mont2_v[64];

/* NTT / INVNTT / base-multiplication assembly entry points. */
extern void forward_ntt_asm(int16_t *src, const int16_t *zeta);
extern void inverse_ntt_asm(int16_t *src, const int16_t *zeta);
extern const int16_t zetas_asm[464*2];
extern const int16_t invzetas_asm[512*2];

extern const int16_t pre_asymmetric_table_sve[128*2];
extern void asymmetric_preprocess_asm(int16_t *bprime, const int16_t *b, const int16_t *zeta);
extern void asymmetric_pwm_montgomery_acc(int16_t *c, const int16_t *a, const int16_t *b, const int16_t *bprime);
extern void asymmetric_pwm_montgomery_acc_polyvec(int16_t *, const int16_t *, const int16_t *, const int16_t *);
extern void asymmetric_pwm_montgomery_acc_polyvec_tomont(int16_t *, const int16_t *, const int16_t *, const int16_t *);

#endif

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

.macro butterfly_vec a, b, zeta, zetaR, tmp, tmp2, q, hX
    barrett_mul \b, \zeta, \zetaR, \q, \tmp, \tmp2, \hX
    sub \b\hX, \a\hX, \tmp\hX
    add \a\hX, \a\hX, \tmp\hX
.endm

.macro two_reg_ntt_4_layer a0, a1, tmpa0, tmpa1, tmp, tmp2, \
                            zeta1, zeta2, zeta3, zeta4, zeta1R, zeta2R, zeta3R, zeta4R, q, hX, wX, nX
    zip1  \tmpa0\nX, \a0\nX, \a1\nX
    zip2  \tmpa1\nX, \a0\nX, \a1\nX
    // layer 4
    // 0, 1, 2, 3, 32, 33, 34, 35, 4, 5, 6, 7, 36, 37, 38, 39, 8, 9, 10, 11, 40, 41, 42, 43, 12, 13, 14, 15, 44, 45, 46, 47,
    // 16, 17, 18, 19, 48, 49, 50, 51, 20, 21, 22, 23, 52, 53, 54, 55, 24, 25, 26, 27, 56, 57, 58, 59, 28, 29, 30, 31, 60, 61, 62, 63,
    butterfly_vec \tmpa0, \tmpa1, \zeta1, \zeta1R, \tmp, \tmp2, \q, \hX

    zip1 \a0\nX, \tmpa0\nX, \tmpa1\nX
    zip2 \a1\nX, \tmpa0\nX, \tmpa1\nX
    // layer 5
    // 0, 1, 2, 3, 16, 17, 18, 19, 32, 33, 34, 35, 48, 49, 50, 51, 4, 5, 6, 7, 20, 21, 22, 23, 36, 37, 38, 39, 52, 53, 54, 55,
    // 8, 9, 10, 11, 24, 25, 26, 27, 40, 41, 42, 43, 56, 57, 58, 59, 12, 13, 14, 15, 28, 29, 30, 31, 44, 45, 46, 47, 60, 61, 62, 63,
    butterfly_vec \a0, \a1, \zeta2, \zeta2R, \tmp, \tmp2, \q, \hX

    zip1 \tmpa0\nX, \a0\nX, \a1\nX
    zip2 \tmpa1\nX, \a0\nX, \a1\nX
    // layer 6
    // 0, 1, 2, 3, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27, 32, 33, 34, 35, 40, 41, 42, 43, 48, 49, 50, 51, 56, 57, 58, 59,
    // 4, 5, 6, 7, 12, 13, 14, 15, 20, 21, 22, 23, 28, 29, 30, 31, 36, 37, 38, 39, 44, 45, 46, 47, 52, 53, 54, 55, 60, 61, 62, 63,
    butterfly_vec \tmpa0, \tmpa1, \zeta3, \zeta3R, \tmp, \tmp2, \q, \hX

    trn1 \a0\wX, \tmpa0\wX, \tmpa1\wX
    trn2 \tmpa1\wX, \tmpa0\wX, \tmpa1\wX
    // layer 7
    // 0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 25, 28, 29, 32, 33, 36, 37, 40, 41, 44, 45, 48, 49, 52, 53, 56, 57, 60, 61,
    // 2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22, 23, 26, 27, 30, 31, 34, 35, 38, 39, 42, 43, 46, 47, 50, 51, 54, 55, 58, 59, 62, 63,
    butterfly_vec \a0, \tmpa1, \zeta4, \zeta4R, \tmp, \tmp2, \q, \hX

    zip2 \a1\wX, \a0\wX, \tmpa1\wX
    zip1 \a0\wX, \a0\wX, \tmpa1\wX
.endm


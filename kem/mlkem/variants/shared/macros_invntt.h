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

.macro invbutterfly_vec a, b, zeta, zetaR, tmp, tmp2, q, hX
    sub \tmp\hX, \b\hX, \a\hX
    add \a\hX, \a\hX, \b\hX
    barrett_mul \tmp, \zeta, \zetaR, \q, \b, \tmp2, \hX
.endm


.macro two_reg_invntt_4_layer a0, a1, tmpa0, tmpa1, tmp, tmp2, \
                            zeta1, zeta2, zeta3, zeta4, zeta1R, zeta2R, zeta3R, zeta4R, q, hX, wX, nX
    trn1 \tmpa0\wX, \a0\wX, \a1\wX
    trn2 \tmpa1\wX, \a0\wX, \a1\wX
    // layer 1
    //  0, 1, 32, 33, 4, 5, 36, 37, 8, 9, 40, 41, 12, 13, 44, 45, 16, 17, 48, 49, 20, 21, 52, 53, 24, 25, 56, 57, 28, 29, 60, 61,
    //  2, 3, 34, 35, 6, 7, 38, 39, 10, 11, 42, 43, 14, 15, 46, 47, 18, 19, 50, 51, 22, 23, 54, 55, 26, 27, 58, 59, 30, 31, 62, 63,
    invbutterfly_vec \tmpa0, \tmpa1, \zeta1, \zeta1R, \tmp, \tmp2, \q, \hX

    trn1 \a0\nX, \tmpa0\nX, \tmpa1\nX
    trn2 \a1\nX, \tmpa0\nX, \tmpa1\nX
    // layer 2
    //  0, 1, 32, 33, 2, 3, 34, 35, 8, 9, 40, 41, 10, 11, 42, 43, 16, 17, 48, 49, 18, 19, 50, 51, 24, 25, 56, 57, 26, 27, 58, 59,
    //  4, 5, 36, 37, 6, 7, 38, 39, 12, 13, 44, 45, 14, 15, 46, 47, 20, 21, 52, 53, 22, 23, 54, 55, 28, 29, 60, 61, 30, 31, 62, 63,
    invbutterfly_vec \a0, \a1, \zeta2, \zeta2R, \tmp, \tmp2, \q, \hX

    uzp1 \tmpa0\nX, \a0\nX, \a1\nX
    uzp2 \tmpa1\nX, \a0\nX, \a1\nX
    trn1 \a0\nX, \tmpa0\nX, \tmpa1\nX
    trn2 \a1\nX, \tmpa0\nX, \tmpa1\nX
    // layer 3
    //  0, 1, 32, 33, 2, 3, 34, 35, 16, 17, 48, 49, 18, 19, 50, 51, 4, 5, 36, 37, 6, 7, 38, 39, 20, 21, 52, 53, 22, 23, 54, 55,
    //  8, 9, 40, 41, 10, 11, 42, 43, 24, 25, 56, 57, 26, 27, 58, 59, 12, 13, 44, 45, 14, 15, 46, 47, 28, 29, 60, 61, 30, 31, 62, 63,
    invbutterfly_vec \a0, \a1, \zeta3, \zeta3R, \tmp, \tmp2, \q, \hX

    uzp1 \tmpa0\nX, \a0\nX, \a1\nX
    uzp2 \tmpa1\nX, \a0\nX, \a1\nX
    trn1 \a0\nX, \tmpa0\nX, \tmpa1\nX
    trn2 \tmpa1\nX, \tmpa0\nX, \tmpa1\nX
    // layer 4
    // 0, 1, 32, 33, 2, 3, 34, 35, 4, 5, 36, 37, 6, 7, 38, 39, 8, 9, 40, 41, 10, 11, 42, 43, 12, 13, 44, 45, 14, 15, 46, 47,
    // 16, 17, 48, 49, 18, 19, 50, 51, 20, 21, 52, 53, 22, 23, 54, 55, 24, 25, 56, 57, 26, 27, 58, 59, 28, 29, 60, 61, 30, 31, 62, 63,
    invbutterfly_vec \a0, \tmpa1, \zeta4, \zeta4R, \tmp, \tmp2, \q, \hX

    uzp2 \a1\wX, \a0\wX, \tmpa1\wX
    uzp1 \a0\wX, \a0\wX, \tmpa1\wX
.endm


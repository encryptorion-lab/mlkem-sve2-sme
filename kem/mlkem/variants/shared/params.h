#ifndef MLK_VARIANTS_PARAMS_H
#define MLK_VARIANTS_PARAMS_H

/*
 * Parameter macros for the project's SVE/SME assembly. Names and values mirror
 * the liboqs / mlkem-native params.h (MLKEM_K, MLKEM_N, MLKEM_Q) so the SVE/SME
 * sources stay consistent with the baseline instead of carrying a parallel set
 * of Kyber-style definitions.
 *
 * Only the hand-written .S files include this header; the C tables in
 * variants/{sve,sme}/ntt.c pick up the liboqs params.h directly through the
 * include path. MLKEM_K is derived from MLK_CONFIG_PARAMETER_SET (passed by the
 * Makefile to every translation unit, including assembly) so one shared copy
 * serves all three parameter sets. The SVE/SME assembly is parameter-dependent:
 *   __asm_asymmetric_pwm.S : "#if MLKEM_K > 2 / > 3"
 *   __asm_{sve,sme}_*ntt.S : "mov xN, #MLKEM_K"
 *
 * The MLKEM_* definitions are guarded with #ifndef so this header is harmless
 * if the liboqs params.h has already been included in the same unit.
 */
#if !defined(MLK_CONFIG_PARAMETER_SET)
#error "MLK_CONFIG_PARAMETER_SET is not defined"
#endif

#ifndef MLKEM_K
#if   MLK_CONFIG_PARAMETER_SET == 512
#define MLKEM_K 2
#elif MLK_CONFIG_PARAMETER_SET == 768
#define MLKEM_K 3
#elif MLK_CONFIG_PARAMETER_SET == 1024
#define MLKEM_K 4
#else
#error "Invalid MLK_CONFIG_PARAMETER_SET (expected 512, 768 or 1024)"
#endif
#endif /* MLKEM_K */

#ifndef MLKEM_N
#define MLKEM_N 256
#endif

#ifndef MLKEM_Q
#define MLKEM_Q 3329
#endif

#endif /* MLK_VARIANTS_PARAMS_H */

/*
 * SVE arithmetic backend bridge for the ML-KEM SVE2/SME benchmark harness.
 *
 * This header plugs the project's hand-written SVE NTT / INVNTT / base
 * multiplication into mlkem-native's "native backend" hook points. Everything
 * else (sampling, poly_reduce, poly_tomont, poly_tobytes, mulcache, rej_uniform)
 * is reused from the liboqs aarch64 (NEON) backend.
 *
 * Replacement points (vs. plain aarch64/NEON backend):
 *   - mlk_ntt_native      -> forward_ntt_asm  (variants/sve/__asm_sve_ntt.S)
 *   - mlk_intt_native     -> inverse_ntt_asm  (variants/sve/__asm_sve_invntt.S)
 *   - polyvec basemul acc -> asymmetric_*     (variants/shared/__asm_asymmetric_pwm.S)
 *
 * Zeta / twiddle tables live in variants/sve/ntt.c.
 */
#ifndef MLK_SVE2_SME_SVE_META_H
#define MLK_SVE2_SME_SVE_META_H

#define MLK_USE_NATIVE_NTT
#define MLK_USE_NATIVE_INTT
/* Project extension: batch all MLKEM_K forward/inverse NTTs of a polyvec into a
 * single streaming-mode region (see mlk_polyvec_{ntt,invntt_tomont} in
 * poly_k.c, guarded by these macros). */
#define MLK_USE_NATIVE_POLYVEC_NTT
#define MLK_USE_NATIVE_POLYVEC_INTT
#define MLK_USE_NATIVE_POLY_REDUCE
#define MLK_USE_NATIVE_POLY_TOMONT
#define MLK_USE_NATIVE_POLY_MULCACHE_COMPUTE
/* Project extension: compute the SVE/SME-specific preprocessed base-multiply
 * operand ("bprime") for all MLKEM_K polynomials of a polyvec in a single
 * streaming-mode region during cache computation, so the cached base multiply
 * can consume it directly (see mlk_polyvec_mulcache_compute in poly_k.c). */
#define MLK_USE_NATIVE_POLYVEC_MULCACHE_COMPUTE
#define MLK_USE_NATIVE_POLYVEC_BASEMUL_ACC_MONTGOMERY_CACHED
#define MLK_USE_NATIVE_POLY_TOBYTES
#define MLK_USE_NATIVE_REJ_UNIFORM
#define MLK_ARITH_BACKEND_AARCH64
#define MLK_ARITH_BACKEND_AARCH64_SVE

#if !defined(__ASSEMBLER__)
#include "native/api.h"
#include "native/aarch64/src/arith_native_aarch64.h"

extern const int16_t zetas_asm[];
extern const int16_t invzetas_asm[];
extern const int16_t pre_asymmetric_table_sve[];

void forward_ntt_asm(int16_t *src, const int16_t *zeta);
void inverse_ntt_asm(int16_t *src, const int16_t *zeta);
void forward_ntt_polyvec_asm(int16_t *base, const int16_t *zeta);
void inverse_ntt_polyvec_asm(int16_t *base, const int16_t *zeta);
void asymmetric_preprocess_asm(int16_t *bprime, const int16_t *b,
                               const int16_t *zeta);
void asymmetric_preprocess_polyvec_asm(int16_t *bprime, const int16_t *b,
                                       const int16_t *zeta);
void asymmetric_pwm_montgomery_acc_polyvec(int16_t *c, const int16_t *a,
                                           const int16_t *b,
                                           const int16_t *bprime);

MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_ntt_native(int16_t data[MLKEM_N])
{
  forward_ntt_asm(data, zetas_asm);
  return MLK_NATIVE_FUNC_SUCCESS;
}

MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_intt_native(int16_t data[MLKEM_N])
{
  inverse_ntt_asm(data, invzetas_asm);
  return MLK_NATIVE_FUNC_SUCCESS;
}

/* polyvec-level batch hooks: `base` points at vec[0].coeffs of a contiguous
 * MLKEM_K * MLKEM_N int16 polyvec. One streaming region, MLKEM_K transforms. */
static MLK_INLINE void mlk_polyvec_ntt_native(int16_t *base)
{
  forward_ntt_polyvec_asm(base, zetas_asm);
}

static MLK_INLINE void mlk_polyvec_invntt_tomont_native(int16_t *base)
{
  inverse_ntt_polyvec_asm(base, invzetas_asm);
}

MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_poly_reduce_native(int16_t data[MLKEM_N])
{
  mlk_poly_reduce_asm(data);
  return MLK_NATIVE_FUNC_SUCCESS;
}

MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_poly_tomont_native(int16_t data[MLKEM_N])
{
  mlk_poly_tomont_asm(data);
  return MLK_NATIVE_FUNC_SUCCESS;
}

/*
 * The SVE/SME backend repurposes the mlkem-native mulcache as a backend-
 * specific cache holding the preprocessed base-multiply operand ("bprime"),
 * NOT the Neon/mlkem-native mulcache layout. bprime is MLKEM_N/2 int16 per
 * polynomial, matching the mulcache footprint exactly, so the existing
 * mlk_poly_mulcache / mlk_polyvec_mulcache storage is reused unchanged. The
 * cached base multiply (below) then consumes this operand directly instead of
 * regenerating it on every call.
 */
MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_poly_mulcache_compute_native(int16_t x[MLKEM_N / 2],
                                                       const int16_t y[MLKEM_N])
{
  asymmetric_preprocess_asm(x, y, pre_asymmetric_table_sve);
  return MLK_NATIVE_FUNC_SUCCESS;
}

/* polyvec-level cache hook: compute bprime for all MLKEM_K polynomials in a
 * single streaming-mode region. `cache_base` points at vec[0].coeffs of a
 * contiguous MLKEM_K * (MLKEM_N/2) mulcache; `poly_base` at vec[0].coeffs of a
 * contiguous MLKEM_K * MLKEM_N polyvec. */
static MLK_INLINE void mlk_polyvec_mulcache_compute_native(
    int16_t *cache_base, const int16_t *poly_base)
{
  asymmetric_preprocess_polyvec_asm(cache_base, poly_base,
                                    pre_asymmetric_table_sve);
}

#if defined(MLK_CONFIG_MULTILEVEL_WITH_SHARED) || MLKEM_K == 2
MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_polyvec_basemul_acc_montgomery_cached_k2_native(
    int16_t r[MLKEM_N], const int16_t a[2 * MLKEM_N],
    const int16_t b[2 * MLKEM_N], const int16_t b_cache[2 * (MLKEM_N / 2)])
{
  /* b_cache holds the preprocessed bprime computed during cache computation. */
  asymmetric_pwm_montgomery_acc_polyvec(r, a, b, b_cache);
  return MLK_NATIVE_FUNC_SUCCESS;
}
#endif

#if defined(MLK_CONFIG_MULTILEVEL_WITH_SHARED) || MLKEM_K == 3
MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_polyvec_basemul_acc_montgomery_cached_k3_native(
    int16_t r[MLKEM_N], const int16_t a[3 * MLKEM_N],
    const int16_t b[3 * MLKEM_N], const int16_t b_cache[3 * (MLKEM_N / 2)])
{
  asymmetric_pwm_montgomery_acc_polyvec(r, a, b, b_cache);
  return MLK_NATIVE_FUNC_SUCCESS;
}
#endif

#if defined(MLK_CONFIG_MULTILEVEL_WITH_SHARED) || MLKEM_K == 4
MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_polyvec_basemul_acc_montgomery_cached_k4_native(
    int16_t r[MLKEM_N], const int16_t a[4 * MLKEM_N],
    const int16_t b[4 * MLKEM_N], const int16_t b_cache[4 * (MLKEM_N / 2)])
{
  asymmetric_pwm_montgomery_acc_polyvec(r, a, b, b_cache);
  return MLK_NATIVE_FUNC_SUCCESS;
}
#endif

MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_poly_tobytes_native(uint8_t r[MLKEM_POLYBYTES],
                                              const int16_t a[MLKEM_N])
{
  mlk_poly_tobytes_asm(r, a);
  return MLK_NATIVE_FUNC_SUCCESS;
}

MLK_MUST_CHECK_RETURN_VALUE
static MLK_INLINE int mlk_rej_uniform_native(int16_t *r, unsigned len,
                                             const uint8_t *buf,
                                             unsigned buflen)
{
  if (len != MLKEM_N || buflen % 24 != 0)
  {
    return MLK_NATIVE_FUNC_FALLBACK;
  }
  return (int)mlk_rej_uniform_asm(r, buf, buflen, mlk_rej_uniform_table);
}
#endif /* !__ASSEMBLER__ */

#endif /* MLK_SVE2_SME_SVE_META_H */

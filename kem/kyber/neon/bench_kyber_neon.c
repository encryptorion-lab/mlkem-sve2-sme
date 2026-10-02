#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cycles.h"

#include "params.h"
#include "kem.h"
#include "poly.h"
#include "polyvec.h"
#include "ntt.h"

/*
 * NEON base-multiplication entry points (declared in ntt.h with the per-dir
 * namespace baked in). KYBER_NAMESPACE(_asm_x) -> PQCLEAN_KYBERnnn_AARCH64__asm_x.
 * point_mul_extended precomputes the "asymmetric" form of an operand; the
 * asymmetric_mul[_montgomery] kernels then do a length-K polyvec
 * basemul-accumulate into a single output polynomial. The asymmetric_const /
 * pre_asymmetric_table_Q1_extended tables live in ntt.h.
 */
#define point_mul_extended KYBER_NAMESPACE(_asm_point_mul_extended)
#define asymmetric_mul KYBER_NAMESPACE(_asm_asymmetric_mul)
#define asymmetric_mul_montgomery KYBER_NAMESPACE(_asm_asymmetric_mul_montgomery)

/*
 * Benchmark driver for the PQClean aarch64 (NEON) Kyber imported from liboqs
 * (oldpqclean_kyber{512,768,1024}_aarch64). It emits the same CSV schema as
 * kem/kyber/ref/bench_kyber.c so the optimized-NEON Kyber baseline can
 * be placed next to the reference-C Kyber and the ML-KEM ref/neon/sve/sme
 * numbers.
 *
 * Operations: KeyGen, Encaps, Decaps, NTT, INVNTT, plus the composites
 * BaseMulAcc / MatrixVectorMul / InnerProdEnc / InnerProdDec. The NEON base
 * multiplication has no single "polyvec basemul" entry point; it is the
 * point_mul_extended (asymmetric pre-transform) + asymmetric_mul[_montgomery]
 * (length-K accumulate) sequence used inside indcpa.c, replicated here.
 */

#ifndef BENCH_VARIANT
#define BENCH_VARIANT "kyber_neon"
#endif
#ifndef BENCH_PARAMSET
#define BENCH_PARAMSET "Kyber-unknown"
#endif
#ifndef NTESTS
#define NTESTS 1000
#endif
#ifndef BENCH_WARMUP
#define BENCH_WARMUP 100
#endif
#ifndef ARITH_REPS
#define ARITH_REPS 100
#endif

/* KEM entry points are namespaced per directory (PQCLEAN_KYBERxxx_AARCH64_*). */
#define crypto_kem_keypair KYBER_NAMESPACE(crypto_kem_keypair)
#define crypto_kem_enc KYBER_NAMESPACE(crypto_kem_enc)
#define crypto_kem_dec KYBER_NAMESPACE(crypto_kem_dec)

int crypto_kem_keypair(uint8_t *pk, uint8_t *sk);
int crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);
int crypto_kem_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss_enc[CRYPTO_BYTES];
static uint8_t ss_dec[CRYPTO_BYTES];

static int16_t bench_poly[KYBER_N];     /* standard domain */
static int16_t bench_poly_ntt[KYBER_N]; /* NTT domain      */

/* Composite (basemul / matrix-vector) operands, all NTT domain unless noted. */
static int16_t mv_matrix[KYBER_K][KYBER_K][KYBER_N]; /* A */
static int16_t mv_s_std[KYBER_K][KYBER_N];           /* s, standard domain */
static int16_t mv_a[KYBER_K][KYBER_N];               /* a, NTT domain */
static int16_t mv_b[KYBER_K][KYBER_N];               /* b, NTT domain */
static int16_t mv_s_ntt[KYBER_K][KYBER_N];           /* s, NTT domain */
static int16_t mv_out[KYBER_K][KYBER_N];
static int16_t mv_asym[KYBER_K][KYBER_N >> 1];
static int16_t mv_acc[KYBER_N];

static uint64_t samples[NTESTS];

/* Deterministic stand-in RNG; speed figures are RNG-independent. */
static uint32_t rng_state = 0x1234567u;
void randombytes(uint8_t *out, size_t outlen)
{
    for (size_t i = 0; i < outlen; i++) {
        rng_state ^= rng_state << 13;
        rng_state ^= rng_state >> 17;
        rng_state ^= rng_state << 5;
        out[i] = (uint8_t)rng_state;
    }
}

static int cmp_u64(const void *a, const void *b)
{
    const uint64_t aa = *(const uint64_t *)a;
    const uint64_t bb = *(const uint64_t *)b;
    return (aa > bb) - (aa < bb);
}

static double sqrt_newton(double x)
{
    double r;
    if (x <= 0.0) {
        return 0.0;
    }
    r = x >= 1.0 ? x : 1.0;
    for (int i = 0; i < 48; i++) {
        r = 0.5 * (r + x / r);
    }
    return r;
}

static int kem_once(void)
{
    if (crypto_kem_keypair(pk, sk) != 0) {
        return 1;
    }
    if (crypto_kem_enc(ct, ss_enc, pk) != 0) {
        return 1;
    }
    if (crypto_kem_dec(ss_dec, ct, sk) != 0) {
        return 1;
    }
    if (memcmp(ss_enc, ss_dec, sizeof(ss_enc)) != 0) {
        fprintf(stderr, "shared secret mismatch\n");
        return 1;
    }
    return 0;
}

static int run_test(unsigned trials)
{
    for (unsigned i = 0; i < trials; i++) {
        rng_state = 0x12340000u + i;
        if (kem_once() != 0) {
            fprintf(stderr, "trial %u failed\n", i);
            return 1;
        }
    }
    printf("%s,%s,correctness,pass,trials=%u\n", BENCH_VARIANT, BENCH_PARAMSET,
           trials);
    return 0;
}

static void print_stats(const char *operation, uint64_t *s, size_t n)
{
    uint64_t median;
    uint64_t p25;
    uint64_t p75;
    long double mean = 0.0;
    long double variance = 0.0;

    qsort(s, n, sizeof(s[0]), cmp_u64);
    median = (n & 1) ? s[n >> 1] : (s[(n >> 1) - 1] + s[n >> 1]) >> 1;
    p25 = s[n >> 2];
    p75 = s[(3 * n) >> 2];
    for (size_t i = 0; i < n; i++) {
        mean += (long double)s[i];
    }
    mean /= (long double)n;
    for (size_t i = 0; i < n; i++) {
        const long double d = (long double)s[i] - mean;
        variance += d * d;
    }
    variance /= (long double)n;

    printf("%s,%s,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
           ",%.2f,%zu,%s;warmup=%d;arith_reps=%d\n",
           BENCH_VARIANT, BENCH_PARAMSET, operation, median, p25, p75,
           p75 - p25, sqrt_newton((double)variance), n, get_counter_notes(),
           BENCH_WARMUP, ARITH_REPS);
}

#define BENCH_LOOP(OP_NAME, BODY)                          \
    do {                                                   \
        for (int w = 0; w < BENCH_WARMUP; w++) {           \
            for (int j = 0; j < ARITH_REPS; j++) {         \
                BODY                                       \
            }                                              \
        }                                                  \
        for (size_t i = 0; i < NTESTS; i++) {              \
            const uint64_t t0 = get_cycle();               \
            for (int j = 0; j < ARITH_REPS; j++) {         \
                BODY                                       \
            }                                              \
            samples[i] = (get_cycle() - t0) / ARITH_REPS;  \
        }                                                  \
        print_stats(OP_NAME, samples, NTESTS);             \
    } while (0)

static int run_speed(void)
{
    init_counter();

    if (crypto_kem_keypair(pk, sk) != 0 ||
        crypto_kem_enc(ct, ss_enc, pk) != 0) {
        return 1;
    }

    printf("variant,paramset,operation,median,p25,p75,iqr,stddev,n,notes\n");

    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)crypto_kem_keypair(pk, sk);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = get_cycle();
        (void)crypto_kem_keypair(pk, sk);
        samples[i] = get_cycle() - t0;
    }
    print_stats("KeyGen", samples, NTESTS);

    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)crypto_kem_enc(ct, ss_enc, pk);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = get_cycle();
        (void)crypto_kem_enc(ct, ss_enc, pk);
        samples[i] = get_cycle() - t0;
    }
    print_stats("Encaps", samples, NTESTS);

    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)crypto_kem_dec(ss_dec, ct, sk);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = get_cycle();
        (void)crypto_kem_dec(ss_dec, ct, sk);
        samples[i] = get_cycle() - t0;
    }
    print_stats("Decaps", samples, NTESTS);

    for (int i = 0; i < KYBER_N; i++) {
        bench_poly[i] = (int16_t)((i * 17u + 23u) % KYBER_Q);
        bench_poly_ntt[i] = bench_poly[i];
    }
    neon_poly_ntt(bench_poly_ntt); /* produce a valid NTT-domain operand */

    BENCH_LOOP("NTT", {
        int16_t tmp[KYBER_N];
        memcpy(tmp, bench_poly, sizeof(tmp));
        neon_poly_ntt(tmp);
    });

    BENCH_LOOP("INVNTT", {
        int16_t tmp[KYBER_N];
        memcpy(tmp, bench_poly_ntt, sizeof(tmp));
        neon_poly_invntt_tomont(tmp);
    });

    /* Composite operands (mirrors kem/kyber/ref schema). */
    for (int k = 0; k < KYBER_K; k++) {
        for (int i = 0; i < KYBER_N; i++) {
            mv_a[k][i] = (int16_t)(((k + 1u) * 31u + i * 7u) % KYBER_Q);
            mv_b[k][i] = (int16_t)(((k + 3u) * 19u + i * 11u) % KYBER_Q);
            mv_s_std[k][i] = (int16_t)(((k + 2u) * 23u + i * 5u) % KYBER_Q);
            for (int j = 0; j < KYBER_K; j++) {
                mv_matrix[k][j][i] =
                    (int16_t)(((k * KYBER_K + j + 1u) * 29u + i * 7u) % KYBER_Q);
            }
        }
    }
    neon_polyvec_ntt(mv_a);
    neon_polyvec_ntt(mv_b);
    memcpy(mv_s_ntt, mv_s_std, sizeof(mv_s_ntt));
    neon_polyvec_ntt(mv_s_ntt);

    /* acc = sum_j a[j]*b[j] (montgomery), one length-K polyvec inner product. */
    BENCH_LOOP("BaseMulAcc", {
        for (int j = 0; j < KYBER_K; j++) {
            point_mul_extended(&mv_asym[j][0], &mv_b[j][0],
                               pre_asymmetric_table_Q1_extended, asymmetric_const);
        }
        asymmetric_mul_montgomery(&mv_a[0][0], &mv_b[0][0], &mv_asym[0][0],
                                  asymmetric_const, mv_acc);
    });

    /* A*s: ntt(s), per-row basemul-acc (montgomery), inverse-NTT the result. */
    BENCH_LOOP("MatrixVectorMul", {
        int16_t s[KYBER_K][KYBER_N];
        memcpy(s, mv_s_std, sizeof(s));
        neon_polyvec_ntt(s);
        for (int j = 0; j < KYBER_K; j++) {
            point_mul_extended(&mv_asym[j][0], &s[j][0],
                               pre_asymmetric_table_Q1_extended, asymmetric_const);
        }
        for (int r = 0; r < KYBER_K; r++) {
            asymmetric_mul_montgomery(&mv_matrix[r][0][0], &s[0][0],
                                      &mv_asym[0][0], asymmetric_const, mv_out[r]);
        }
        neon_polyvec_invntt_to_mont(mv_out);
    });

    /* Encryption inner product: basemul-acc then single-poly inverse NTT. */
    BENCH_LOOP("InnerProdEnc", {
        for (int j = 0; j < KYBER_K; j++) {
            point_mul_extended(&mv_asym[j][0], &mv_b[j][0],
                               pre_asymmetric_table_Q1_extended, asymmetric_const);
        }
        asymmetric_mul(&mv_a[0][0], &mv_b[0][0], &mv_asym[0][0], asymmetric_const,
                       mv_acc);
        invntt(mv_acc);
    });

    /* Decryption inner product: ntt(b), basemul-acc with s, inverse NTT. */
    BENCH_LOOP("InnerProdDec", {
        int16_t b[KYBER_K][KYBER_N];
        memcpy(b, mv_b, sizeof(b));
        neon_polyvec_ntt(b);
        for (int j = 0; j < KYBER_K; j++) {
            point_mul_extended(&mv_asym[j][0], &b[j][0],
                               pre_asymmetric_table_Q1_extended, asymmetric_const);
        }
        asymmetric_mul(&mv_s_ntt[0][0], &b[0][0], &mv_asym[0][0], asymmetric_const,
                       mv_acc);
        invntt(mv_acc);
    });

    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "--test";

    if (strcmp(mode, "--test") == 0) {
        return run_test(64);
    }
    if (strcmp(mode, "--speed") == 0) {
        return run_speed();
    }

    fprintf(stderr, "usage: %s [--test|--speed]\n", argv[0]);
    return 2;
}

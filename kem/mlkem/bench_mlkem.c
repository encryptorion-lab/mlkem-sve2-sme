#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cycles.h"

#include "kem.h"
#include "poly.h"
#include "poly_k.h"

#ifndef BENCH_VARIANT
#define BENCH_VARIANT "unknown"
#endif

#ifndef BENCH_PARAMSET
#define BENCH_PARAMSET "ML-KEM-unknown"
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

#define KEYPAIR_COINS_BYTES (2 * MLKEM_SYMBYTES)
#define ENC_COINS_BYTES MLKEM_SYMBYTES

static uint8_t pk[MLKEM_INDCCA_PUBLICKEYBYTES];
static uint8_t sk[MLKEM_INDCCA_SECRETKEYBYTES];
static uint8_t ct[MLKEM_INDCCA_CIPHERTEXTBYTES];
static uint8_t ss_enc[MLKEM_SSBYTES];
static uint8_t ss_dec[MLKEM_SSBYTES];
static uint8_t keypair_coins[KEYPAIR_COINS_BYTES];
static uint8_t enc_coins[ENC_COINS_BYTES];
static mlk_poly bench_poly;
static mlk_poly bench_poly_ntt;
static mlk_polyvec bench_a_ntt;
static mlk_polyvec bench_b_ntt;
static mlk_polyvec_mulcache bench_b_cache;
static mlk_poly bench_acc;

/* Operands for the composite arithmetic benchmarks (MatrixVectorMul,
 * InnerProdEnc, InnerProdDec). Mirrors the three WRAP_FUNC blocks in the
 * standalone pqcrystals-style harness, translated to the mlkem-native API
 * (whose base multiply consumes a precomputed mulcache). */
static mlk_polymat bench_matrix;          /* A, NTT domain, coeffs < 4096 */
static mlk_polyvec bench_skpv_src;        /* s, standard domain (< q)      */
static mlk_polyvec bench_skpv_ntt;        /* s, NTT domain                 */
static mlk_polyvec bench_b_src;           /* b, standard domain (< q)      */
static mlk_polyvec bench_pkpv;            /* matrix-vector product output  */

/*
 * Cycle counter from common/cycles.c. On Apple silicon this reads the PMU via
 * kperf and requires root (init_counter()); without it get_cycle() degrades to
 * a constant and the figures are meaningless. On other aarch64 it reads
 * PMCCNTR_EL0. The reported unit is always "cycles".
 */
static uint64_t now_cycles(void)
{
    return get_cycle();
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

static void fill_pattern(uint8_t *buf, size_t len, uint32_t seed)
{
    uint32_t x = seed;
    for (size_t i = 0; i < len; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        buf[i] = (uint8_t)x;
    }
}

static int kem_once(void)
{
    int rc;

    rc = mlk_kem_keypair_derand(pk, sk, keypair_coins, 0);
    if (rc != 0) {
        fprintf(stderr, "keypair_derand failed: %d\n", rc);
        return rc;
    }

    rc = mlk_kem_enc_derand(ct, ss_enc, pk, enc_coins, 0);
    if (rc != 0) {
        fprintf(stderr, "enc_derand failed: %d\n", rc);
        return rc;
    }

    rc = mlk_kem_dec(ss_dec, ct, sk, 0);
    if (rc != 0) {
        fprintf(stderr, "dec failed: %d\n", rc);
        return rc;
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
        fill_pattern(keypair_coins, sizeof(keypair_coins), 0x12340000u + i);
        fill_pattern(enc_coins, sizeof(enc_coins), 0x56780000u + i);
        if (kem_once() != 0) {
            fprintf(stderr, "trial %u failed\n", i);
            return 1;
        }
    }

    printf("%s,%s,correctness,pass,trials=%u\n", BENCH_VARIANT,
           BENCH_PARAMSET, trials);
    return 0;
}

static void print_stats(const char *operation, uint64_t *samples, size_t n)
{
    uint64_t median;
    uint64_t p25;
    uint64_t p75;
    uint64_t iqr;
    long double mean = 0.0;
    long double variance = 0.0;

    qsort(samples, n, sizeof(samples[0]), cmp_u64);
    median = (n & 1) ? samples[n >> 1]
                     : (samples[(n >> 1) - 1] + samples[n >> 1]) >> 1;
    p25 = samples[n >> 2];
    p75 = samples[(3 * n) >> 2];
    iqr = p75 - p25;

    for (size_t i = 0; i < n; i++) {
        mean += (long double)samples[i];
    }
    mean /= (long double)n;

    for (size_t i = 0; i < n; i++) {
        const long double delta = (long double)samples[i] - mean;
        variance += delta * delta;
    }
    variance /= (long double)n;

    printf("%s,%s,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
           ",%.2f,%zu,cycles;warmup=%d;arith_reps=%d\n",
           BENCH_VARIANT, BENCH_PARAMSET, operation, median, p25, p75, iqr,
           sqrt_newton((double)variance), n, BENCH_WARMUP, ARITH_REPS);
}

static int prepare_fixed_keys(void)
{
    fill_pattern(keypair_coins, sizeof(keypair_coins), 0xaabb0001u);
    fill_pattern(enc_coins, sizeof(enc_coins), 0xccdd0001u);
    return kem_once();
}

static void prepare_arithmetic_inputs(void)
{
    for (size_t i = 0; i < MLKEM_N; i++) {
        bench_poly.coeffs[i] = (int16_t)((i * 17u + 23u) % MLKEM_Q);
        bench_poly_ntt.coeffs[i] = bench_poly.coeffs[i];
    }
    mlk_poly_ntt(&bench_poly_ntt);

    for (size_t k = 0; k < MLKEM_K; k++) {
        for (size_t i = 0; i < MLKEM_N; i++) {
            bench_a_ntt.vec[k].coeffs[i] =
                (int16_t)(((k + 1u) * 31u + i * 7u) % MLKEM_Q);
            bench_b_ntt.vec[k].coeffs[i] =
                (int16_t)(((k + 3u) * 19u + i * 11u) % MLKEM_Q);
        }
        mlk_poly_ntt(&bench_a_ntt.vec[k]);
        mlk_poly_ntt(&bench_b_ntt.vec[k]);
    }
    mlk_polyvec_mulcache_compute(&bench_b_cache, &bench_b_ntt);

    /* Composite-benchmark operands. The matrix and skpv_ntt live in the NTT
     * domain (coefficients reduced mod q, hence < 4096 as the base multiply
     * requires); the *_src vectors stay in the standard domain so the timed
     * blocks can re-run polyvec_ntt on a fresh, in-bound copy each iteration. */
    for (size_t k = 0; k < MLKEM_K; k++) {
        for (size_t i = 0; i < MLKEM_N; i++) {
            bench_skpv_src.vec[k].coeffs[i] =
                (int16_t)(((k + 2u) * 23u + i * 5u) % MLKEM_Q);
            bench_b_src.vec[k].coeffs[i] =
                (int16_t)(((k + 5u) * 13u + i * 3u) % MLKEM_Q);
        }
        bench_skpv_ntt.vec[k] = bench_skpv_src.vec[k];
        mlk_poly_ntt(&bench_skpv_ntt.vec[k]);

        for (size_t j = 0; j < MLKEM_K; j++) {
            for (size_t i = 0; i < MLKEM_N; i++) {
                bench_matrix.vec[k].vec[j].coeffs[i] =
                    (int16_t)(((k * MLKEM_K + j + 1u) * 29u + i * 7u) % MLKEM_Q);
            }
        }
    }
}

static void bench_ntt(uint64_t *samples)
{
    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly;
            mlk_poly_ntt(&tmp);
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly;
            mlk_poly_ntt(&tmp);
        }
        samples[i] = (now_cycles() - t0) / ARITH_REPS;
    }
    print_stats("NTT", samples, NTESTS);
}

static void bench_invntt(uint64_t *samples)
{
    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly_ntt;
            mlk_poly_invntt_tomont(&tmp);
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly_ntt;
            mlk_poly_invntt_tomont(&tmp);
        }
        samples[i] = (now_cycles() - t0) / ARITH_REPS;
    }
    print_stats("INVNTT", samples, NTESTS);
}

static void bench_basemul(uint64_t *samples)
{
    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_polyvec_basemul_acc_montgomery_cached(&bench_acc,
                                                      &bench_a_ntt,
                                                      &bench_b_ntt,
                                                      &bench_b_cache);
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_polyvec_basemul_acc_montgomery_cached(&bench_acc,
                                                      &bench_a_ntt,
                                                      &bench_b_ntt,
                                                      &bench_b_cache);
        }
        samples[i] = (now_cycles() - t0) / ARITH_REPS;
    }
    print_stats("BaseMulAcc", samples, NTESTS);
}

/*
 * MatrixVectorMul: forward-NTT the secret, multiply by the (NTT-domain) matrix
 * row by row with Montgomery accumulation, convert each result to the Montgomery
 * domain, then inverse-NTT the product vector. Equivalent to the pqcrystals
 * harness block, using the mlkem-native cached base multiply (so the per-row
 * mulcache of the freshly transformed secret is part of the operation).
 */
static void matrixvectormul_once(void)
{
    mlk_polyvec skpv = bench_skpv_src;
    mlk_polyvec_mulcache skpv_cache;

    mlk_polyvec_ntt(&skpv);
    mlk_polyvec_mulcache_compute(&skpv_cache, &skpv);
    for (size_t j = 0; j < MLKEM_K; j++) {
        mlk_polyvec_basemul_acc_montgomery_cached(
            &bench_pkpv.vec[j], &bench_matrix.vec[j], &skpv, &skpv_cache);
        mlk_poly_tomont(&bench_pkpv.vec[j]);
    }
    mlk_polyvec_invntt_tomont(&bench_pkpv);
}

static void bench_matrixvectormul(uint64_t *samples)
{
    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            matrixvectormul_once();
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            matrixvectormul_once();
        }
        samples[i] = (now_cycles() - t0) / ARITH_REPS;
    }
    print_stats("MatrixVectorMul", samples, NTESTS);
}

/*
 * InnerProdEnc: pointwise multiply-accumulate of the (NTT-domain) public key by
 * the (NTT-domain) ephemeral secret, then inverse-NTT the scalar result. The
 * secret's mulcache is reused across the encryption, so it is precomputed and
 * not part of the timed region (matching how indcpa enc uses it).
 */
static void innerprodenc_once(void)
{
    mlk_polyvec_basemul_acc_montgomery_cached(&bench_acc, &bench_a_ntt,
                                              &bench_b_ntt, &bench_b_cache);
    mlk_poly_invntt_tomont(&bench_acc);
}

static void bench_innerprodenc(uint64_t *samples)
{
    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            innerprodenc_once();
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            innerprodenc_once();
        }
        samples[i] = (now_cycles() - t0) / ARITH_REPS;
    }
    print_stats("InnerProdEnc", samples, NTESTS);
}

/*
 * InnerProdDec: forward-NTT the ciphertext vector, pointwise multiply-accumulate
 * with the (NTT-domain) secret, then inverse-NTT the scalar result. The
 * ciphertext's mulcache depends on the just-computed NTT, so it is part of the
 * operation (matching indcpa dec).
 */
static void innerproddec_once(void)
{
    mlk_polyvec b = bench_b_src;
    mlk_polyvec_mulcache b_cache;

    mlk_polyvec_ntt(&b);
    mlk_polyvec_mulcache_compute(&b_cache, &b);
    mlk_polyvec_basemul_acc_montgomery_cached(&bench_acc, &bench_skpv_ntt, &b,
                                              &b_cache);
    mlk_poly_invntt_tomont(&bench_acc);
}

static void bench_innerproddec(uint64_t *samples)
{
    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            innerproddec_once();
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            innerproddec_once();
        }
        samples[i] = (now_cycles() - t0) / ARITH_REPS;
    }
    print_stats("InnerProdDec", samples, NTESTS);
}

static int run_speed(void)
{
    uint64_t *samples = calloc(NTESTS, sizeof(*samples));
    if (samples == NULL) {
        fprintf(stderr, "calloc failed: %s\n", strerror(errno));
        return 1;
    }

    /* Set up the PMU cycle counter (no-op / best effort without root). */
    init_counter();

    if (prepare_fixed_keys() != 0) {
        free(samples);
        return 1;
    }

    printf("variant,paramset,operation,median,p25,p75,iqr,stddev,n,notes\n");

    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)mlk_kem_keypair_derand(pk, sk, keypair_coins, 0);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        (void)mlk_kem_keypair_derand(pk, sk, keypair_coins, 0);
        samples[i] = now_cycles() - t0;
    }
    print_stats("KeyGen", samples, NTESTS);

    if (prepare_fixed_keys() != 0) {
        free(samples);
        return 1;
    }
    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)mlk_kem_enc_derand(ct, ss_enc, pk, enc_coins, 0);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        (void)mlk_kem_enc_derand(ct, ss_enc, pk, enc_coins, 0);
        samples[i] = now_cycles() - t0;
    }
    print_stats("Encaps", samples, NTESTS);

    if (prepare_fixed_keys() != 0) {
        free(samples);
        return 1;
    }
    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)mlk_kem_dec(ss_dec, ct, sk, 0);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        (void)mlk_kem_dec(ss_dec, ct, sk, 0);
        samples[i] = now_cycles() - t0;
    }
    print_stats("Decaps", samples, NTESTS);

    prepare_arithmetic_inputs();
    bench_ntt(samples);
    bench_invntt(samples);
    bench_basemul(samples);
    bench_matrixvectormul(samples);
    bench_innerprodenc(samples);
    bench_innerproddec(samples);

    free(samples);
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

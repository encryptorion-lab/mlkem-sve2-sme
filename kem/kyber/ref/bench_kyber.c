#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cycles.h"

#include "api.h"
#include "kem.h"
#include "params.h"
#include "poly.h"
#include "polyvec.h"

/*
 * Benchmark driver for the imported pqcrystals Kyber (round 3) reference,
 * matching the CSV output of kem/mlkem/bench_mlkem.c so the
 * legacy-Kyber baseline can be compared directly against the ML-KEM variants.
 * The three composite operations (MatrixVectorMul, InnerProdEnc, InnerProdDec)
 * mirror the WRAP_FUNC blocks in the standalone pqcrystals-style harness.
 */

#ifndef BENCH_VARIANT
#define BENCH_VARIANT "kyber_ref"
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

static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss_enc[CRYPTO_BYTES];
static uint8_t ss_dec[CRYPTO_BYTES];

/* Arithmetic operands. */
static poly bench_poly;        /* standard domain                       */
static poly bench_poly_ntt;    /* NTT domain                            */
static polyvec bench_a_ntt;    /* NTT domain                            */
static polyvec bench_b_ntt;    /* NTT domain                            */
static poly bench_acc;         /* base-multiply output                  */
static polyvec bench_matrix[KYBER_K]; /* A, NTT domain                  */
static polyvec bench_skpv_src; /* s, standard domain                    */
static polyvec bench_skpv_ntt; /* s, NTT domain                         */
static polyvec bench_b_src;    /* b, standard domain                    */
static polyvec bench_pkpv;     /* matrix-vector product output          */
static poly bench_v;           /* inner-product output                  */

/*
 * Deterministic stand-in for the system RNG. The KEM correctness check below
 * only needs reproducibility, and the speed figures are RNG-independent.
 */
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

static void prepare_arithmetic_inputs(void)
{
    for (int i = 0; i < KYBER_N; i++) {
        bench_poly.coeffs[i] = (int16_t)((i * 17u + 23u) % KYBER_Q);
        bench_poly_ntt.coeffs[i] = bench_poly.coeffs[i];
    }
    poly_ntt(&bench_poly_ntt);

    for (int k = 0; k < KYBER_K; k++) {
        for (int i = 0; i < KYBER_N; i++) {
            bench_a_ntt.vec[k].coeffs[i] =
                (int16_t)(((k + 1u) * 31u + i * 7u) % KYBER_Q);
            bench_b_ntt.vec[k].coeffs[i] =
                (int16_t)(((k + 3u) * 19u + i * 11u) % KYBER_Q);
            bench_skpv_src.vec[k].coeffs[i] =
                (int16_t)(((k + 2u) * 23u + i * 5u) % KYBER_Q);
            bench_b_src.vec[k].coeffs[i] =
                (int16_t)(((k + 5u) * 13u + i * 3u) % KYBER_Q);
        }
        poly_ntt(&bench_a_ntt.vec[k]);
        poly_ntt(&bench_b_ntt.vec[k]);

        bench_skpv_ntt.vec[k] = bench_skpv_src.vec[k];
        poly_ntt(&bench_skpv_ntt.vec[k]);

        for (int j = 0; j < KYBER_K; j++) {
            for (int i = 0; i < KYBER_N; i++) {
                bench_matrix[k].vec[j].coeffs[i] =
                    (int16_t)(((k * KYBER_K + j + 1u) * 29u + i * 7u) % KYBER_Q);
            }
        }
    }
}

#define BENCH_LOOP(OP_NAME, BODY)                                  \
    do {                                                           \
        for (int w = 0; w < BENCH_WARMUP; w++) {                   \
            for (int j = 0; j < ARITH_REPS; j++) {                 \
                BODY                                               \
            }                                                      \
        }                                                          \
        for (size_t i = 0; i < NTESTS; i++) {                      \
            const uint64_t t0 = now_cycles();                      \
            for (int j = 0; j < ARITH_REPS; j++) {                 \
                BODY                                               \
            }                                                      \
            samples[i] = (now_cycles() - t0) / ARITH_REPS;         \
        }                                                          \
        print_stats(OP_NAME, samples, NTESTS);                     \
    } while (0)

static int run_speed(void)
{
    uint64_t *samples = calloc(NTESTS, sizeof(*samples));
    if (samples == NULL) {
        fprintf(stderr, "calloc failed: %s\n", strerror(errno));
        return 1;
    }

    init_counter();

    fill_pattern(pk, 1, 0xaa); /* touch buffers */
    if (crypto_kem_keypair(pk, sk) != 0 ||
        crypto_kem_enc(ct, ss_enc, pk) != 0) {
        free(samples);
        return 1;
    }

    printf("variant,paramset,operation,median,p25,p75,iqr,stddev,n,notes\n");

    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)crypto_kem_keypair(pk, sk);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        (void)crypto_kem_keypair(pk, sk);
        samples[i] = now_cycles() - t0;
    }
    print_stats("KeyGen", samples, NTESTS);

    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)crypto_kem_enc(ct, ss_enc, pk);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        (void)crypto_kem_enc(ct, ss_enc, pk);
        samples[i] = now_cycles() - t0;
    }
    print_stats("Encaps", samples, NTESTS);

    for (int i = 0; i < BENCH_WARMUP; i++) {
        (void)crypto_kem_dec(ss_dec, ct, sk);
    }
    for (size_t i = 0; i < NTESTS; i++) {
        const uint64_t t0 = now_cycles();
        (void)crypto_kem_dec(ss_dec, ct, sk);
        samples[i] = now_cycles() - t0;
    }
    print_stats("Decaps", samples, NTESTS);

    prepare_arithmetic_inputs();

    BENCH_LOOP("NTT", {
        poly tmp = bench_poly;
        poly_ntt(&tmp);
    });

    BENCH_LOOP("INVNTT", {
        poly tmp = bench_poly_ntt;
        poly_invntt_tomont(&tmp);
    });

    BENCH_LOOP("BaseMulAcc", {
        polyvec_basemul_acc_montgomery(&bench_acc, &bench_a_ntt, &bench_b_ntt);
    });

    BENCH_LOOP("MatrixVectorMul", {
        polyvec skpv = bench_skpv_src;
        polyvec_ntt(&skpv);
        for (int r = 0; r < KYBER_K; r++) {
            polyvec_basemul_acc_montgomery(&bench_pkpv.vec[r], &bench_matrix[r],
                                           &skpv);
            poly_tomont(&bench_pkpv.vec[r]);
        }
        polyvec_invntt_tomont(&bench_pkpv);
    });

    BENCH_LOOP("InnerProdEnc", {
        polyvec_basemul_acc_montgomery(&bench_v, &bench_a_ntt, &bench_b_ntt);
        poly_invntt_tomont(&bench_v);
    });

    BENCH_LOOP("InnerProdDec", {
        polyvec b = bench_b_src;
        polyvec_ntt(&b);
        polyvec_basemul_acc_montgomery(&bench_v, &bench_skpv_ntt, &b);
        poly_invntt_tomont(&bench_v);
    });

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

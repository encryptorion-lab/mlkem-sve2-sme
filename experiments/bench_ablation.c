/*
 * Experiment-only correctness/speed wrapper.  This deliberately includes the
 * repository benchmark translation unit so every stage uses the same ML-KEM
 * setup and timing path as the production backends.
 */
#define main mlkem_repository_benchmark_main
#include "../kem/mlkem/bench_mlkem.c"
#undef main

#include <stdint.h>

#ifndef ABLATION_STAGE
#error "ABLATION_STAGE must be defined"
#endif

#define CORRECTNESS_TRIALS 256u
#define ARITH_TRIALS 64u

static int16_t canonical(int64_t x)
{
    x %= MLKEM_Q;
    if (x < 0) {
        x += MLKEM_Q;
    }
    return (int16_t)x;
}

static int deterministic_replay_test(void)
{
    uint8_t pk_copy[sizeof(pk)], sk_copy[sizeof(sk)], ct_copy[sizeof(ct)];
    uint8_t ss_copy[sizeof(ss_enc)];

    for (unsigned trial = 0; trial < 32; trial++) {
        fill_pattern(keypair_coins, sizeof(keypair_coins),
                     0xd3710000u + trial);
        fill_pattern(enc_coins, sizeof(enc_coins), 0xe5ca0000u + trial);
        if (kem_once() != 0) {
            return 1;
        }
        memcpy(pk_copy, pk, sizeof(pk));
        memcpy(sk_copy, sk, sizeof(sk));
        memcpy(ct_copy, ct, sizeof(ct));
        memcpy(ss_copy, ss_enc, sizeof(ss_enc));

        if (kem_once() != 0 ||
            memcmp(pk_copy, pk, sizeof(pk)) != 0 ||
            memcmp(sk_copy, sk, sizeof(sk)) != 0 ||
            memcmp(ct_copy, ct, sizeof(ct)) != 0 ||
            memcmp(ss_copy, ss_enc, sizeof(ss_enc)) != 0 ||
            memcmp(ss_copy, ss_dec, sizeof(ss_dec)) != 0) {
            fprintf(stderr, "S%d deterministic replay failed at trial %u\n",
                    ABLATION_STAGE, trial);
            return 1;
        }
    }
    printf("%s,%s,deterministic-replay,pass,trials=32\n",
           BENCH_VARIANT, BENCH_PARAMSET);
    return 0;
}

/*
 * invntt_tomont intentionally includes the Montgomery conversion used by
 * ML-KEM.  Compare against the backend's own poly_tomont transformation, not
 * against the original polynomial.  This test is layout-aware: it never
 * compares a MatNTT-domain array with a canonical/VecNTT-domain array.
 */
static int paired_ntt_test(void)
{
    mlk_poly original;
    mlk_poly expected;
    mlk_poly transformed;

    for (unsigned trial = 0; trial < ARITH_TRIALS; trial++) {
        uint32_t x = 0x9e3779b9u ^ (trial * 0x45d9f3bu);
        for (size_t i = 0; i < MLKEM_N; i++) {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            original.coeffs[i] = (int16_t)(x % MLKEM_Q);
        }
        expected = original;
        mlk_poly_tomont(&expected);
        transformed = original;
        mlk_poly_ntt(&transformed);
        mlk_poly_invntt_tomont(&transformed);
        for (size_t i = 0; i < MLKEM_N; i++) {
            if (canonical(transformed.coeffs[i]) !=
                canonical(expected.coeffs[i])) {
                fprintf(stderr,
                        "S%d paired NTT mismatch trial=%u coefficient=%zu "
                        "got=%d expected=%d\n",
                        ABLATION_STAGE, trial, i,
                        canonical(transformed.coeffs[i]),
                        canonical(expected.coeffs[i]));
                return 1;
            }
        }
    }
    printf("%s,%s,paired-ntt-invntt,pass,trials=%u\n",
           BENCH_VARIANT, BENCH_PARAMSET, ARITH_TRIALS);
    return 0;
}

/*
 * End-to-end negacyclic multiplication is the layout-independent arithmetic
 * oracle for MatNTT stages.  Only vec[0] is populated; the remaining ML-KEM
 * vector components are zero.
 */
static int polynomial_multiplication_test(void)
{
    mlk_polyvec a = {0};
    mlk_polyvec b = {0};
    mlk_polyvec_mulcache cache;
    mlk_poly result;
    int16_t input_a[MLKEM_N];
    int16_t input_b[MLKEM_N];
    int64_t expected[MLKEM_N];

    for (unsigned trial = 0; trial < ARITH_TRIALS; trial++) {
        memset(expected, 0, sizeof(expected));
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        for (size_t i = 0; i < MLKEM_N; i++) {
            input_a[i] =
                (int16_t)((i * 17u + trial * 29u + 3u) % MLKEM_Q);
            input_b[i] =
                (int16_t)((i * 31u + trial * 11u + 7u) % MLKEM_Q);
            a.vec[0].coeffs[i] = input_a[i];
            b.vec[0].coeffs[i] = input_b[i];
        }
        for (size_t i = 0; i < MLKEM_N; i++) {
            for (size_t j = 0; j < MLKEM_N; j++) {
                size_t degree = i + j;
                int64_t product = (int64_t)input_a[i] * input_b[j];
                if (degree < MLKEM_N) {
                    expected[degree] += product;
                } else {
                    expected[degree - MLKEM_N] -= product;
                }
            }
        }

        mlk_polyvec_ntt(&a);
        mlk_polyvec_ntt(&b);
        mlk_polyvec_mulcache_compute(&cache, &b);
        mlk_polyvec_basemul_acc_montgomery_cached(&result, &a, &b, &cache);
        mlk_poly_invntt_tomont(&result);

        for (size_t i = 0; i < MLKEM_N; i++) {
            if (canonical(result.coeffs[i]) != canonical(expected[i])) {
                fprintf(stderr,
                        "S%d polynomial product mismatch trial=%u "
                        "coefficient=%zu got=%d expected=%d\n",
                        ABLATION_STAGE, trial, i,
                        canonical(result.coeffs[i]), canonical(expected[i]));
                return 1;
            }
        }
    }
    printf("%s,%s,standard-domain-polymul,pass,trials=%u\n",
           BENCH_VARIANT, BENCH_PARAMSET, ARITH_TRIALS);
    return 0;
}

static int counter_is_usable(void)
{
    volatile uint64_t sink = 0;
    uint64_t before;
    uint64_t after;

    init_counter();
    before = get_cycle();
    for (unsigned i = 0; i < 100000; i++) {
        sink += i;
    }
    after = get_cycle();
    (void)sink;
    if (after <= before) {
        fprintf(stderr,
                "fatal: wall-clock timer is not advancing "
                "(before=%" PRIu64 ", after=%" PRIu64 ").\n",
                before, after);
        return 0;
    }
    return 1;
}

static void print_raw_sample(const char *operation, size_t index,
                             uint64_t cycles)
{
    printf("S%d,%s,%s,%zu,%" PRIu64 "\n", ABLATION_STAGE,
           BENCH_PARAMSET, operation, index, cycles);
}

static int run_raw_speed(void)
{
    prepare_arithmetic_inputs();
    printf("stage,paramset,operation,sample,cycles\n");

    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly;
            mlk_poly_ntt(&tmp);
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly;
            mlk_poly_ntt(&tmp);
        }
        print_raw_sample("NTT", i, (now_cycles() - t0) / ARITH_REPS);
    }

    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly_ntt;
            mlk_poly_invntt_tomont(&tmp);
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            mlk_poly tmp = bench_poly_ntt;
            mlk_poly_invntt_tomont(&tmp);
        }
        print_raw_sample("INVNTT", i,
                         (now_cycles() - t0) / ARITH_REPS);
    }

    for (int i = 0; i < BENCH_WARMUP; i++) {
        for (int j = 0; j < ARITH_REPS; j++) {
            matrixvectormul_once();
        }
    }
    for (size_t i = 0; i < NTESTS; i++) {
        uint64_t t0 = now_cycles();
        for (int j = 0; j < ARITH_REPS; j++) {
            matrixvectormul_once();
        }
        print_raw_sample("MatrixVectorMul", i,
                         (now_cycles() - t0) / ARITH_REPS);
    }
    return 0;
}

static int run_ablation_correctness(void)
{
    if (run_test(CORRECTNESS_TRIALS) != 0 ||
        deterministic_replay_test() != 0 ||
        paired_ntt_test() != 0 ||
        polynomial_multiplication_test() != 0) {
        return 1;
    }
    printf("%s,%s,ablation-suite,pass,stage=S%d\n",
           BENCH_VARIANT, BENCH_PARAMSET, ABLATION_STAGE);
    return 0;
}

static int dump_ntt(void)
{
    mlk_poly value;
    for (size_t i = 0; i < MLKEM_N; i++) {
        value.coeffs[i] = (int16_t)((i * 17u + 23u) % MLKEM_Q);
    }
    mlk_poly_ntt(&value);
    for (size_t i = 0; i < MLKEM_N; i++) {
        printf("%zu,%d\n", i, canonical(value.coeffs[i]));
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "--test";
    if (strcmp(mode, "--test") == 0) {
        return run_ablation_correctness();
    }
    if (strcmp(mode, "--speed") == 0) {
        if (!counter_is_usable()) {
            return 1;
        }
        return run_speed();
    }
    if (strcmp(mode, "--raw-speed") == 0) {
        if (!counter_is_usable()) {
            return 1;
        }
        return run_raw_speed();
    }
    if (strcmp(mode, "--dump-ntt") == 0) {
        return dump_ntt();
    }
    fprintf(stderr,
            "usage: %s [--test|--speed|--raw-speed|--dump-ntt]\n", argv[0]);
    return 2;
}

#ifndef CYCLES_H
#define CYCLES_H

/*
 * Wall-clock timing converted to estimated core cycles.
 *
 * Apple: CLOCK_UPTIME_RAW (same timebase as C++ steady_clock / mach_absolute_time,
 * reported in nanoseconds). Other hosts: CLOCK_MONOTONIC_RAW.
 * Cycles = elapsed_ns * recorded_P_core_Hz / 1e9.
 *
 * Frequency resolution order: BENCH_CPU_HZ, sysctl hw.cpufrequency(_max),
 * then the advertised P-core maximum for known Apple chips.
 * The measuring thread requests QOS_CLASS_USER_INTERACTIVE so macOS prefers
 * performance cores.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

void init_counter(void);
uint64_t get_cycle(void);
uint64_t get_time_ns(void);
uint64_t ns_to_cycles(uint64_t ns);
uint64_t get_cpu_hz(void);
uint64_t get_timer_res_ns(void);
const char *get_timer_name(void);
const char *get_cpu_hz_source(void);
const char *get_counter_notes(void);

#ifndef BENCH_WARMUP
#define BENCH_WARMUP 100
#endif

#ifndef BENCH_VARIANT
#define BENCH_VARIANT "unknown"
#endif

#ifndef BENCH_PARAMSET
#if defined(KYBER_K) && KYBER_K == 2
#define BENCH_PARAMSET "ML-KEM-512"
#elif defined(KYBER_K) && KYBER_K == 3
#define BENCH_PARAMSET "ML-KEM-768"
#elif defined(KYBER_K) && KYBER_K == 4
#define BENCH_PARAMSET "ML-KEM-1024"
#else
#define BENCH_PARAMSET "unknown"
#endif
#endif

#define BENCH_STRINGIFY2(x) #x
#define BENCH_STRINGIFY(x) BENCH_STRINGIFY2(x)

#ifndef BENCH_NOTES
#define BENCH_NOTES "warmup=" BENCH_STRINGIFY(BENCH_WARMUP) ";timer=wallclock"
#endif

#ifdef __AVERAGE__

#define LOOP_INIT(__clock0, __clock1) { \
__clock0 = get_cycle(); \
}
#define LOOP_TAIL(__f_string, records, __clock0, __clock1) { \
__clock1 = get_cycle(); \
records[NTESTS>>1] = (__clock1 - __clock0) / NTESTS; \
printf(__f_string " average cycles: %lld\n", (__clock1 - __clock0) / NTESTS); \
}
#define BODY_INIT(__clock0, __clock1) {}
#define BODY_TAIL(records, __clock0, __clock1) {}

#elif defined(__MEDIAN__)

#include <stdlib.h>

static int cmp_uint64(const void *a, const void *b){
    const uint64_t aa = *((const uint64_t*)a);
    const uint64_t bb = *((const uint64_t*)b);
    return (aa > bb) - (aa < bb);
}

static double bench_sqrt(double x) {
    double r;
    if (x <= 0.0) {
        return 0.0;
    }
    r = x >= 1.0 ? x : 1.0;
    for (size_t i = 0; i < 48; i++) {
        r = 0.5 * (r + x / r);
    }
    return r;
}

static void bench_print_stats(const char *operation, uint64_t *records, size_t n) {
    static int printed_header = 0;
    uint64_t median;
    uint64_t p25;
    uint64_t p75;
    uint64_t iqr;
    long double mean = 0.0;
    long double variance = 0.0;
    double stddev;

    qsort(records, n, sizeof(uint64_t), cmp_uint64);

    if ((n & 1) == 0) {
        median = (records[(n >> 1) - 1] + records[n >> 1]) >> 1;
    } else {
        median = records[n >> 1];
    }
    p25 = records[n >> 2];
    p75 = records[(3 * n) >> 2];
    iqr = p75 - p25;

    for (size_t i = 0; i < n; i++) {
        mean += (long double)records[i];
    }
    mean /= (long double)n;

    for (size_t i = 0; i < n; i++) {
        const long double delta = (long double)records[i] - mean;
        variance += delta * delta;
    }
    variance /= (long double)n;
    stddev = bench_sqrt((double)variance);

    if (!printed_header) {
        printf("variant,paramset,operation,median,p25,p75,iqr,stddev,n,notes\n");
        printed_header = 1;
    }
    printf("%s,%s,%s,%llu,%llu,%llu,%llu,%.2f,%zu,%s;warmup=%s\n",
           BENCH_VARIANT,
           BENCH_PARAMSET,
           operation,
           (unsigned long long)median,
           (unsigned long long)p25,
           (unsigned long long)p75,
           (unsigned long long)iqr,
           stddev,
           n,
           get_counter_notes(),
           BENCH_STRINGIFY(BENCH_WARMUP));
}

#define LOOP_INIT(__clock0, __clock1) {}
#define LOOP_TAIL(__f_string, records, __clock0, __clock1) { \
bench_print_stats(__f_string, records, NTESTS); \
}
#define BODY_INIT(__clock0, __clock1) { \
__clock0 = get_cycle(); \
}
#define BODY_TAIL(records, __clock0, __clock1) { \
__clock1 = get_cycle(); \
records[i] = __clock1 - __clock0; \
}

#endif

#define WRAP_FUNC(__f_string, records, __clock0, __clock1, func){ \
for(size_t warmup_i = 0; warmup_i < BENCH_WARMUP; warmup_i++){ \
func; \
} \
LOOP_INIT(__clock0, __clock1); \
for(size_t i = 0; i < NTESTS; i++){ \
BODY_INIT(__clock0, __clock1); \
func; \
BODY_TAIL(records, __clock0, __clock1); \
} \
LOOP_TAIL(__f_string, records, __clock0, __clock1); \
}

#endif

/*
 * Wall-clock calibration for the paper timers. Every probe is bracketed
 * with CLOCK_UPTIME_RAW (or CLOCK_MONOTONIC_RAW) timestamps. Reported
 * cycles are elapsed_ns * recorded_P_core_Hz / 1e9. SME work stays in
 * calibrate_sme.S so this file never enters streaming mode.
 */

#include <inttypes.h>
#include <stdio.h>

#include "cycles.h"

#ifndef CALIB_ITERS
#define CALIB_ITERS 10000
#endif

#ifndef CALIB_SCALAR
#define CALIB_SCALAR 1000000u
#endif

#ifndef CALIB_WARMUP
#define CALIB_WARMUP 100
#endif

#ifndef CALIB_SWITCH_REPS
#define CALIB_SWITCH_REPS 100
#endif

void calib_mode_switch(void);
void calib_ssve_muls(void);

static volatile uint64_t scalar_sink;

static void scalar_batch(void)
{
    uint64_t acc = 1;
    for (uint32_t i = 0; i < CALIB_SCALAR; i++) {
        acc = acc * 6364136223846793005ull + 1ull;
    }
    scalar_sink = acc;
}

static void emit(const char *kind, int sample, uint64_t elapsed_ns,
                 uint32_t work_units)
{
    printf("%s,%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%s,%s,%u,%" PRIu64 "\n",
           kind, sample, elapsed_ns, ns_to_cycles(elapsed_ns), get_cpu_hz(),
           get_timer_name(), get_cpu_hz_source(), work_units,
           get_timer_res_ns());
}

int main(void)
{
    init_counter();

    for (int i = 0; i < CALIB_WARMUP; i++) {
        scalar_batch();
        calib_mode_switch();
        calib_ssve_muls();
        (void)get_time_ns();
        (void)get_cycle();
    }

    printf("kind,sample,elapsed_ns,est_cycles,cpu_hz,timer,hz_source,work_units,timer_res_ns\n");

    for (int i = 0; i < CALIB_ITERS; i++) {
        const uint64_t t0 = get_time_ns();
        const uint64_t t1 = get_time_ns();
        emit("empty", i, t1 - t0, 0);
    }

    for (int i = 0; i < CALIB_ITERS; i++) {
        const uint64_t t0 = get_time_ns();
        scalar_batch();
        const uint64_t t1 = get_time_ns();
        emit("scalar_loop", i, t1 - t0, CALIB_SCALAR);
    }

    for (int i = 0; i < CALIB_ITERS; i++) {
        const uint64_t t0 = get_time_ns();
        for (int r = 0; r < CALIB_SWITCH_REPS; r++) {
            calib_mode_switch();
        }
        const uint64_t t1 = get_time_ns();
        emit("smstart_smstop", i, t1 - t0, CALIB_SWITCH_REPS);
    }

    for (int i = 0; i < CALIB_ITERS; i++) {
        const uint64_t t0 = get_time_ns();
        calib_ssve_muls();
        const uint64_t t1 = get_time_ns();
        emit("ssve_mul", i, t1 - t0, 256);
    }

    return 0;
}

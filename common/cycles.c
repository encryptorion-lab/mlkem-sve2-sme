#include "cycles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <pthread.h>
#include <sys/sysctl.h>
#endif

#ifdef __APPLE__
#define TIMER_NAME "CLOCK_UPTIME_RAW"
#else
#define TIMER_NAME "CLOCK_MONOTONIC_RAW"
#endif

static uint64_t g_cpu_hz;
static uint64_t g_timer_res_ns;
static const char *g_hz_source = "unset";
static char g_notes[384];
static int g_ready;

static uint64_t now_ns(void)
{
#ifdef __APPLE__
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

static int parse_hz(const char *text, uint64_t *out)
{
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    value = strtoull(text, &end, 10);
    if (end == text || *end != '\0' || value == 0) {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

#ifdef __APPLE__
static int sysctl_u64(const char *name, uint64_t *out)
{
    uint64_t value = 0;
    size_t size = sizeof(value);

    if (sysctlbyname(name, &value, &size, NULL, 0) != 0 || value == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

static int advertised_pcore_hz(uint64_t *out, const char **source)
{
    char brand[128];
    size_t size = sizeof(brand);

    memset(brand, 0, sizeof(brand));
    if (sysctlbyname("machdep.cpu.brand_string", brand, &size, NULL, 0) != 0) {
        return 0;
    }
    if (strstr(brand, "M4 Pro") != NULL || strstr(brand, "M4 Max") != NULL) {
        *out = 4510000000ull;
        *source = "advertised-pcore-max:Apple-M4-Pro/Max";
        return 1;
    }
    if (strstr(brand, "M4") != NULL) {
        *out = 4404000000ull;
        *source = "advertised-pcore-max:Apple-M4";
        return 1;
    }
    return 0;
}
#endif

static uint64_t detect_cpu_hz(const char **source)
{
    uint64_t hz = 0;

    if (parse_hz(getenv("BENCH_CPU_HZ"), &hz)) {
        *source = "BENCH_CPU_HZ";
        return hz;
    }
#ifdef __APPLE__
    if (sysctl_u64("hw.cpufrequency_max", &hz)) {
        *source = "sysctl:hw.cpufrequency_max";
        return hz;
    }
    if (sysctl_u64("hw.cpufrequency", &hz)) {
        *source = "sysctl:hw.cpufrequency";
        return hz;
    }
    if (advertised_pcore_hz(&hz, source)) {
        return hz;
    }
#endif
    *source = "unset";
    return 0;
}

static void pin_pcore(void)
{
#ifdef __APPLE__
    (void)pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
}

static uint64_t measure_timer_res_ns(void)
{
    uint64_t min_pos = UINT64_MAX;
    uint64_t prev = now_ns();

    for (int i = 0; i < 200000; i++) {
        const uint64_t now = now_ns();
        const uint64_t delta = now - prev;
        if (delta > 0 && delta < min_pos) {
            min_pos = delta;
        }
        prev = now;
    }
    return min_pos == UINT64_MAX ? 0 : min_pos;
}

void init_counter(void)
{
    if (g_ready) {
        pin_pcore();
        return;
    }

    pin_pcore();
    g_cpu_hz = detect_cpu_hz(&g_hz_source);
    if (g_cpu_hz == 0) {
        fprintf(stderr,
                "fatal: cannot determine P-core frequency. "
                "Set BENCH_CPU_HZ to the performance-core frequency in Hz.\n");
        exit(1);
    }
    g_timer_res_ns = measure_timer_res_ns();
    snprintf(g_notes, sizeof(g_notes),
             "est_cycles;timer=%s;timer_res_ns=%llu;cpu_hz=%llu;hz_source=%s;"
             "qos=user_interactive",
             TIMER_NAME, (unsigned long long)g_timer_res_ns,
             (unsigned long long)g_cpu_hz, g_hz_source);
    fprintf(stderr, "# %s\n", g_notes);
    g_ready = 1;
}

uint64_t get_time_ns(void)
{
    return now_ns();
}

uint64_t ns_to_cycles(uint64_t ns)
{
    return (uint64_t)((__uint128_t)ns * (uint64_t)g_cpu_hz / 1000000000ull);
}

uint64_t get_cycle(void)
{
    return ns_to_cycles(now_ns());
}

uint64_t get_cpu_hz(void)
{
    return g_cpu_hz;
}

uint64_t get_timer_res_ns(void)
{
    return g_timer_res_ns;
}

const char *get_timer_name(void)
{
    return TIMER_NAME;
}

const char *get_cpu_hz_source(void)
{
    return g_hz_source;
}

const char *get_counter_notes(void)
{
    return g_notes;
}

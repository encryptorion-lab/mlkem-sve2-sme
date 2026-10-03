/*
 * Table 1 instruction first-measurement / reciprocal-throughput probes.
 *
 * Same timer as the kernel and full-scheme benches: CLOCK_UPTIME_RAW
 * timestamps, P-core QoS, CE = elapsed_ns * assumed_Hz / 1e9.
 *
 * Arithmetic and permute first-measurement kernels form a destination-
 * dependent chain; memory and ZA first-measurement kernels report issue
 * rates without a dependent consumer. Reciprocal-throughput kernels issue
 * independent destinations or a sequential store stream. SVE2/SME work stays
 * inside one smstart/smstop per sample so the mode switch is amortized
 * across INSTR_INSNS instructions. Mode-switch cost is a separate row.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cycles.h"
#include "instr_params.h"

#ifndef INSTR_NTESTS
#define INSTR_NTESTS 1000
#endif

#ifndef INSTR_WARMUP
#define INSTR_WARMUP 100
#endif

#ifndef INSTR_BUF_BYTES
#define INSTR_BUF_BYTES (64u * 1024u)
#endif

typedef void (*instr_fn)(void *buf);

void neon_lat_add(void *buf);
void neon_tp_add(void *buf);
void neon_lat_sub(void *buf);
void neon_tp_sub(void *buf);
void neon_lat_mul(void *buf);
void neon_tp_mul(void *buf);
void neon_lat_sqrdmulh(void *buf);
void neon_tp_sqrdmulh(void *buf);
void neon_lat_mls(void *buf);
void neon_tp_mls(void *buf);
void neon_lat_smull(void *buf);
void neon_tp_smull(void *buf);
void neon_lat_smlsl(void *buf);
void neon_tp_smlsl(void *buf);
void neon_lat_zip1(void *buf);
void neon_tp_zip1(void *buf);
void neon_lat_trn1(void *buf);
void neon_tp_trn1(void *buf);
void neon_lat_uzp1(void *buf);
void neon_tp_uzp1(void *buf);
void neon_lat_ld1(void *buf);
void neon_tp_ld1(void *buf);
void neon_lat_ld2(void *buf);
void neon_tp_ld2(void *buf);
void neon_lat_st1(void *buf);
void neon_tp_st1(void *buf);

void sve_lat_add(void *buf);
void sve_tp_add(void *buf);
void sve_lat_sub(void *buf);
void sve_tp_sub(void *buf);
void sve_lat_mul(void *buf);
void sve_tp_mul(void *buf);
void sve_lat_sqrdmulh(void *buf);
void sve_tp_sqrdmulh(void *buf);
void sve_lat_mls(void *buf);
void sve_tp_mls(void *buf);
void sve_lat_smullb(void *buf);
void sve_tp_smullb(void *buf);
void sve_lat_smlslb(void *buf);
void sve_tp_smlslb(void *buf);
void sve_lat_zip1(void *buf);
void sve_tp_zip1(void *buf);
void sve_lat_trn1(void *buf);
void sve_tp_trn1(void *buf);
void sve_lat_uzp1(void *buf);
void sve_tp_uzp1(void *buf);
void sve_lat_ld1h(void *buf);
void sve_tp_ld1h(void *buf);
void sve_lat_ld2h(void *buf);
void sve_tp_ld2h(void *buf);
void sve_lat_st1h(void *buf);
void sve_tp_st1h(void *buf);
void sve_lat_mova_h(void *buf);
void sve_tp_mova_h(void *buf);
void sve_lat_mova_v(void *buf);
void sve_tp_mova_v(void *buf);
void sve_mode_switch(void *buf);

struct probe {
    const char *isa;
    const char *kind;
    const char *op;
    const char *mnemonic;
    int lanes;
    uint32_t work_units;
    instr_fn fn;
};

static const struct probe probes[] = {
    {"neon", "lat", "add", "add", 8, INSTR_INSNS, neon_lat_add},
    {"neon", "tp", "add", "add", 8, INSTR_INSNS, neon_tp_add},
    {"sve", "lat", "add", "add", 32, INSTR_INSNS, sve_lat_add},
    {"sve", "tp", "add", "add", 32, INSTR_INSNS, sve_tp_add},

    {"neon", "lat", "sub", "sub", 8, INSTR_INSNS, neon_lat_sub},
    {"neon", "tp", "sub", "sub", 8, INSTR_INSNS, neon_tp_sub},
    {"sve", "lat", "sub", "sub", 32, INSTR_INSNS, sve_lat_sub},
    {"sve", "tp", "sub", "sub", 32, INSTR_INSNS, sve_tp_sub},

    {"neon", "lat", "mul", "mul", 8, INSTR_INSNS, neon_lat_mul},
    {"neon", "tp", "mul", "mul", 8, INSTR_INSNS, neon_tp_mul},
    {"sve", "lat", "mul", "mul", 32, INSTR_INSNS, sve_lat_mul},
    {"sve", "tp", "mul", "mul", 32, INSTR_INSNS, sve_tp_mul},

    {"neon", "lat", "sqrdmulh", "sqrdmulh", 8, INSTR_INSNS, neon_lat_sqrdmulh},
    {"neon", "tp", "sqrdmulh", "sqrdmulh", 8, INSTR_INSNS, neon_tp_sqrdmulh},
    {"sve", "lat", "sqrdmulh", "sqrdmulh", 32, INSTR_INSNS, sve_lat_sqrdmulh},
    {"sve", "tp", "sqrdmulh", "sqrdmulh", 32, INSTR_INSNS, sve_tp_sqrdmulh},

    {"neon", "lat", "mls", "mls", 8, INSTR_INSNS, neon_lat_mls},
    {"neon", "tp", "mls", "mls", 8, INSTR_INSNS, neon_tp_mls},
    {"sve", "lat", "mls", "mls", 32, INSTR_INSNS, sve_lat_mls},
    {"sve", "tp", "mls", "mls", 32, INSTR_INSNS, sve_tp_mls},

    {"neon", "lat", "smull", "smull", 4, INSTR_INSNS, neon_lat_smull},
    {"neon", "tp", "smull", "smull", 4, INSTR_INSNS, neon_tp_smull},
    {"sve", "lat", "smull", "smullb", 16, INSTR_INSNS, sve_lat_smullb},
    {"sve", "tp", "smull", "smullb", 16, INSTR_INSNS, sve_tp_smullb},

    {"neon", "lat", "smlsl", "smlsl", 4, INSTR_INSNS, neon_lat_smlsl},
    {"neon", "tp", "smlsl", "smlsl", 4, INSTR_INSNS, neon_tp_smlsl},
    {"sve", "lat", "smlsl", "smlslb", 16, INSTR_INSNS, sve_lat_smlslb},
    {"sve", "tp", "smlsl", "smlslb", 16, INSTR_INSNS, sve_tp_smlslb},

    {"neon", "lat", "zip", "zip1", 8, INSTR_INSNS, neon_lat_zip1},
    {"neon", "tp", "zip", "zip1", 8, INSTR_INSNS, neon_tp_zip1},
    {"sve", "lat", "zip", "zip1", 32, INSTR_INSNS, sve_lat_zip1},
    {"sve", "tp", "zip", "zip1", 32, INSTR_INSNS, sve_tp_zip1},

    {"neon", "lat", "trn", "trn1", 8, INSTR_INSNS, neon_lat_trn1},
    {"neon", "tp", "trn", "trn1", 8, INSTR_INSNS, neon_tp_trn1},
    {"sve", "lat", "trn", "trn1", 32, INSTR_INSNS, sve_lat_trn1},
    {"sve", "tp", "trn", "trn1", 32, INSTR_INSNS, sve_tp_trn1},

    {"neon", "lat", "uzp", "uzp1", 8, INSTR_INSNS, neon_lat_uzp1},
    {"neon", "tp", "uzp", "uzp1", 8, INSTR_INSNS, neon_tp_uzp1},
    {"sve", "lat", "uzp", "uzp1", 32, INSTR_INSNS, sve_lat_uzp1},
    {"sve", "tp", "uzp", "uzp1", 32, INSTR_INSNS, sve_tp_uzp1},

    {"neon", "lat", "ld1", "ld1", 8, INSTR_INSNS, neon_lat_ld1},
    {"neon", "tp", "ld1", "ld1", 8, INSTR_INSNS, neon_tp_ld1},
    {"sve", "lat", "ld1", "ld1h", 32, INSTR_INSNS, sve_lat_ld1h},
    {"sve", "tp", "ld1", "ld1h", 32, INSTR_INSNS, sve_tp_ld1h},

    {"neon", "lat", "ld2", "ld2", 16, INSTR_INSNS, neon_lat_ld2},
    {"neon", "tp", "ld2", "ld2", 16, INSTR_INSNS, neon_tp_ld2},
    {"sve", "lat", "ld2", "ld2h", 64, INSTR_INSNS, sve_lat_ld2h},
    {"sve", "tp", "ld2", "ld2h", 64, INSTR_INSNS, sve_tp_ld2h},

    {"neon", "lat", "st1", "st1", 8, INSTR_INSNS, neon_lat_st1},
    {"neon", "tp", "st1", "st1", 8, INSTR_INSNS, neon_tp_st1},
    {"sve", "lat", "st1", "st1h", 32, INSTR_INSNS, sve_lat_st1h},
    {"sve", "tp", "st1", "st1h", 32, INSTR_INSNS, sve_tp_st1h},

    {"sve", "lat", "mova_h", "mova h-store", 32, INSTR_INSNS, sve_lat_mova_h},
    {"sve", "tp", "mova_h", "mova h-store", 32, INSTR_INSNS, sve_tp_mova_h},
    {"sve", "lat", "mova_v", "mova v-load", 32, INSTR_INSNS, sve_lat_mova_v},
    {"sve", "tp", "mova_v", "mova v-load", 32, INSTR_INSNS, sve_tp_mova_v},

    {"sve", "tp", "mode_switch", "smstart+smstop", 0, INSTR_REPS, sve_mode_switch},
};

static void *aligned_buf(void)
{
    void *ptr = NULL;
    if (posix_memalign(&ptr, 64, INSTR_BUF_BYTES) != 0 || ptr == NULL) {
        fprintf(stderr, "posix_memalign failed\n");
        exit(1);
    }
    memset(ptr, 0x5a, INSTR_BUF_BYTES);
    return ptr;
}

int main(void)
{
    void *buf = aligned_buf();
    const size_t nprobes = sizeof(probes) / sizeof(probes[0]);

    init_counter();

    for (size_t p = 0; p < nprobes; p++) {
        for (int w = 0; w < INSTR_WARMUP; w++) {
            probes[p].fn(buf);
        }
    }

    printf("isa,kind,op,mnemonic,lanes,work_units,sample,elapsed_ns,est_cycles,"
           "cyc_per_insn,cyc_per_coeff,cpu_hz,timer,hz_source,timer_res_ns\n");

    for (size_t p = 0; p < nprobes; p++) {
        const struct probe *pr = &probes[p];
        for (int i = 0; i < INSTR_NTESTS; i++) {
            const uint64_t t0 = get_time_ns();
            pr->fn(buf);
            const uint64_t t1 = get_time_ns();
            const uint64_t elapsed_ns = t1 - t0;
            const uint64_t est_cycles = ns_to_cycles(elapsed_ns);
            const double cyc_per_insn =
                (double)est_cycles / (double)pr->work_units;
            const double cyc_per_coeff =
                pr->lanes > 0 ? cyc_per_insn / (double)pr->lanes : 0.0;
            printf("%s,%s,%s,%s,%d,%u,%d,%" PRIu64 ",%" PRIu64
                   ",%.8f,%.8f,%" PRIu64 ",%s,%s,%" PRIu64 "\n",
                   pr->isa, pr->kind, pr->op, pr->mnemonic, pr->lanes,
                   pr->work_units, i, elapsed_ns, est_cycles, cyc_per_insn,
                   cyc_per_coeff, get_cpu_hz(), get_timer_name(),
                   get_cpu_hz_source(), get_timer_res_ns());
        }
    }

    free(buf);
    return 0;
}

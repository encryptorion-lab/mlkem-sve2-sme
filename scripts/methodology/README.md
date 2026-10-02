# Independent-run methodology 

This directory measures the **production** Ref / Neon / VecNTT / MatNTT
backends. It is not the S1--S5 ablation suite.

## What is measured

All published estimated cycle-equivalents (CE) come from the same timer:

1. Bracket the work with `CLOCK_UPTIME_RAW` timestamps on Apple
   (`CLOCK_MONOTONIC_RAW` elsewhere). This is the C equivalent of a
   high-resolution `steady_clock`.
2. Request performance-core scheduling with `QOS_CLASS_USER_INTERACTIVE`;
   this is a preference, not core pinning.
3. Record the P-core frequency (environment `BENCH_CPU_HZ`, otherwise
   `hw.cpufrequency_max` / `hw.cpufrequency`, otherwise the advertised
   P-core maximum for this chip).
4. Convert `CE = elapsed_ns × assumed_cpu_hz / 1e9`.

There is no `kperf`, `cntvct_el0`, or PMU path. Root is not required.
The CE values are not retired core cycles, and the assumed 4.51 GHz maximum
frequency is not an instantaneous-frequency measurement. The paper runs
recorded a timer resolution of 41 ns, so absolute short-probe values inherit
timer-resolution and frequency uncertainty. Full-KEM throughput is derived
directly from elapsed time rather than from the CE scale.
`make calibrate` records the same units for the empty-timer overhead, a
scalar loop, a batched `smstart`/`smstop` (100 entries, then divided),
and a streaming-SVE multiply loop. On this M4 Pro the userspace clocks
all step by about 42 ns; short probes are therefore amortized.

## Protocol

- AC power, no concurrent benchmark process.
- 30 independent sequential processes.
- Inside each process, backends are shuffled. Only one binary runs at a time.
- Each process uses the existing `--speed` path: 100 warmup iterations, then
  10,000 timed iterations. Arithmetic kernels keep `ARITH_REPS=100`.
- Kyber and ML-KEM are both rebuilt with `NTESTS=10000` so the two schemes
  use the same iteration count as the manuscript.
- A process reports its own median, $P_{25}$, $P_{75}$, IQR, and standard
  deviation. Those per-process CSVs are the archived raw data.
- The published point estimate is the **median of the 30 process medians**.
  Dispersion is the **IQR of those 30 process medians**. Samples from
  different processes are not pooled.

## Commands

Build the production binaries once:

```sh
cd /path/to/mlkem-sve2-sme
make -C kem/mlkem NTESTS=10000 all
make -C kem/kyber NTESTS=10000 all
make -C scripts/methodology calibrate
make -C scripts/methodology instr
```

Then, on AC power:

```sh
make speed-independent          # all parameter sets, 30 runs
make speed-independent-768      # ML-KEM-768 and Kyber-768 only
make calibrate                  # 30 independent timer calibrations
make instr                      # Table 1 Neon vs SVE2/SME instruction probes
```

Optional: pin the conversion frequency explicitly, for example
`BENCH_CPU_HZ=4510000000` on this M4 Pro.

Outputs normally land in the repository-local `results/methodology/` tree:

```text
results/methodology/
  environment.txt
  timer.log
  raw/block-XX-<scheme>-<backend>-<param>.csv
  per_run.csv
  summary.csv
  calibration/raw/block-XX.csv
  calibration/summary.csv
  instr/raw/block-XX.csv
  instr/summary.csv
  instr/table.csv
```

`summary.csv` is what later replaces the single-run medians in the paper
tables. Do not edit the manuscript until these files exist.

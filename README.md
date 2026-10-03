# Implementing and Measuring ML-KEM on ARMv9-A with SVE2 and SME

This repository contains the artifact for our paper *"Implementing and Measuring ML-KEM on ARMv9-A with SVE2 and SME"*.

**Authors:**

- Hanyu Wei, Fudan University, Shanghai, China
- Wenqian Li, Fudan University, Shanghai, China
- Shiyu Shen, City University of Hong Kong, Hong Kong, China
- Hao Yang, City University of Hong Kong, Hong Kong, China
- Yunlei Zhao, Fudan University, Shanghai, China

This project contains AArch64 implementations of **ML-KEM** (FIPS 203) and **Kyber**, with hand-optimized polynomial arithmetic using **SVE2** (VecNTT) and **SME** (MatNTT). Four implementation families are provided for each scheme:

| Family | Description |
|--------|-------------|
| **ref** | Portable C reference |
| **neon** | liboqs / PQClean AArch64 NEON baseline |
| **sve** | SVE2 streaming-mode NTT |
| **sme** | SME matrix-tile NTT |

SVE and SME replace only NTT, inverse NTT, and base-multiply-accumulate; the rest of the pipeline reuses the imported liboqs NEON backend. Project assembly lives in `kem/mlkem/variants/`.

Benchmarks were measured on **Apple M4 Pro** (macOS, Apple clang).

## Running the Code

From the repository root, build all backends and run correctness checks:

```sh
make all          # ML-KEM + Kyber; ref / neon / sve / sme; 512 / 768 / 1024
make test         # deterministic KeyGen → Encaps → Decaps round-trip
```

Reproduce the paper's independent-run measurements without root privileges:

```sh
make speed-independent             # production tables; 30 independent processes
make -C scripts/methodology instr  # Table 1 instruction probes
make -C experiments all test       # five-stage ablation correctness
make -C experiments speed-768      # five-stage ML-KEM-768 ablation
```

The directly measured quantity is wall-clock elapsed time (`CLOCK_UPTIME_RAW` on Apple). The paper tables report estimated cycle-equivalents (CE), computed as elapsed time multiplied by an assumed 4.51 GHz performance-core frequency. This is a reporting convention, not a hardware core-cycle counter or a measurement of instantaneous frequency. The timer resolution recorded for the paper runs was 41 ns. User-interactive QoS requests performance-core scheduling but does not pin the thread or guarantee the assumed frequency. No benchmark command requires root, `sudo`, `kperf`, or `cntvct_el0`.

Per scheme:

```sh
# ML-KEM (512 / 768 / 1024)
cd kem/mlkem
make all && make test
make speed

# Kyber (512 / 768 / 1024)
cd kem/kyber
make all && make test
make speed
```

**Requirements:** AArch64 with SME (`-march=armv9.2-a+sme+sha3`), Clang or GCC.

`make speed` is a quick single-process check and reports nine operations per binary: KeyGen, Encaps, Decaps, NTT, INVNTT, BaseMulAcc, MatrixVectorMul, InnerProdEnc, and InnerProdDec. Use `make speed-independent` for the paper protocol.

## Benchmark outputs

Quick single-process runs from `make speed` write ML-KEM CSV files to `kem/mlkem/results/speed/` and the merged Kyber table to `kem/kyber/results/kyber-speed.csv`.

The 30-run production-backend measurements used for the paper are available under `results/methodology/`. Aggregated KEM results are in `summary.csv`, per-run statistics are in `per_run.csv`, and individual process outputs are under `raw/`.

The five-stage NTT ablation results reported in the paper are available in `experiments/results/`.

## Repository Layout

```
common/              timing/CE utilities, FIPS-202 (Keccak)
kem/
  mlkem/             ML-KEM harness + SVE/SME variants/
  kyber/             Kyber harness (ref / neon / sve / sme)
scripts/methodology/ independent-run and instruction measurement protocol
results/methodology/ archived 30-run production-backend measurements
experiments/         five-stage forward-NTT ablation and canonical results
third_party/liboqs/  vendored mlkem-native and Kyber sources
```

## License

Project-authored code (notably `kem/mlkem/variants/`, `common/`, and the benchmark harnesses under `kem/`) is released under the [GNU General Public License v3.0](LICENSE). See [NOTICE](NOTICE) for third-party components vendored under `third_party/liboqs/` (mlkem-native, liboqs aarch64 ML-KEM backend, pqcrystals Kyber reference, PQClean Kyber NEON), which retain their original licenses (Apache-2.0, CC0, ISC, MIT, per upstream file).

# ML-KEM on SVE2&SME

This repository contains the artifact for our paper *"Optimized Implementation of ML-KEM on ARMv9-A with SVE2 and SME"*.

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

From the repository root, build all backends, run correctness checks, and (optionally) collect speed numbers:

```sh
make all          # ML-KEM + Kyber; ref / neon / sve / sme; 512 / 768 / 1024
make test         # deterministic KeyGen → Encaps → Decaps round-trip (no root)
sudo make speed   # median CPU cycles -> results/speed/
```

On macOS, **`sudo` is required for `make speed`** (Apple `kperf` cycle counter). Without root the reported cycle counts
are invalid. 

Per scheme:

```sh
# ML-KEM (512 / 768 / 1024)
cd kem/mlkem
make all && make test
sudo make speed

# Kyber (512 / 768 / 1024)
cd kem/kyber
make all && make test
sudo make speed
```

**Requirements:** AArch64 with SME (`-march=armv9.2-a+sme+sha3`), Clang or GCC.

`make speed` reports nine operations per binary: KeyGen, Encaps, Decaps, NTT, INVNTT, BaseMulAcc, MatrixVectorMul, InnerProdEnc, and InnerProdDec. 

## Repository Layout

```
common/              cycle counter, FIPS-202 (Keccak)
kem/
  mlkem/             ML-KEM harness + SVE/SME variants/
  kyber/             Kyber harness (ref / neon / sve / sme)
third_party/liboqs/  vendored mlkem-native and Kyber sources
```

## License

Project-authored code (notably `kem/mlkem/variants/`, `common/`, and the benchmark harnesses under `kem/`) is released under the [GNU General Public License v3.0](LICENSE). See [NOTICE](NOTICE) for third-party components vendored under `third_party/liboqs/` (mlkem-native, liboqs aarch64 ML-KEM backend, pqcrystals Kyber reference, PQClean Kyber NEON), which retain their original licenses (Apache-2.0, CC0, ISC, MIT, per upstream file).

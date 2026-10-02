# ML-KEM SVE2/SME NTT ablation

The production VecNTT and MatNTT sources are the final versions. This
directory copies them and removes or replaces one factor at a time.
Production files are not modified by the generator.

## Stages

- **S1 — baseline.** Production Vec layout with a spill after every layer.
  Each layer touches both registers and memory.
- **S2 — pair-major fused VecNTT.** Frozen predecessor: coefficients stay in
  registers, but layers 4--7 run as `two_reg_ntt_4_layer` on one register pair
  before the next pair starts.
- **S3 — production VecNTT.** Unmodified final Vec kernel: one load, all seven
  layers in registers, layer-major order so all four pairs of a layer occupy
  the out-of-order issue window before the next layer.
- **S4 — grouped MatNTT.** Vertical ``mova`` loads in a group all finish
  before any ``zip``. Zips and butterflies then run from the last loaded
  pair back to the first.
- **S5 — production MatNTT.** Unmodified final Mat kernel.

Layer fusion is evaluated by comparing S1 with the layer-major S3 production
kernel. Scheduling is evaluated within the fused VecNTT pipeline by comparing
pair-major S2 with layer-major S3.

ZA-based layout conversion is S4 compared with S3. Twiddle reuse is part of
that MatNTT table, and its benefit is the smaller table recorded in
`generated/twiddle_bytes.txt`, not an extra kernel that reloads the same
factors.

S1--S3 share the Vec NTT-domain order and the production Vec inverse.
S4--S5 share the Mat order and the production Mat inverse. Inverses are copied
so each binary is self-contained; their timings are not part of the forward-NTT
ablation.

## Build and correctness

```sh
cd experiments
make clean
make all
make test
```

`make test` checks ML-KEM-512/768/1024 and checks that S1--S3 agree with each
other and that S4 agrees with S5. Root is not required.

## Formal performance run

On AC power, with no other benchmark running:

```sh
make speed-768
```

Thirty sequential processes. Inside each process the stage order is shuffled.
Each process uses the same `--speed` path as the module benches: 100 warmup
iterations, then the median of 10,000 timed iterations, with arithmetic
kernels repeated `ARITH_REPS=100` times inside each sample. Results are not
printed between samples. The reported unit is an estimated cycle-equivalent
(CE): wall-clock elapsed time multiplied by an assumed 4.51 GHz P-core
frequency. It is not a hardware core-cycle count. The paper runs recorded a
41 ns timer resolution, and user-interactive QoS does not pin the process or
guarantee that frequency. Outputs are written under `experiments/results/`
when commands are run from the repository root (the local path is `results/`
inside this directory). This is the only results directory intended for Git,
and its committed five-stage data match the paper. Root privileges are not
required.

`make clean` removes build products but preserves the committed measurements.
Use `make clean-results` only when intentionally replacing
`experiments/results/`.

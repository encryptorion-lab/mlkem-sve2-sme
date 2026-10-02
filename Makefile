# Top-level orchestrator for ML-KEM and Kyber open-source benchmarks.
#
#   make all        build every backend (ref / neon / sve / sme)
#   make test       KEM correctness, all backends
#   make speed      estimated CE from wall-clock × assumed P-core frequency

SUBDIRS := kem/mlkem kem/kyber
MAKEFLAGS += --no-print-directory

all test speed:
	@for d in $(SUBDIRS); do \
		echo "==> $$d: $@"; \
		$(MAKE) -C $$d $@ || exit 1; \
	done

# Independent-run protocol for the paper tables. Rebuilds with
# NTESTS=10000, then runs 30 sequential processes. No root.
speed-independent speed-independent-768 speed-independent-512-1024:
	$(MAKE) -C kem/mlkem NTESTS=10000 all
	$(MAKE) -C kem/kyber NTESTS=10000 all
	$(MAKE) -C scripts/methodology $@

speed-independent-mlkem-sve-768:
	rm -f kem/mlkem/build/aarch64_sve-768/bench_mlkem
	$(MAKE) -C kem/mlkem NTESTS=10000 build/aarch64_sve-768/bench_mlkem
	$(MAKE) -C scripts/methodology $@

calibrate:
	$(MAKE) -C scripts/methodology calibrate

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
	$(MAKE) -C scripts/methodology clean

.PHONY: all test speed speed-independent speed-independent-768 \
	speed-independent-512-1024 speed-independent-mlkem-sve-768 calibrate clean

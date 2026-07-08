# Top-level orchestrator for ML-KEM and Kyber open-source benchmarks.
#
#   make all        build every backend (ref / neon / sve / sme)
#   make test       KEM correctness, all backends
#   sudo make speed cycle benchmarks (root on macOS for kperf)

SUBDIRS := kem/mlkem kem/kyber
MAKEFLAGS += --no-print-directory

all test speed:
	@for d in $(SUBDIRS); do \
		echo "==> $$d: $@"; \
		$(MAKE) -C $$d $@ || exit 1; \
	done

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

.PHONY: all test speed clean

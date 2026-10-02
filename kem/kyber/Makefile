# Thin orchestrator for the legacy-Kyber backends.
#
# The four Kyber backends come from different upstreams with incompatible APIs,
# drivers and colliding headers, so each keeps its own Makefile/driver in a
# subdirectory here (ref/, neon/, sve/, sme/). This orchestrator does NOT rebuild
# any of that logic; it only invokes each sub-build and then merges the
# per-backend results/speed CSVs into one ref|neon|sve|sme table.
#
# Usage:
#   make all            # build all four backends (512/768/1024)
#   make test           # KEM correctness for all four
#   make speed          # benchmark all four + write the combined table
#   make table          # just re-merge existing CSVs (no rerun)

SUBDIRS := ref neon sve sme
MAKEFLAGS += --no-print-directory

all:
	@for d in $(SUBDIRS); do echo "==> build $$d"; $(MAKE) -C $$d all || exit 1; done

test:
	@for d in $(SUBDIRS); do echo "==> test $$d"; $(MAKE) -C $$d test || exit 1; done

speed:
	@for d in $(SUBDIRS); do echo "==> speed $$d"; $(MAKE) -C $$d speed || exit 1; done
	@$(MAKE) table

# Merge whatever results/speed CSVs currently exist into one comparison table.
table:
	@mkdir -p results
	@python3 merge_speed.py

clean:
	rm -rf results

.PHONY: all test speed table clean

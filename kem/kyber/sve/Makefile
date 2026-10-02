# Legacy Kyber KEM with the NTT / INVNTT / base-multiplication routed through
# the project SVE (streaming-mode) assembly. The vendored PQClean aarch64 NEON
# sources are reused for everything else; indcpa.c is replaced by the patched
# copy in patched/kyber$(P)/ (NTT+basemul call sites swapped to project asm).
#
# Two include worlds collide (PQClean ntt.h vs project ntt.h), so the project
# arithmetic translation units are compiled to objects separately from the
# Kyber/driver translation units, then linked.

ROOT := ../../..
COMMON := $(ROOT)/common
KYBERDIR := $(ROOT)/third_party/liboqs/src/kem/kyber
SVE := ../../mlkem/variants/sve
SHARED := ../../mlkem/variants/shared
BUILD := build

CC ?= /usr/bin/cc
PARAMSETS := 512 768 1024
NTESTS ?= 1000
WARMUP ?= 100
ARITH_REPS ?= 100

# +sme: project NTT/INVNTT/basemul run in SME streaming mode. +sha3: NEON
# Keccak (feat.S). One arch flag covers both worlds.
ARCH := -march=armv9.2-a+sme+sha3
OPT := -O3 -fomit-frame-pointer -fno-vectorize

VARIANT := kyber_sve
BACKEND_NTT := $(SVE)/ntt.c
BACKEND_ASM := $(SVE)/__asm_sve_ntt.S $(SVE)/__asm_sve_invntt.S \
	$(SHARED)/__asm_asymmetric_pwm.S

impl_dir = $(KYBERDIR)/oldpqclean_kyber$(1)_aarch64

# Kyber sources reused verbatim (indcpa.c excluded -> patched copy instead).
define kyber_srcs
$(call impl_dir,$(1))/kem.c \
$(call impl_dir,$(1))/polyvec.c $(call impl_dir,$(1))/neon_polyvec.c \
$(call impl_dir,$(1))/poly.c $(call impl_dir,$(1))/neon_poly.c \
$(call impl_dir,$(1))/cbd.c $(call impl_dir,$(1))/reduce.c \
$(call impl_dir,$(1))/verify.c $(call impl_dir,$(1))/rejsample.c \
$(call impl_dir,$(1))/ntt.c $(call impl_dir,$(1))/neon_symmetric-shake.c \
$(call impl_dir,$(1))/fips202x2.c \
$(call impl_dir,$(1))/__asm_NTT.S $(call impl_dir,$(1))/__asm_iNTT.S \
$(call impl_dir,$(1))/__asm_base_mul.S $(call impl_dir,$(1))/__asm_poly.S \
$(call impl_dir,$(1))/feat.S
endef

# Compile flags for the two worlds.
kyber_cflags = $(OPT) $(ARCH) -std=c11 -Wall -Wextra -Wno-unused-parameter \
	-Wno-unused-function -Wno-unused-const-variable \
	-Wno-incompatible-pointer-types \
	-Iglue -I$(call impl_dir,$(1)) -I$(COMMON) \
	-DBENCH_VARIANT=\"$(VARIANT)\" -DBENCH_PARAMSET=\"Kyber-$(1)\" \
	-DNTESTS=$(NTESTS) -DBENCH_WARMUP=$(WARMUP) -DARITH_REPS=$(ARITH_REPS)

proj_cflags = $(OPT) $(ARCH) -I$(SHARED) -I$(SVE) -I$(COMMON) \
	-DMLK_CONFIG_PARAMETER_SET=$(1)

# Per-paramset link: bundle everything via a single cc call per world is not
# possible (header clash), so use a recursive sub-build dir of objects.
define build_rule
$(BUILD)/$(VARIANT)-$(1)/bench: \
		patched/kyber$(1)/indcpa.c bench_kyber_arith.c \
		glue/symmetric-shake.c $(COMMON)/fips202.c $(COMMON)/cycles.c \
		$(call kyber_srcs,$(1)) $(BACKEND_NTT) $(BACKEND_ASM)
	@mkdir -p $$(@D)/k $$(@D)/p
	@# Kyber + driver + patched indcpa (PQClean include world)
	$$(CC) $(call kyber_cflags,$(1)) -c patched/kyber$(1)/indcpa.c -o $$(@D)/k/indcpa.o
	$$(CC) $(call kyber_cflags,$(1)) -c bench_kyber_arith.c -o $$(@D)/k/bench.o
	$$(CC) $(call kyber_cflags,$(1)) -c glue/symmetric-shake.c -o $$(@D)/k/glue.o
	$$(CC) $(call kyber_cflags,$(1)) -c $(COMMON)/fips202.c -o $$(@D)/k/fips202.o
	$$(CC) $(call kyber_cflags,$(1)) -c $(COMMON)/cycles.c -o $$(@D)/k/cycles.o
	@n=0; for s in $(call kyber_srcs,$(1)); do \
		$$(CC) $(call kyber_cflags,$(1)) -c $$$$s -o $$(@D)/k/src$$$$n.o || exit 1; \
		n=$$$$((n+1)); done
	@# Project arithmetic (project include world)
	$$(CC) $(call proj_cflags,$(1)) -c $(BACKEND_NTT) -o $$(@D)/p/ntt.o
	@n=0; for s in $(BACKEND_ASM); do \
		$$(CC) $(call proj_cflags,$(1)) -c $$$$s -o $$(@D)/p/asm$$$$n.o || exit 1; \
		n=$$$$((n+1)); done
	$$(CC) $(ARCH) $$(@D)/k/*.o $$(@D)/p/*.o -o $$@
endef

$(foreach p,$(PARAMSETS),$(eval $(call build_rule,$(p))))

BENCHES := $(foreach p,$(PARAMSETS),$(BUILD)/$(VARIANT)-$(p)/bench)

all: $(BENCHES)

test: all
	@for exe in $(BENCHES); do $$exe --test; done

speed: all
	@mkdir -p results/speed
	@for exe in $(BENCHES); do \
		name=$$(basename $$(dirname $$exe)); \
		$$exe --speed > results/speed/$$name.csv; done

clean:
	rm -rf $(BUILD)

.PHONY: all test speed clean

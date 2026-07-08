# Legacy Kyber KEM with NTT / INVNTT / base-multiplication routed through the
# project SME (MatNTT, ZA-tile) assembly. Identical structure to ../sve;
# the patched indcpa.c, driver and glue are shared from there (the project asm
# symbol names are backend-independent). Only the linked NTT kernels and zeta
# tables differ (variants/sme instead of variants/sve).

ROOT := ../../..
COMMON := $(ROOT)/common
KYBERDIR := $(ROOT)/third_party/liboqs/src/kem/kyber
SVEEXP := ../sve
SME := ../../mlkem/variants/sme
SHARED := ../../mlkem/variants/shared
BUILD := build

CC ?= /usr/bin/cc
PARAMSETS := 512 768 1024
NTESTS ?= 1000
WARMUP ?= 100
ARITH_REPS ?= 100

ARCH := -march=armv9.2-a+sme+sha3
OPT := -O3 -fomit-frame-pointer -fno-vectorize

VARIANT := kyber_sme
BACKEND_NTT := $(SME)/ntt.c
BACKEND_ASM := $(SME)/__asm_sme_ntt.S $(SME)/__asm_sme_invntt.S \
	$(SHARED)/__asm_asymmetric_pwm.S

impl_dir = $(KYBERDIR)/oldpqclean_kyber$(1)_aarch64

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

kyber_cflags = $(OPT) $(ARCH) -std=c11 -Wall -Wextra -Wno-unused-parameter \
	-Wno-unused-function -Wno-unused-const-variable \
	-Wno-incompatible-pointer-types \
	-I$(SVEEXP)/glue -I$(call impl_dir,$(1)) -I$(COMMON) \
	-DBENCH_VARIANT=\"$(VARIANT)\" -DBENCH_PARAMSET=\"Kyber-$(1)\" \
	-DNTESTS=$(NTESTS) -DBENCH_WARMUP=$(WARMUP) -DARITH_REPS=$(ARITH_REPS)

proj_cflags = $(OPT) $(ARCH) -I$(SHARED) -I$(SME) -I$(COMMON) \
	-DMLK_CONFIG_PARAMETER_SET=$(1)

define build_rule
$(BUILD)/$(VARIANT)-$(1)/bench: \
		$(SVEEXP)/patched/kyber$(1)/indcpa.c $(SVEEXP)/bench_kyber_arith.c \
		$(SVEEXP)/glue/symmetric-shake.c $(COMMON)/fips202.c $(COMMON)/cycles.c \
		$(call kyber_srcs,$(1)) $(BACKEND_NTT) $(BACKEND_ASM)
	@mkdir -p $$(@D)/k $$(@D)/p
	$$(CC) $(call kyber_cflags,$(1)) -c $(SVEEXP)/patched/kyber$(1)/indcpa.c -o $$(@D)/k/indcpa.o
	$$(CC) $(call kyber_cflags,$(1)) -c $(SVEEXP)/bench_kyber_arith.c -o $$(@D)/k/bench.o
	$$(CC) $(call kyber_cflags,$(1)) -c $(SVEEXP)/glue/symmetric-shake.c -o $$(@D)/k/glue.o
	$$(CC) $(call kyber_cflags,$(1)) -c $(COMMON)/fips202.c -o $$(@D)/k/fips202.o
	$$(CC) $(call kyber_cflags,$(1)) -c $(COMMON)/cycles.c -o $$(@D)/k/cycles.o
	@n=0; for s in $(call kyber_srcs,$(1)); do \
		$$(CC) $(call kyber_cflags,$(1)) -c $$$$s -o $$(@D)/k/src$$$$n.o || exit 1; \
		n=$$$$((n+1)); done
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
		sudo $$exe --speed > results/speed/$$name.csv; done

clean:
	rm -rf $(BUILD)

.PHONY: all test speed clean

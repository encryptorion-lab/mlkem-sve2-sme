ROOT := ../../..
COMMON := $(ROOT)/common
KYBERDIR := $(ROOT)/third_party/liboqs/src/kem/kyber
BUILD := build

CC ?= /usr/bin/cc
PARAMSETS := 512 768 1024
NTESTS ?= 1000
WARMUP ?= 100
ARITH_REPS ?= 100

# NEON Keccak (feat.S / fips202x2) uses the Armv8.2 SHA3 extension (eor3/rax1/
# xar/bcax), available on Apple M-series. -Wno-incompatible-pointer-types: the
# upstream symmetric.h types the XOF state as shake128incctx while common's
# one-shot shake128_squeezeblocks takes the layout-identical shake128ctx (see
# glue/symmetric-shake.c, shared with kyber-ref).
CFLAGS := -std=c11 -O3 -Wall -Wextra -Wno-unused-parameter \
	-Wno-unused-function -Wno-unused-const-variable \
	-Wno-incompatible-pointer-types -fomit-frame-pointer \
	-march=armv8.2-a+sha3

# Per-directory upstream sources (KYBER_K is baked into each dir's params.h).
# symmetric-shake.c is excluded; glue/ reimplements its two helpers on the
# one-shot common/fips202 API.
define impl_dir
$(KYBERDIR)/oldpqclean_kyber$(1)_aarch64
endef

define upstream_srcs
$(call impl_dir,$(1))/kem.c $(call impl_dir,$(1))/indcpa.c \
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

GLUE_SRCS := glue/symmetric-shake.c
COMMON_SRCS := $(COMMON)/fips202.c $(COMMON)/cycles.c

define bench_target
$(BUILD)/kyber_neon-$(1)/bench_kyber_neon
endef

define build_rule
$(call bench_target,$(1)): bench_kyber_neon.c $$(GLUE_SRCS) $$(COMMON_SRCS) $(call upstream_srcs,$(1))
	@mkdir -p $$(@D)
	$$(CC) $$(CFLAGS) -Iglue -I$(call impl_dir,$(1)) -I$(COMMON) \
		-DBENCH_VARIANT=\"kyber_neon\" -DBENCH_PARAMSET=\"Kyber-$(1)\" \
		-DNTESTS=$$(NTESTS) -DBENCH_WARMUP=$$(WARMUP) -DARITH_REPS=$$(ARITH_REPS) \
		$(call upstream_srcs,$(1)) $$(GLUE_SRCS) $$(COMMON_SRCS) \
		bench_kyber_neon.c -o $$@
endef

$(foreach p,$(PARAMSETS),$(eval $(call build_rule,$(p))))

BENCHES := $(foreach p,$(PARAMSETS),$(call bench_target,$(p)))

all: $(BENCHES)

test: all
	@for exe in $(BENCHES); do $$exe --test; done

speed: all
	@mkdir -p results/speed
	@for exe in $(BENCHES); do name=$$(basename $$(dirname $$exe)); sudo $$exe --speed > results/speed/$$name.csv; done

clean:
	rm -rf $(BUILD) results

.PHONY: all test speed clean

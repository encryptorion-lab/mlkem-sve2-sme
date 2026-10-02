ROOT := ../..
LIBOQS_MLKEM := $(ROOT)/third_party/liboqs/src/kem/ml_kem
COMMON := $(ROOT)/common
VARIANTS := variants
BUILD := build

CC ?= /usr/bin/cc
PARAMSETS := 512 768 1024
# Implementation families, all peers:
#   ref         -> mlkem-native_ml-kem-{param}_ref       (portable C)
#   aarch64     -> mlkem-native_ml-kem-{param}_aarch64   (liboqs NEON backend)
#   aarch64_sve -> liboqs aarch64 backend, NTT/INTT/basemul replaced by variants/sve
#   aarch64_sme -> liboqs aarch64 backend, NTT/INTT/basemul replaced by variants/sme
IMPLS := ref aarch64 aarch64_sve aarch64_sme
NTESTS ?= 10000
WARMUP ?= 100
ARITH_REPS ?= 100

BASE_CFLAGS := -std=c11 -O3 -Wall -Wextra -Wno-unused-parameter \
	-Wno-unused-function -fomit-frame-pointer -Iconfig -I$(COMMON)

MLKEM_C_SRCS = mlkem/src/compress.c mlkem/src/debug.c mlkem/src/indcpa.c \
	mlkem/src/kem.c mlkem/src/poly.c mlkem/src/poly_k.c \
	mlkem/src/sampling.c mlkem/src/verify.c

# liboqs aarch64 (NEON) helpers reused by every aarch64-family backend.
AARCH64_SHARED_SRCS = mlkem/src/native/aarch64/src/aarch64_zetas.c \
	mlkem/src/native/aarch64/src/poly_mulcache_compute_asm.S \
	mlkem/src/native/aarch64/src/poly_reduce_asm.S \
	mlkem/src/native/aarch64/src/poly_tobytes_asm.S \
	mlkem/src/native/aarch64/src/poly_tomont_asm.S \
	mlkem/src/native/aarch64/src/rej_uniform_asm.S \
	mlkem/src/native/aarch64/src/rej_uniform_table.c

# Pure NEON backend additionally uses liboqs' own NTT/INTT/basemul.
AARCH64_BASELINE_SRCS = $(AARCH64_SHARED_SRCS) \
	mlkem/src/native/aarch64/src/intt.S \
	mlkem/src/native/aarch64/src/ntt.S \
	mlkem/src/native/aarch64/src/polyvec_basemul_acc_montgomery_cached_asm_k2.S \
	mlkem/src/native/aarch64/src/polyvec_basemul_acc_montgomery_cached_asm_k3.S \
	mlkem/src/native/aarch64/src/polyvec_basemul_acc_montgomery_cached_asm_k4.S

# Project SVE/SME arithmetic (parameter-independent, one canonical copy).
# __asm_asymmetric_pwm.S is shared between SVE and SME.
SVE_SRCS = $(VARIANTS)/sve/ntt.c \
	$(VARIANTS)/sve/__asm_sve_ntt.S $(VARIANTS)/sve/__asm_sve_invntt.S \
	$(VARIANTS)/shared/__asm_asymmetric_pwm.S

SME_SRCS = $(VARIANTS)/sme/ntt.c \
	$(VARIANTS)/sme/__asm_sme_ntt.S $(VARIANTS)/sme/__asm_sme_invntt.S \
	$(VARIANTS)/shared/__asm_asymmetric_pwm.S

# Every implementation that uses the aarch64 import lives under the _aarch64
# tree (the _ref tree is used only by the portable ref build).
define variant_dir
$(LIBOQS_MLKEM)/mlkem-native_ml-kem-$(1)_$(if $(filter $(2),ref),ref,aarch64)
endef

define variant_config
$(if $(filter $(2),ref),mlkem_config_ref.h,$(if $(filter $(2),aarch64),mlkem_config_aarch64.h,mlkem_config_$(2).h))
endef

define variant_extra_sources
$(if $(filter $(2),aarch64),$(addprefix $(call variant_dir,$(1),$(2))/,$(AARCH64_BASELINE_SRCS)),) \
$(if $(filter $(2),aarch64_sve),$(addprefix $(call variant_dir,$(1),$(2))/,$(AARCH64_SHARED_SRCS)) $(SVE_SRCS),) \
$(if $(filter $(2),aarch64_sme),$(addprefix $(call variant_dir,$(1),$(2))/,$(AARCH64_SHARED_SRCS)) $(SME_SRCS),)
endef

define variant_extra_includes
$(if $(filter $(2),aarch64_sve),-I$(VARIANTS)/sve -I$(VARIANTS)/shared,) \
$(if $(filter $(2),aarch64_sme),-I$(VARIANTS)/sme -I$(VARIANTS)/shared,)
endef

define variant_extra_flags
$(if $(filter $(2),aarch64_sve aarch64_sme),-mtune=native -march=armv9.2-a+sme -fno-vectorize,)
endef

# CSV/log variant tag (mlkem_ref, mlkem_neon, ...) matching kyber_ref, kyber_neon, ...
define bench_variant_name
$(if $(filter $(2),ref),mlkem_ref,$(if $(filter $(2),aarch64),mlkem_neon,$(if $(filter $(2),aarch64_sve),mlkem_sve,mlkem_sme)))
endef

define bench_target
$(BUILD)/$(2)-$(1)/bench_mlkem
endef

define build_rule
$(call bench_target,$(1),$(2)): bench_mlkem.c $$(COMMON)/fips202.c $$(COMMON)/cycles.c \
	$$(call variant_extra_sources,$(1),$(2))
	@mkdir -p $$(@D)
	$$(CC) $$(BASE_CFLAGS) -I$$(call variant_dir,$(1),$(2))/mlkem/src \
		-I$$(call variant_dir,$(1),$(2)) $$(call variant_extra_includes,$(1),$(2)) \
		$$(call variant_extra_flags,$(1),$(2)) \
		-DMLK_CONFIG_PARAMETER_SET=$(1) \
		-DMLK_CONFIG_FILE=\"$$(call variant_config,$(1),$(2))\" \
		-DBENCH_VARIANT=\"$(call bench_variant_name,$(1),$(2))\" \
		-DBENCH_PARAMSET=\"ML-KEM-$(1)\" \
		-DNTESTS=$$(NTESTS) -DBENCH_WARMUP=$$(WARMUP) \
		-DARITH_REPS=$$(ARITH_REPS) \
		$$(addprefix $$(call variant_dir,$(1),$(2))/,$$(MLKEM_C_SRCS)) \
		$$(call variant_extra_sources,$(1),$(2)) \
		$$(COMMON)/fips202.c $$(COMMON)/cycles.c bench_mlkem.c -o $$@
endef

$(foreach p,$(PARAMSETS),$(foreach v,$(IMPLS),$(eval $(call build_rule,$(p),$(v)))))

BENCHES := $(foreach p,$(PARAMSETS),$(foreach v,$(IMPLS),$(call bench_target,$(p),$(v))))

all: $(BENCHES)

test: all
	@set -e; \
	for tag_impl in ref:ref neon:aarch64 sve:aarch64_sve sme:aarch64_sme; do \
		tag=$${tag_impl%%:*}; impl=$${tag_impl#*:}; \
		echo "==> test $$tag"; \
		for p in $(PARAMSETS); do \
			$(BUILD)/$$impl-$$p/bench_mlkem --test; \
		done; \
	done

speed: all
	@mkdir -p results/speed
	@for exe in $(BENCHES); do name=$$(basename $$(dirname $$exe)); $$exe --speed > results/speed/$$name.csv; done

clean:
	rm -rf $(BUILD) results

.PHONY: all test speed clean

ROOT := ..
MLKEM := $(ROOT)/kem/mlkem
LIBOQS_MLKEM := $(ROOT)/third_party/liboqs/src/kem/ml_kem
COMMON := $(ROOT)/common
BUILD := build
GENERATED := generated
GENERATED_STAMP := $(GENERATED)/.stamp

CC ?= /usr/bin/cc
STAGES := 1 2 3 4 5
PARAMSETS := 512 768 1024
NTESTS ?= 10000
WARMUP ?= 100
ARITH_REPS ?= 100

BASE_CFLAGS := -std=c11 -O3 -Wall -Wextra -Wno-unused-parameter \
	-Wno-unused-function -fomit-frame-pointer -I$(MLKEM)/config -I$(COMMON) \
	-mtune=native -march=armv9.2-a+sme -fno-vectorize

MLKEM_C_SRCS = mlkem/src/compress.c mlkem/src/debug.c mlkem/src/indcpa.c \
	mlkem/src/kem.c mlkem/src/poly.c mlkem/src/poly_k.c \
	mlkem/src/sampling.c mlkem/src/verify.c

AARCH64_SHARED_SRCS = mlkem/src/native/aarch64/src/aarch64_zetas.c \
	mlkem/src/native/aarch64/src/poly_mulcache_compute_asm.S \
	mlkem/src/native/aarch64/src/poly_reduce_asm.S \
	mlkem/src/native/aarch64/src/poly_tobytes_asm.S \
	mlkem/src/native/aarch64/src/poly_tomont_asm.S \
	mlkem/src/native/aarch64/src/rej_uniform_asm.S \
	mlkem/src/native/aarch64/src/rej_uniform_table.c

define variant_dir
$(LIBOQS_MLKEM)/mlkem-native_ml-kem-$(1)_aarch64
endef

define stage_family
$(if $(filter $(1),1 2 3),sve,sme)
endef

define stage_config
mlkem_config_aarch64_$(call stage_family,$(1)).h
endef

define bench_target
$(BUILD)/s$(1)-$(2)/bench_ablation
endef

define build_rule
$(call bench_target,$(1),$(2)): $(GENERATED_STAMP) bench_ablation.c
	@mkdir -p $$(@D)
	$$(CC) $$(BASE_CFLAGS) \
		-I$$(call variant_dir,$(2))/mlkem/src \
		-I$$(call variant_dir,$(2)) \
		-I$$(MLKEM)/variants/$$(call stage_family,$(1)) \
		-I$$(MLKEM)/variants/shared \
		-DMLK_CONFIG_PARAMETER_SET=$(2) \
		-DMLK_CONFIG_FILE=\"$$(call stage_config,$(1))\" \
		-DBENCH_VARIANT=\"mlkem_ablation_s$(1)\" \
		-DBENCH_PARAMSET=\"ML-KEM-$(2)\" \
		-DABLATION_STAGE=$(1) -DNTESTS=$$(NTESTS) \
		-DBENCH_WARMUP=$$(WARMUP) -DARITH_REPS=$$(ARITH_REPS) \
		$$(addprefix $$(call variant_dir,$(2))/,$$(MLKEM_C_SRCS)) \
		$$(addprefix $$(call variant_dir,$(2))/,$$(AARCH64_SHARED_SRCS)) \
		$$(GENERATED)/s$(1)/ntt.c \
		$$(GENERATED)/s$(1)/ntt.S \
		$$(GENERATED)/s$(1)/invntt.S \
		$$(MLKEM)/variants/shared/__asm_asymmetric_pwm.S \
		$$(COMMON)/fips202.c $$(COMMON)/cycles.c bench_ablation.c \
		-o $$@
endef

$(foreach s,$(STAGES),$(foreach p,$(PARAMSETS),\
	$(eval $(call build_rule,$(s),$(p)))))

BENCHES := $(foreach s,$(STAGES),$(foreach p,$(PARAMSETS),\
	$(call bench_target,$(s),$(p))))
BENCHES_768 := $(foreach s,$(STAGES),$(call bench_target,$(s),768))

$(GENERATED_STAMP): generate_variants.py \
	baselines/pair_major_ntt.S \
	$(MLKEM)/variants/sve/__asm_sve_ntt.S \
	$(MLKEM)/variants/sve/__asm_sve_invntt.S \
	$(MLKEM)/variants/sve/ntt.c \
	$(MLKEM)/variants/sme/__asm_sme_ntt.S \
	$(MLKEM)/variants/sme/__asm_sme_invntt.S \
	$(MLKEM)/variants/sme/ntt.c
	python3 generate_variants.py
	@touch $@

generate: $(GENERATED_STAMP)

all: $(BENCHES)

test: all
	@set -e; \
	for s in $(STAGES); do \
		for p in $(PARAMSETS); do \
			echo "==> S$$s ML-KEM-$$p"; \
			$(BUILD)/s$$s-$$p/bench_ablation --test; \
		done; \
	done; \
	for p in $(PARAMSETS); do \
		python3 scripts/verify_layout_groups.py \
			--build-dir $(BUILD) --paramset $$p; \
	done

# Formal timing is intentionally not part of all/test.  Run only after the
# machine is prepared as described in README.md.
speed-768: $(BENCHES_768)
	python3 scripts/run_independent.py \
		--build-dir $(BUILD) --paramset 768 --runs 30

static: $(BENCHES_768)
	python3 scripts/static_counts.py --build-dir $(BUILD) --paramset 768

clean:
	rm -rf $(BUILD)

clean-results:
	rm -rf results results-local

# The production-table protocol lives in scripts/methodology, not here.
calibrate speed-independent speed-independent-768:
	$(MAKE) -C $(ROOT) $@

.PHONY: generate all test speed-768 static clean clean-results \
	calibrate speed-independent speed-independent-768

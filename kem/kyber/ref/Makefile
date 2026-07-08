ROOT := ../../..
COMMON := $(ROOT)/common
# Pristine pqcrystals Kyber (round 3) reference imported from liboqs.
REFDIR := $(ROOT)/third_party/liboqs/src/kem/kyber/pqcrystals-kyber_kyber768_ref
BUILD := build

CC ?= /usr/bin/cc
PARAMSETS := 512 768 1024
NTESTS ?= 1000
WARMUP ?= 100
ARITH_REPS ?= 100

# -Wno-incompatible-pointer-types: the upstream symmetric.h types the XOF state
# as shake128incctx while common's one-shot shake128_squeezeblocks takes the
# layout-identical shake128ctx (see glue/symmetric-shake.c).
CFLAGS := -std=c11 -O3 -Wall -Wextra -Wno-unused-parameter \
	-Wno-unused-function -Wno-incompatible-pointer-types -fomit-frame-pointer

# Upstream reference sources. symmetric-shake.c is replaced by glue/ (the
# upstream file targets the PQClean incremental FIPS202 API; common/ ships a
# one-shot API), so it is intentionally excluded here.
REF_SRCS = $(REFDIR)/kem.c $(REFDIR)/indcpa.c $(REFDIR)/polyvec.c \
	$(REFDIR)/poly.c $(REFDIR)/ntt.c $(REFDIR)/cbd.c $(REFDIR)/reduce.c \
	$(REFDIR)/verify.c

GLUE_SRCS = glue/symmetric-shake.c
COMMON_SRCS = $(COMMON)/fips202.c $(COMMON)/cycles.c

# 512 -> KYBER_K=2, 768 -> 3, 1024 -> 4
define kyber_k
$(if $(filter $(1),512),2,$(if $(filter $(1),768),3,4))
endef

define bench_target
$(BUILD)/kyber_ref-$(1)/bench_kyber
endef

define build_rule
$(call bench_target,$(1)): bench_kyber.c $$(GLUE_SRCS) $$(REF_SRCS) $$(COMMON_SRCS)
	@mkdir -p $$(@D)
	$$(CC) $$(CFLAGS) -Iglue -I$$(REFDIR) -I$$(COMMON) \
		-DKYBER_K=$(call kyber_k,$(1)) \
		-DBENCH_VARIANT=\"kyber_ref\" -DBENCH_PARAMSET=\"Kyber-$(1)\" \
		-DNTESTS=$$(NTESTS) -DBENCH_WARMUP=$$(WARMUP) \
		-DARITH_REPS=$$(ARITH_REPS) \
		$$(REF_SRCS) $$(GLUE_SRCS) $$(COMMON_SRCS) bench_kyber.c -o $$@
endef

$(foreach p,$(PARAMSETS),$(eval $(call build_rule,$(p))))

BENCHES := $(foreach p,$(PARAMSETS),$(call bench_target,$(p)))

all: $(BENCHES)

test: all
	@for exe in $(BENCHES); do $$exe --test; done

speed: all
	@mkdir -p results/speed
	@for exe in $(BENCHES); do name=$$(basename $$(dirname $$exe)); $$exe --speed > results/speed/$$name.csv; done

clean:
	rm -rf $(BUILD) results

.PHONY: all test speed clean

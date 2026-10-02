ROOT := ../..
COMMON := $(ROOT)/common
BUILD := build
CC ?= /usr/bin/cc

calibrate: $(BUILD)/calibrate_timers
	python3 run_calibration.py --binary $(BUILD)/calibrate_timers --runs 30

$(BUILD)/calibrate_timers: calibrate_timers.c calibrate_sme.S $(COMMON)/cycles.c
	@mkdir -p $(@D)
	$(CC) -std=c11 -O3 -fomit-frame-pointer -march=armv9.2-a+sme \
		-I$(COMMON) calibrate_timers.c calibrate_sme.S $(COMMON)/cycles.c -o $@

instr: $(BUILD)/instr_bench
	python3 run_instr.py --binary $(BUILD)/instr_bench --runs 30

instr-smoke: $(BUILD)/instr_bench
	python3 run_instr.py --binary $(BUILD)/instr_bench --runs 1

$(BUILD)/instr_bench: instr_bench.c instr_bench.S instr_params.h $(COMMON)/cycles.c
	@mkdir -p $(@D)
	$(CC) -std=c11 -O3 -fomit-frame-pointer -march=armv9.2-a+sme \
		-I$(COMMON) -I. instr_bench.c instr_bench.S $(COMMON)/cycles.c -o $@

speed-independent:
	python3 run_independent.py --runs 30

speed-independent-768:
	python3 run_independent.py --runs 30 --paramset 768

speed-independent-512-1024:
	python3 run_independent.py --runs 30 --paramset 512 --paramset 1024

speed-independent-mlkem-sve-768:
	python3 run_independent.py --runs 30 --paramset 768 --scheme mlkem --backend sve

clean:
	rm -rf $(BUILD)

# Previous sudo kperf runs left results/ owned by root. This target
# only prints the reclaim command; it does not prompt for a password.
fix-results-perms:
	@echo "sudo chown -R \"$$(whoami)\" \"$(ROOT)/results\""

.PHONY: calibrate instr instr-smoke speed-independent speed-independent-768 \
	speed-independent-512-1024 speed-independent-mlkem-sve-768 \
	clean fix-results-perms

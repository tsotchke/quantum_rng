# Compiler and flags
CC = gcc

# AGGRESSIVE OPTIMIZATION (with numerical stability fixes)
# -Ofast: Maximum optimization
# -flto: Link-time optimization
# -ffast-math: Fast FP math (gate code is written to tolerate this)
# -funroll-loops: Loop unrolling
CFLAGS = -Wall -Wextra -Ofast -flto -ffast-math -funroll-loops -fPIC -I.

# CPU-specific tuning is opt-in so the default build stays portable across
# machines and CI. Enable it for a local performance build with `make NATIVE=1`.
ifdef NATIVE
CFLAGS += -march=native
endif

# M2 ULTRA PARALLELIZATION - Phase 1: OpenMP 24-Core Support
OPENMP_FLAGS =
OPENMP_LIBS =

# Detect OpenMP support and configure appropriately
ifeq ($(shell uname),Darwin)
    # macOS: Check if we're using clang (default) or gcc
    CC_VERSION := $(shell $(CC) --version 2>/dev/null | head -n 1)
    
    # If using clang (default on macOS), use Xcode's OpenMP flags
    ifneq ($(findstring clang,$(CC_VERSION)),)
        # Clang on macOS: Use Homebrew libomp
        LIBOMP_PREFIX := $(shell brew --prefix libomp 2>/dev/null)
        ifneq ($(LIBOMP_PREFIX),)
            OPENMP_FLAGS = -Xpreprocessor -fopenmp -I$(LIBOMP_PREFIX)/include
            OPENMP_LIBS = -L$(LIBOMP_PREFIX)/lib -lomp
            CFLAGS += $(OPENMP_FLAGS)
        endif
    else
        # Using actual GCC: standard OpenMP flags
        OPENMP_FLAGS = -fopenmp
        OPENMP_LIBS = -fopenmp
        CFLAGS += $(OPENMP_FLAGS)
    endif
else
    # Linux/other: standard GCC OpenMP
    OPENMP_FLAGS = -fopenmp
    OPENMP_LIBS = -fopenmp
    CFLAGS += $(OPENMP_FLAGS)
endif

# ARM tuning is opt-in (specific to Apple M2); `make NATIVE=1` enables it.
ifdef NATIVE
ifeq ($(shell uname -m),arm64)
    CFLAGS += -mcpu=apple-m2
endif
endif

# OpenMP thread defaults (harmless on any core count; only affects parallel demos)
ifeq ($(shell uname -m),arm64)
    export OMP_NUM_THREADS ?= 24
    export OMP_PROC_BIND ?= close
    export OMP_PLACES ?= cores
endif

# Phase 3: Accelerate framework support (macOS only)
ACCELERATE_FLAGS =
ifeq ($(shell uname),Darwin)
    ACCELERATE_FLAGS = -framework Accelerate
endif

LDFLAGS = -lm -lpthread -flto $(OPENMP_LIBS) $(ACCELERATE_FLAGS)

# Directory structure
SRC_DIR = src/quantum_rng
ENTROPY_DIR = src/entropy
HEALTH_DIR = src/health
SECURE_RNG_DIR = src/secure_rng
TEST_DIR = tests
EXAMPLES_DIR = examples

# Source files
CORE_SRCS = $(wildcard $(SRC_DIR)/*.c)
ENTROPY_SRCS = $(wildcard $(ENTROPY_DIR)/*.c)
HEALTH_SRCS = $(wildcard $(HEALTH_DIR)/*.c)
SECURE_RNG_SRCS = $(wildcard $(SECURE_RNG_DIR)/*.c)
PROFILING_SRCS = $(wildcard src/profiling/*.c)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c) $(wildcard $(TEST_DIR)/statistical/*.c)

# Object files
CORE_OBJS = $(CORE_SRCS:.c=.o)
ENTROPY_OBJS = $(ENTROPY_SRCS:.c=.o)
HEALTH_OBJS = $(HEALTH_SRCS:.c=.o)
SECURE_RNG_OBJS = $(SECURE_RNG_SRCS:.c=.o)
PROFILING_OBJS = $(PROFILING_SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)

# Combined object files for complete library
ALL_LIB_OBJS = $(CORE_OBJS) $(ENTROPY_OBJS) $(HEALTH_OBJS) $(SECURE_RNG_OBJS) $(PROFILING_OBJS)

# Output files
LIB = libquantumrng.so
SECURE_LIB = libsecure_qrng.so
CLI = quantum_rng_cli
CLI_V2 = qrng_v2
TEST_BIN = test_quantum_rng
COMPREHENSIVE_TEST = comprehensive_test
EDGE_CASES_TEST = edge_cases_test
KEY_EXCHANGE_TEST = key_exchange_test
QUANTUM_DICE_TEST = quantum_dice_test
QUANTUM_DICE_DEMO = quantum_dice_demo
QUANTUM_CHAIN_TEST = quantum_chain_test
MONTE_CARLO_TEST = monte_carlo_test
OPTIONS_PRICING_TEST = options_pricing_test
OPTIONS_PRICING_DEMO = options_pricing_demo
HEALTH_TESTS = health_tests_test
SECURE_RNG_TEST = secure_rng_test
THREAD_SAFETY_TEST = thread_safety_test
BELL_LOTTERY = bell_certified_lottery
QUANTUM_MONEY = quantum_money
QUANTUM_VS_CLASSICAL = quantum_vs_classical
QUANTUM_SHOWCASE = quantum_showcase
POST_QUANTUM_CRYPTO = post_quantum_crypto
QUANTUM_ADVANTAGE = quantum_advantage_demo
QUANTUM_ATTACK = quantum_attack_classical
QRNG_V3_TEST = qrng_v3_test
GROVER_PARALLEL_BENCH = grover_parallel_benchmark

# Phony targets
.PHONY: all clean test test_examples test_health test_secure_rng test_thread_safety test_v3 showcase quantum_examples parallel_bench examples_all verify_all metal

# Main targets
all: $(LIB) $(SECURE_LIB) $(CLI) $(CLI_V2) $(QRNG_V3_TEST)
	@echo "Running Quantum RNG v3.0 optimized tests..."
	LD_LIBRARY_PATH=. ./$(QRNG_V3_TEST)

# Library builds
$(LIB): $(CORE_OBJS) $(ENTROPY_OBJS) $(HEALTH_OBJS) $(PROFILING_OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# Secure RNG library (includes everything)
$(SECURE_LIB): $(ALL_LIB_OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# CLI builds
$(CLI): src/quantum_rng_cli.o $(LIB)
	$(CC) -o $@ $< -L. -lquantumrng $(LDFLAGS)

$(CLI_V2): src/qrng_cli_v2.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Note: options_pricing.c is a library module (no main). The runnable
# programs are options_pricing_demo and options_pricing_test, defined below.

$(OPTIONS_PRICING_TEST): $(EXAMPLES_DIR)/finance/options_pricing_test.o $(EXAMPLES_DIR)/finance/options_pricing.o $(EXAMPLES_DIR)/finance/heston_model.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

$(OPTIONS_PRICING_DEMO): $(EXAMPLES_DIR)/finance/options_pricing_demo.o $(EXAMPLES_DIR)/finance/options_pricing.o $(EXAMPLES_DIR)/finance/heston_model.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

# Quantum dice builds
$(QUANTUM_DICE_TEST): $(EXAMPLES_DIR)/games/quantum_dice_test.o $(EXAMPLES_DIR)/games/quantum_dice.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

$(QUANTUM_DICE_DEMO): $(EXAMPLES_DIR)/games/quantum_dice_demo.o $(EXAMPLES_DIR)/games/quantum_dice.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

# Quantum chain build
$(QUANTUM_CHAIN_TEST): $(EXAMPLES_DIR)/crypto/quantum_chain_test.o $(EXAMPLES_DIR)/crypto/quantum_chain.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

# Monte Carlo build
$(MONTE_CARLO_TEST): $(EXAMPLES_DIR)/finance/monte_carlo_test.o $(EXAMPLES_DIR)/finance/monte_carlo.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

# Finance command-line front-ends (real CLIs over the pricing/simulation engines)
options_pricing: $(EXAMPLES_DIR)/finance/options_pricing_cli.o $(EXAMPLES_DIR)/finance/options_pricing.o $(EXAMPLES_DIR)/finance/heston_model.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

monte_carlo: $(EXAMPLES_DIR)/finance/monte_carlo_cli.o $(EXAMPLES_DIR)/finance/monte_carlo.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

# Test builds
test: $(TEST_BIN) $(COMPREHENSIVE_TEST) $(EDGE_CASES_TEST) $(QUANTUM_DICE_TEST)
	@echo "Running basic tests..."
	LD_LIBRARY_PATH=. ./$(TEST_BIN)
	@echo "\nRunning comprehensive tests..."
	LD_LIBRARY_PATH=. ./$(COMPREHENSIVE_TEST)
	@echo "\nRunning edge case tests..."
	LD_LIBRARY_PATH=. ./$(EDGE_CASES_TEST)
	@echo "\nRunning quantum dice tests..."
	LD_LIBRARY_PATH=. ./$(QUANTUM_DICE_TEST)

# Health tests (NIST SP 800-90B compliance)
test_health: $(HEALTH_TESTS)
	@echo "Running NIST SP 800-90B health tests..."
	LD_LIBRARY_PATH=. ./$(HEALTH_TESTS)

$(HEALTH_TESTS): $(TEST_DIR)/health_tests_test.o $(HEALTH_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Secure RNG tests (complete integration)
test_secure_rng: $(SECURE_RNG_TEST)
	@echo "Running Secure RNG integration tests..."
	LD_LIBRARY_PATH=. ./$(SECURE_RNG_TEST)

$(SECURE_RNG_TEST): $(TEST_DIR)/secure_rng_test.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Thread safety tests
test_thread_safety: $(THREAD_SAFETY_TEST)
	@echo "Running thread safety and mode switching tests..."
	LD_LIBRARY_PATH=. ./$(THREAD_SAFETY_TEST)

$(THREAD_SAFETY_TEST): $(TEST_DIR)/thread_safety_test.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Quantum RNG v3 test
test_v3: $(QRNG_V3_TEST)
	@echo "Running Quantum RNG v3.0 tests..."
	LD_LIBRARY_PATH=. ./$(QRNG_V3_TEST)

$(QRNG_V3_TEST): $(TEST_DIR)/qrng_v3_test.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Example application tests
test_examples: $(KEY_EXCHANGE_TEST) $(QUANTUM_DICE_DEMO) $(QUANTUM_CHAIN_TEST) $(MONTE_CARLO_TEST) $(OPTIONS_PRICING_TEST) $(OPTIONS_PRICING_DEMO)
	@echo "\nRunning key exchange tests..."
	LD_LIBRARY_PATH=. ./$(KEY_EXCHANGE_TEST)
	@echo "\nRunning quantum dice demo..."
	LD_LIBRARY_PATH=. ./$(QUANTUM_DICE_DEMO)
	@echo "\nRunning quantum chain tests..."
	LD_LIBRARY_PATH=. ./$(QUANTUM_CHAIN_TEST)
	@echo "\nRunning Monte Carlo tests..."
	LD_LIBRARY_PATH=. ./$(MONTE_CARLO_TEST)
	@echo "\nRunning options pricing tests..."
	LD_LIBRARY_PATH=. ./$(OPTIONS_PRICING_TEST)
	@echo "\nRunning options pricing demo..."
	LD_LIBRARY_PATH=. ./$(OPTIONS_PRICING_DEMO)

$(TEST_BIN): $(TEST_DIR)/test_quantum_rng.o $(TEST_DIR)/statistical/statistical_tests.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

$(COMPREHENSIVE_TEST): $(TEST_DIR)/comprehensive_test.o $(TEST_DIR)/statistical/statistical_tests.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

$(EDGE_CASES_TEST): $(TEST_DIR)/edge_cases_test.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

$(KEY_EXCHANGE_TEST): $(EXAMPLES_DIR)/crypto/key_exchange_test.o $(EXAMPLES_DIR)/crypto/key_exchange.o $(LIB)
	$(CC) -o $@ $^ -L. -lquantumrng $(LDFLAGS)

# Showcase examples (new quantum demonstrations)
showcase: $(BELL_LOTTERY) $(QUANTUM_MONEY) $(QUANTUM_VS_CLASSICAL) $(QUANTUM_SHOWCASE)
	@echo "Showcase examples built successfully!"
	@echo "Run: ./quantum_showcase --quick for 5-minute demo"

$(BELL_LOTTERY): $(EXAMPLES_DIR)/games/bell_certified_lottery.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(QUANTUM_MONEY): $(EXAMPLES_DIR)/crypto/quantum_money.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(QUANTUM_VS_CLASSICAL): $(EXAMPLES_DIR)/testing/quantum_vs_classical.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(QUANTUM_SHOWCASE): $(EXAMPLES_DIR)/quantum_showcase.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Other quantum examples
quantum_examples: $(POST_QUANTUM_CRYPTO) $(QUANTUM_ADVANTAGE) $(QUANTUM_ATTACK)
	@echo "Quantum examples built successfully!"

$(POST_QUANTUM_CRYPTO): $(EXAMPLES_DIR)/quantum/post_quantum_crypto.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(QUANTUM_ADVANTAGE): $(EXAMPLES_DIR)/quantum/quantum_advantage_demo.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(QUANTUM_ATTACK): $(EXAMPLES_DIR)/quantum/quantum_attack_classical.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Parallel Grover benchmark (M2 Ultra Phase 1)
parallel_bench: $(GROVER_PARALLEL_BENCH)
	@echo "Running M2 Ultra parallel Grover benchmark..."
	@echo "This tests 24-core parallelization targeting 20-30x speedup"
	LD_LIBRARY_PATH=. ./$(GROVER_PARALLEL_BENCH)

$(GROVER_PARALLEL_BENCH): $(EXAMPLES_DIR)/quantum/grover_parallel_benchmark.o src/quantum_rng/grover_parallel.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# ============================================================================
# COMPLETE EXAMPLE SUITE
# Every example below builds -Wall -Wextra clean and runs to success against
# the full library object set. Grouped by domain.
# ============================================================================

# --- Crypto ---
key_derivation_test: $(EXAMPLES_DIR)/crypto/key_derivation_test.o $(EXAMPLES_DIR)/crypto/key_derivation.o $(EXAMPLES_DIR)/crypto/key_derivation_utils.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

key_verification: $(EXAMPLES_DIR)/crypto/key_verification.o $(EXAMPLES_DIR)/crypto/key_derivation.o $(EXAMPLES_DIR)/crypto/key_derivation_utils.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

secure_token: $(EXAMPLES_DIR)/crypto/secure_token.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

password_gen: $(EXAMPLES_DIR)/crypto/password_gen.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Finance ---
quantum_portfolio: $(EXAMPLES_DIR)/finance/quantum_portfolio.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Games (single-file demos) ---
GAMES_SINGLE = loot_system particle_system procedural_worlds quantum_evolution terrain_generation quantum_slots
$(GAMES_SINGLE): %: $(EXAMPLES_DIR)/games/%.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Machine Learning ---
ML_SINGLE = neural_init quantum_gan quantum_transformer
$(ML_SINGLE): %: $(EXAMPLES_DIR)/ml/%.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Networking ---
NETWORK_SINGLE = quantum_routing traffic_sim
$(NETWORK_SINGLE): %: $(EXAMPLES_DIR)/network/%.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Science ---
SCIENCE_SINGLE = molecular_dynamics quantum_noise quantum_walk weather_sim
$(SCIENCE_SINGLE): %: $(EXAMPLES_DIR)/science/%.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Quantum algorithm demos (single-file) ---
QUANTUM_SINGLE = grover_hash_collision grover_large_scale_demo grover_large_scale_optimized \
                 grover_password_crack phase3_phase4_benchmark
$(QUANTUM_SINGLE): %: $(EXAMPLES_DIR)/quantum/%.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Flagship / testing ---
secure_rng_demo: $(EXAMPLES_DIR)/secure_rng_demo.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

fuzz_test: $(EXAMPLES_DIR)/testing/fuzz_test.o $(ALL_LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- Metal GPU benchmarks (macOS only; require Apple clang + Metal frameworks) ---
# Built with the system compiler because Homebrew LLVM cannot target the Metal
# runtime. Guarded so `make examples_all` stays portable.
METAL_BINS = metal_gpu_benchmark metal_batch_benchmark
metal:
ifeq ($(shell uname),Darwin)
	@echo "Building Metal GPU benchmarks with Apple clang..."
	/usr/bin/clang -Wall -Wextra -O2 -I. -Isrc -o metal_gpu_benchmark \
	    $(EXAMPLES_DIR)/quantum/metal_gpu_benchmark.c src/quantum_rng/metal/metal_bridge.mm \
	    $(ALL_LIB_OBJS) -lm -lpthread $(OPENMP_LIBS) -framework Accelerate \
	    -framework Metal -framework Foundation
	/usr/bin/clang -Wall -Wextra -O2 -I. -Isrc -o metal_batch_benchmark \
	    $(EXAMPLES_DIR)/quantum/metal_batch_benchmark.c src/quantum_rng/metal/metal_bridge.mm \
	    $(ALL_LIB_OBJS) -lm -lpthread $(OPENMP_LIBS) -framework Accelerate \
	    -framework Metal -framework Foundation
	@echo "Metal GPU benchmarks built."
else
	@echo "Metal benchmarks are macOS-only; skipping on $(shell uname)."
endif

# --- Aggregate: build every example (except Metal, which is opt-in) ---
ALL_EXAMPLE_BINS = $(KEY_EXCHANGE_TEST) $(QUANTUM_CHAIN_TEST) key_derivation_test key_verification \
                   $(QUANTUM_MONEY) $(MONTE_CARLO_TEST) $(OPTIONS_PRICING_TEST) $(OPTIONS_PRICING_DEMO) \
                   quantum_portfolio $(QUANTUM_DICE_TEST) $(QUANTUM_DICE_DEMO) $(BELL_LOTTERY) \
                   $(GAMES_SINGLE) $(ML_SINGLE) $(NETWORK_SINGLE) $(SCIENCE_SINGLE) \
                   $(QUANTUM_SINGLE) $(GROVER_PARALLEL_BENCH) $(POST_QUANTUM_CRYPTO) \
                   $(QUANTUM_ADVANTAGE) $(QUANTUM_ATTACK) $(QUANTUM_SHOWCASE) \
                   $(QUANTUM_VS_CLASSICAL) secure_rng_demo fuzz_test \
                   options_pricing monte_carlo secure_token password_gen

examples_all: $(ALL_EXAMPLE_BINS)
	@echo ""
	@echo "All $(words $(ALL_EXAMPLE_BINS)) example binaries built successfully."
	@echo "Metal GPU benchmarks are opt-in: run 'make metal' (macOS)."

# --- Full verification: core suites + every example builds ---
verify_all: test test_health test_secure_rng test_thread_safety test_v3 examples_all
	@echo ""
	@echo "=============================================================="
	@echo " FULL VERIFICATION COMPLETE"
	@echo " Core suites passed and all examples built."
	@echo "=============================================================="

# Clean
clean:
	rm -f $(CORE_OBJS) $(ENTROPY_OBJS) $(HEALTH_OBJS) $(SECURE_RNG_OBJS) $(TEST_OBJS)
	rm -f $(LIB) $(SECURE_LIB) $(CLI) $(CLI_V2) $(TEST_BIN) $(COMPREHENSIVE_TEST) $(EDGE_CASES_TEST)
	rm -f $(KEY_EXCHANGE_TEST) $(QUANTUM_DICE_TEST) $(QUANTUM_DICE_DEMO)
	rm -f $(QUANTUM_CHAIN_TEST) $(MONTE_CARLO_TEST) $(OPTIONS_PRICING_TEST) $(OPTIONS_PRICING_DEMO)
	rm -f $(HEALTH_TESTS) $(SECURE_RNG_TEST) $(THREAD_SAFETY_TEST)
	rm -f $(BELL_LOTTERY) $(QUANTUM_MONEY) $(QUANTUM_VS_CLASSICAL) $(QUANTUM_SHOWCASE)
	rm -f $(POST_QUANTUM_CRYPTO) $(QUANTUM_ADVANTAGE) $(QUANTUM_ATTACK)
	rm -f src/qrng_cli_v2.o tests/thread_safety_test.o tests/qrng_v3_test.o
	rm -f src/quantum_rng/grover_parallel.o examples/quantum/grover_parallel_benchmark.o
	rm -f key_derivation_test key_verification quantum_portfolio
	rm -f $(GAMES_SINGLE) $(ML_SINGLE) $(NETWORK_SINGLE) $(SCIENCE_SINGLE) $(QUANTUM_SINGLE)
	rm -f secure_rng_demo fuzz_test $(METAL_BINS)
	rm -f options_pricing monte_carlo secure_token password_gen
	find . -name "*.o" -delete

# Dependencies
%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Core dependencies
$(CORE_OBJS) $(TEST_OBJS): $(SRC_DIR)/quantum_rng.h
$(TEST_OBJS): $(TEST_DIR)/statistical/statistical_tests.h
$(TEST_DIR)/health_tests_test.o: $(HEALTH_DIR)/health_tests.h
$(HEALTH_OBJS): $(HEALTH_DIR)/health_tests.h
$(ENTROPY_OBJS): $(ENTROPY_DIR)/hardware_entropy.h $(ENTROPY_DIR)/entropy_pool.h
$(HEALTH_OBJS): $(HEALTH_DIR)/health_tests.h
$(PROFILING_OBJS): src/profiling/performance_monitor.h
$(SECURE_RNG_OBJS): $(SECURE_RNG_DIR)/secure_rng.h $(SRC_DIR)/quantum_rng.h $(ENTROPY_DIR)/hardware_entropy.h $(HEALTH_DIR)/health_tests.h
$(TEST_DIR)/secure_rng_test.o: $(SECURE_RNG_DIR)/secure_rng.h
$(TEST_DIR)/qrng_v3_test.o: $(SRC_DIR)/quantum_rng_v3.h
$(SRC_DIR)/quantum_rng_v3.o: $(SRC_DIR)/quantum_rng_v3.h $(SRC_DIR)/quantum_state.h $(SRC_DIR)/quantum_gates.h $(SRC_DIR)/bell_test.h $(SRC_DIR)/grover.h $(ENTROPY_DIR)/entropy_pool.h src/profiling/performance_monitor.h
$(EXAMPLES_DIR)/finance/options_pricing.o: $(EXAMPLES_DIR)/finance/options_pricing.h $(EXAMPLES_DIR)/finance/heston_model.h
$(EXAMPLES_DIR)/games/quantum_dice.o: $(EXAMPLES_DIR)/games/quantum_dice.h
$(EXAMPLES_DIR)/games/quantum_dice_test.o: $(EXAMPLES_DIR)/games/quantum_dice.h
$(EXAMPLES_DIR)/games/quantum_dice_demo.o: $(EXAMPLES_DIR)/games/quantum_dice.h
$(EXAMPLES_DIR)/crypto/quantum_chain.o: $(EXAMPLES_DIR)/crypto/quantum_chain.h
$(EXAMPLES_DIR)/crypto/quantum_chain_test.o: $(EXAMPLES_DIR)/crypto/quantum_chain.h
$(EXAMPLES_DIR)/finance/monte_carlo.o: $(EXAMPLES_DIR)/finance/monte_carlo.h
$(EXAMPLES_DIR)/finance/monte_carlo_test.o: $(EXAMPLES_DIR)/finance/monte_carlo.h
$(EXAMPLES_DIR)/finance/options_pricing_test.o: $(EXAMPLES_DIR)/finance/options_pricing.h $(EXAMPLES_DIR)/finance/heston_model.h
$(EXAMPLES_DIR)/finance/options_pricing_demo.o: $(EXAMPLES_DIR)/finance/options_pricing.h $(EXAMPLES_DIR)/finance/heston_model.h
$(EXAMPLES_DIR)/games/bell_certified_lottery.o: $(EXAMPLES_DIR)/games/bell_certified_lottery.h
$(EXAMPLES_DIR)/crypto/quantum_money.o: $(EXAMPLES_DIR)/crypto/quantum_money.h
src/quantum_rng/grover_parallel.o: src/quantum_rng/grover_parallel.h src/quantum_rng/grover.h
$(EXAMPLES_DIR)/quantum/grover_parallel_benchmark.o: src/quantum_rng/grover_parallel.h

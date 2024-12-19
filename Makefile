# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O3 -march=native -fPIC -I. -I..
LDFLAGS = -lm

# Directory structure
BUILD_DIR = build
SRC_DIR = src/quantum_rng
TEST_DIR = tests
EXAMPLES_DIR = examples

# Source files
CORE_SRCS = $(wildcard $(SRC_DIR)/*.c)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c) $(wildcard $(TEST_DIR)/statistical/*.c)

# Object files
CORE_OBJS = $(CORE_SRCS:.c=.o)
TEST_OBJS = $(TEST_SRCS:.c=.o)

# Output files
LIB = $(BUILD_DIR)/libquantumrng.so
CLI = $(BUILD_DIR)/quantum_rng_cli
TEST_BIN = $(BUILD_DIR)/test_quantum_rng
COMPREHENSIVE_TEST = $(BUILD_DIR)/comprehensive_test
EDGE_CASES_TEST = $(BUILD_DIR)/edge_cases_test
KEY_EXCHANGE_TEST = $(BUILD_DIR)/key_exchange_test
KEY_DERIVATION_TEST = $(BUILD_DIR)/key_derivation_test
OPTIONS_PRICING = $(BUILD_DIR)/options_pricing
QUANTUM_DICE_TEST = $(BUILD_DIR)/quantum_dice_test
QUANTUM_DICE_DEMO = $(BUILD_DIR)/quantum_dice_demo
QUANTUM_CHAIN_TEST = $(BUILD_DIR)/quantum_chain_test
MONTE_CARLO_TEST = $(BUILD_DIR)/monte_carlo_test

# Phony targets
.PHONY: all clean test test_examples

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Main targets
all: $(BUILD_DIR) $(LIB) $(CLI) test test_examples $(OPTIONS_PRICING)

# Library build
$(LIB): $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# CLI build
$(CLI): src/quantum_rng_cli.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $< -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

# Quantum dice builds
$(QUANTUM_DICE_TEST): $(EXAMPLES_DIR)/games/quantum_dice_test.o $(EXAMPLES_DIR)/games/quantum_dice.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

$(QUANTUM_DICE_DEMO): $(EXAMPLES_DIR)/games/quantum_dice_demo.o $(EXAMPLES_DIR)/games/quantum_dice.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

# Quantum chain build
$(QUANTUM_CHAIN_TEST): $(EXAMPLES_DIR)/crypto/quantum_chain_test.o $(EXAMPLES_DIR)/crypto/quantum_chain.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

# Monte Carlo build
$(MONTE_CARLO_TEST): $(EXAMPLES_DIR)/finance/monte_carlo_test.o $(EXAMPLES_DIR)/finance/monte_carlo.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

# Key derivation build
$(KEY_DERIVATION_TEST): $(EXAMPLES_DIR)/crypto/key_derivation_test.o $(EXAMPLES_DIR)/crypto/key_derivation.o $(EXAMPLES_DIR)/crypto/key_derivation_utils.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

# Test builds
test: $(TEST_BIN) $(COMPREHENSIVE_TEST) $(EDGE_CASES_TEST) $(QUANTUM_DICE_TEST)
	@echo "Running basic tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(TEST_BIN)
	@echo "\nRunning comprehensive tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(COMPREHENSIVE_TEST)
	@echo "\nRunning edge case tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(EDGE_CASES_TEST)
	@echo "\nRunning quantum dice tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(QUANTUM_DICE_TEST)

# Example application tests
test_examples: $(KEY_EXCHANGE_TEST) $(KEY_DERIVATION_TEST) $(QUANTUM_DICE_DEMO) $(QUANTUM_CHAIN_TEST) $(MONTE_CARLO_TEST) $(OPTIONS_PRICING_TEST) $(OPTIONS_PRICING_DEMO)
	@echo "\nRunning key exchange tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(KEY_EXCHANGE_TEST)
	@echo "\nRunning key derivation tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(KEY_DERIVATION_TEST)
	@echo "\nRunning quantum dice demo..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(QUANTUM_DICE_DEMO)
	@echo "\nRunning quantum chain tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(QUANTUM_CHAIN_TEST)
	@echo "\nRunning Monte Carlo tests..."
	LD_LIBRARY_PATH=$(BUILD_DIR) ./$(MONTE_CARLO_TEST)

$(TEST_BIN): $(TEST_DIR)/test_quantum_rng.o $(TEST_DIR)/statistical/statistical_tests.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

$(COMPREHENSIVE_TEST): $(TEST_DIR)/comprehensive_test.o $(TEST_DIR)/statistical/statistical_tests.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

$(EDGE_CASES_TEST): $(TEST_DIR)/edge_cases_test.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

$(KEY_EXCHANGE_TEST): $(EXAMPLES_DIR)/crypto/key_exchange_test.o $(EXAMPLES_DIR)/crypto/key_exchange.o $(LIB) | $(BUILD_DIR)
	$(CC) -o $@ $^ -L$(BUILD_DIR) -lquantumrng $(LDFLAGS)

# Clean
clean:
	rm -f $(CORE_OBJS) $(TEST_OBJS)
	rm -rf $(BUILD_DIR)
	find . -name "*.o" -delete

# Dependencies
%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Core dependencies
$(CORE_OBJS) $(TEST_OBJS): $(SRC_DIR)/quantum_rng.h
$(TEST_OBJS): $(TEST_DIR)/statistical/statistical_tests.h
$(EXAMPLES_DIR)/games/quantum_dice.o: $(EXAMPLES_DIR)/games/quantum_dice.h
$(EXAMPLES_DIR)/games/quantum_dice_test.o: $(EXAMPLES_DIR)/games/quantum_dice.h
$(EXAMPLES_DIR)/games/quantum_dice_demo.o: $(EXAMPLES_DIR)/games/quantum_dice.h
$(EXAMPLES_DIR)/crypto/quantum_chain.o: $(EXAMPLES_DIR)/crypto/quantum_chain.h
$(EXAMPLES_DIR)/crypto/quantum_chain_test.o: $(EXAMPLES_DIR)/crypto/quantum_chain.h
$(EXAMPLES_DIR)/crypto/key_derivation.o: $(EXAMPLES_DIR)/crypto/key_derivation.h
$(EXAMPLES_DIR)/crypto/key_derivation_test.o: $(EXAMPLES_DIR)/crypto/key_derivation.h
$(EXAMPLES_DIR)/finance/monte_carlo.o: $(EXAMPLES_DIR)/finance/monte_carlo.h
$(EXAMPLES_DIR)/finance/monte_carlo_test.o: $(EXAMPLES_DIR)/finance/monte_carlo.h

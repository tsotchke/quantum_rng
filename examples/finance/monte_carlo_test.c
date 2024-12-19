#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "monte_carlo.h"
#include "../../src/quantum_rng/quantum_rng.h"

#define EPSILON 0.01  // For floating point comparisons
#define TEST_SEED "quantum_monte_carlo_test_seed"

// Helper functions
static void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

static void print_test_result(const char* test_name, int success) {
    printf("%s: %s\n", test_name, success ? "PASS" : "FAIL");
}

static int double_equals(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

// Test configuration initialization
static void test_config_init() {
    print_test_header("Configuration Initialization");
    
    simulation_config_t config;
    init_simulation_config(&config);
    
    // Verify default values
    assert(config.num_simulations == DEFAULT_NUM_SIMULATIONS);
    assert(config.trading_days == DEFAULT_TRADING_DAYS);
    assert(double_equals(config.asset.initial_price, DEFAULT_INITIAL_PRICE, EPSILON));
    assert(double_equals(config.asset.volatility, DEFAULT_VOLATILITY, EPSILON));
    assert(double_equals(config.asset.risk_free_rate, DEFAULT_RISK_FREE_RATE, EPSILON));
    assert(double_equals(config.asset.dividend_yield, DEFAULT_DIVIDEND_YIELD, EPSILON));
    assert(config.output_mode == OUTPUT_NORMAL);
    
    print_test_result("Configuration initialization", 1);
}

// Test argument parsing
static void test_arg_parsing() {
    print_test_header("Argument Parsing");
    
    simulation_config_t config;
    init_simulation_config(&config);
    
    char *argv[] = {
        "monte_carlo",
        "-n", "50000",
        "-d", "365",
        "-p", "150.0",
        "-v", "0.3",
        "-r", "0.06",
        "-y", "0.03",
        "-o", "json",
        "-s", TEST_SEED
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    
    parse_simulation_args(argc, argv, &config);
    
    assert(config.num_simulations == 50000);
    assert(config.trading_days == 365);
    assert(double_equals(config.asset.initial_price, 150.0, EPSILON));
    assert(double_equals(config.asset.volatility, 0.3, EPSILON));
    assert(double_equals(config.asset.risk_free_rate, 0.06, EPSILON));
    assert(double_equals(config.asset.dividend_yield, 0.03, EPSILON));
    assert(config.output_mode == OUTPUT_JSON);
    assert(strcmp(config.seed, TEST_SEED) == 0);
    
    print_test_result("Argument parsing", 1);
}

// Test simulation results
static void test_simulation_results() {
    print_test_header("Simulation Results");
    
    simulation_config_t config;
    init_simulation_config(&config);
    
    // Use fixed seed for reproducible results
    strncpy(config.seed, TEST_SEED, sizeof(config.seed));
    config.seed_length = strlen(TEST_SEED);
    
    // Run simulation
    simulation_results_t results = run_simulation(&config);
    
    // Verify basic statistical properties
    assert(results.mean_price > 0);
    assert(results.std_dev > 0);
    assert(results.min_price <= results.mean_price);
    assert(results.max_price >= results.mean_price);
    assert(results.confidence_lower < results.confidence_upper);
    
    // Verify theoretical bounds
    double annual_return = config.asset.risk_free_rate - config.asset.dividend_yield;
    double expected_mean = config.asset.initial_price * 
                          exp(annual_return * config.trading_days / DEFAULT_TRADING_DAYS);
    
    // Mean should be within 10% of theoretical value
    assert(fabs(results.mean_price - expected_mean) / expected_mean < 0.1);
    
    free_simulation_results(&results);
    print_test_result("Simulation results", 1);
}

// Test output formats
static void test_output_formats() {
    print_test_header("Output Formats");
    
    simulation_config_t config;
    init_simulation_config(&config);
    strncpy(config.seed, TEST_SEED, sizeof(config.seed));
    config.seed_length = strlen(TEST_SEED);
    
    simulation_results_t results = run_simulation(&config);
    
    // Test JSON output
    config.output_mode = OUTPUT_JSON;
    FILE* json_file = fopen("test_output.json", "w");
    assert(json_file != NULL);
    output_results_json(json_file, &results, &config);
    fclose(json_file);
    
    // Verify JSON file exists and is non-empty
    json_file = fopen("test_output.json", "r");
    assert(json_file != NULL);
    fseek(json_file, 0, SEEK_END);
    long json_size = ftell(json_file);
    assert(json_size > 0);
    fclose(json_file);
    
    // Test CSV output
    config.output_mode = OUTPUT_CSV;
    FILE* csv_file = fopen("test_output.csv", "w");
    assert(csv_file != NULL);
    output_results_csv(csv_file, &results, &config);
    fclose(csv_file);
    
    // Verify CSV file exists and is non-empty
    csv_file = fopen("test_output.csv", "r");
    assert(csv_file != NULL);
    fseek(csv_file, 0, SEEK_END);
    long csv_size = ftell(csv_file);
    assert(csv_size > 0);
    fclose(csv_file);
    
    // Clean up
    remove("test_output.json");
    remove("test_output.csv");
    free_simulation_results(&results);
    
    print_test_result("Output formats", 1);
}

// Test error handling
static void test_error_handling() {
    print_test_header("Error Handling");
    
    simulation_config_t config;
    init_simulation_config(&config);
    
    // Test invalid number of simulations
    config.num_simulations = MIN_SIMULATIONS - 1;
    simulation_results_t results = run_simulation(&config);
    assert(results.prices == NULL);
    
    // Test invalid trading days
    config.num_simulations = DEFAULT_NUM_SIMULATIONS;
    config.trading_days = 0;
    results = run_simulation(&config);
    assert(results.prices == NULL);
    
    // Test invalid asset parameters
    config.trading_days = DEFAULT_TRADING_DAYS;
    config.asset.initial_price = -1.0;
    results = run_simulation(&config);
    assert(results.prices == NULL);
    
    print_test_result("Error handling", 1);
}

// Performance test
static void test_performance() {
    print_test_header("Performance Test");
    
    simulation_config_t config;
    init_simulation_config(&config);
    config.num_simulations = 100000; // Large number for performance testing
    
    // Measure execution time
    clock_t start = clock();
    simulation_results_t results = run_simulation(&config);
    clock_t end = clock();
    
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    double sims_per_second = config.num_simulations / time_spent;
    
    printf("Completed %d simulations in %.2f seconds\n", 
           config.num_simulations, time_spent);
    printf("Performance: %.2f simulations/second\n", sims_per_second);
    
    free_simulation_results(&results);
    print_test_result("Performance test", 1);
}

int main() {
    printf("=== Monte Carlo Simulation Test Suite ===\n");
    
    test_config_init();
    test_arg_parsing();
    test_simulation_results();
    test_output_formats();
    test_error_handling();
    test_performance();
    
    printf("\nAll tests completed successfully!\n");
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include "options_pricing.h"
#include "heston_model.h"
#include "../../src/quantum_rng/quantum_rng.h"

#define EPSILON 0.01  // For deterministic (closed-form) comparisons

/*
 * The quantum RNG produces roughly 0.6M doubles/second and mixes in
 * hardware entropy on every draw, so results are statistical rather than
 * bit-reproducible.  Tests therefore use moderate path counts and compare
 * Monte Carlo estimates against closed-form values within a multiple of
 * the reported standard error.  For European options under GBM the
 * terminal distribution is exact for any step count, so a reduced step
 * count is used where path granularity does not matter.
 */
#define TEST_NUM_PATHS 10000
#define TEST_TIME_STEPS 64
#define TEST_Z_BOUND 5.0   // Assert within 5 standard errors (false-positive rate ~ 1e-6)

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

static void init_test_config(pricing_config_t *config) {
    init_pricing_config(config);
    config->num_paths = TEST_NUM_PATHS;
    config->time_steps = TEST_TIME_STEPS;
    config->show_progress = 0;
}

// Test Monte Carlo pricing against the Black-Scholes closed form
static void test_black_scholes() {
    print_test_header("Black-Scholes Pricing");

    double spot = 100.0;
    double strike = 100.0;
    double rate = 0.05;
    double vol = 0.2;
    double time = 1.0;

    option_params_t params = {
        .spot_price = spot,
        .strike_price = strike,
        .time_to_maturity = time,
        .volatility = vol,
        .risk_free_rate = rate,
        .dividend_yield = 0.0,
        .type = OPTION_CALL
    };

    pricing_config_t config;
    init_test_config(&config);
    config.option = params;

    pricing_results_t results = run_pricing_simulation(&config);
    double bs_call = black_scholes_price(&config.option);

    printf("MC call price: %.4f (std error: %.4f), Black-Scholes: %.4f\n",
           results.price, results.std_error, bs_call);

    // Verify option bounds
    assert(results.price >= 0);
    assert(results.price <= spot);

    // MC estimate must agree with the closed form within its own error bars
    assert(fabs(results.price - bs_call) < TEST_Z_BOUND * results.std_error);

    // Test put option
    config.option.type = OPTION_PUT;
    pricing_results_t put_results = run_pricing_simulation(&config);
    double bs_put = black_scholes_price(&config.option);

    printf("MC put price:  %.4f (std error: %.4f), Black-Scholes: %.4f\n",
           put_results.price, put_results.std_error, bs_put);
    assert(fabs(put_results.price - bs_put) < TEST_Z_BOUND * put_results.std_error);

    // Put-call parity: C - P = S - K e^{-rT}
    double parity_rhs = spot - strike * exp(-rate * time);

    // Exact for the closed form ...
    assert(double_equals(bs_call - bs_put, parity_rhs, 1e-9));

    // ... and within combined MC error for the independent simulations
    double mc_parity_diff = (results.price - put_results.price) - parity_rhs;
    double combined_se = sqrt(results.std_error * results.std_error +
                              put_results.std_error * put_results.std_error);
    printf("MC put-call parity difference: %.4f (combined std error: %.4f)\n",
           mc_parity_diff, combined_se);
    assert(fabs(mc_parity_diff) < TEST_Z_BOUND * combined_se);

    free_pricing_results(&results);
    free_pricing_results(&put_results);

    print_test_result("Black-Scholes pricing", 1);
}

// Test Heston model simulation
static void test_heston_simulation() {
    print_test_header("Heston Model Simulation");

    pricing_config_t config;
    init_test_config(&config);
    config.num_paths = 5000;

    config.vol_model = VOL_HESTON;
    config.heston = (heston_params_t){
        .kappa = 2.0,      // Mean reversion speed
        .theta = 0.04,     // Long-term variance
        .sigma = 0.3,      // Volatility of variance
        .rho = -0.7,       // Price-vol correlation
        .v0 = 0.04         // Initial variance
    };

    config.option = (option_params_t){
        .spot_price = 100.0,
        .strike_price = 100.0,
        .time_to_maturity = 1.0,
        .volatility = 0.2,
        .risk_free_rate = 0.05,
        .dividend_yield = 0.0,
        .type = OPTION_CALL
    };

    pricing_results_t results = run_pricing_simulation(&config);

    printf("Heston price: %.4f (std error: %.4f)\n", results.price, results.std_error);
    printf("95%% CI: [%.4f, %.4f]\n", results.confidence_lower, results.confidence_upper);

    // Verify basic properties
    assert(results.price >= 0);
    assert(results.price <= config.option.spot_price);
    assert(results.std_error > 0);
    assert(results.confidence_lower < results.confidence_upper);

    free_pricing_results(&results);
    print_test_result("Heston simulation", 1);
}

// Test option Greeks (closed form under constant volatility)
static void test_option_greeks() {
    print_test_header("Option Greeks");

    pricing_config_t config;
    init_test_config(&config);
    config.num_paths = 1000;   // Greeks are analytic here; keep pricing cheap
    config.time_steps = 32;

    config.greek_flags = GREEK_ALL;
    config.option = (option_params_t){
        .spot_price = 100.0,
        .strike_price = 100.0,
        .time_to_maturity = 1.0,
        .volatility = 0.2,
        .risk_free_rate = 0.05,
        .dividend_yield = 0.0,
        .type = OPTION_CALL
    };

    pricing_results_t call_results = run_pricing_simulation(&config);
    calculate_greeks(&call_results, &config);

    config.option.type = OPTION_PUT;
    pricing_results_t put_results = run_pricing_simulation(&config);
    calculate_greeks(&put_results, &config);

    printf("Call Greeks: delta=%.4f gamma=%.4f theta=%.4f vega=%.4f rho=%.4f\n",
           call_results.greeks.delta, call_results.greeks.gamma,
           call_results.greeks.theta, call_results.greeks.vega, call_results.greeks.rho);
    printf("Put Greeks:  delta=%.4f gamma=%.4f theta=%.4f vega=%.4f rho=%.4f\n",
           put_results.greeks.delta, put_results.greeks.gamma,
           put_results.greeks.theta, put_results.greeks.vega, put_results.greeks.rho);

    // Verify put-call parity relationships for Greeks
    assert(double_equals(call_results.greeks.delta - put_results.greeks.delta,
                        exp(-config.option.dividend_yield * config.option.time_to_maturity),
                        EPSILON));
    assert(double_equals(call_results.greeks.gamma, put_results.greeks.gamma, EPSILON));
    assert(double_equals(call_results.greeks.vega, put_results.greeks.vega, EPSILON));

    // Verify bounds
    assert(call_results.greeks.delta >= 0 && call_results.greeks.delta <= 1);
    assert(put_results.greeks.delta >= -1 && put_results.greeks.delta <= 0);
    assert(call_results.greeks.gamma >= 0);
    assert(call_results.greeks.vega >= 0);

    free_pricing_results(&call_results);
    free_pricing_results(&put_results);
    print_test_result("Option Greeks", 1);
}

// Test Monte Carlo pricing: constant volatility vs Heston with matched variance
static void test_monte_carlo_pricing() {
    print_test_header("Monte Carlo Pricing");

    pricing_config_t config;
    init_test_config(&config);

    config.option = (option_params_t){
        .spot_price = 100.0,
        .strike_price = 100.0,
        .time_to_maturity = 1.0,
        .volatility = 0.2,
        .risk_free_rate = 0.05,
        .dividend_yield = 0.0,
        .type = OPTION_CALL
    };

    // Run with constant volatility
    pricing_results_t const_vol_results = run_pricing_simulation(&config);
    printf("Constant volatility price: %.4f (std error: %.4f)\n",
           const_vol_results.price, const_vol_results.std_error);

    // Run with Heston model whose long-run variance matches vol^2
    config.vol_model = VOL_HESTON;
    config.heston = (heston_params_t){
        .kappa = 2.0,
        .theta = config.option.volatility * config.option.volatility,
        .sigma = 0.3,
        .rho = -0.7,
        .v0 = config.option.volatility * config.option.volatility
    };

    pricing_results_t heston_results = run_pricing_simulation(&config);
    printf("Heston model price: %.4f (std error: %.4f)\n",
           heston_results.price, heston_results.std_error);

    // Prices should be similar for ATM options with matched variance
    double price_diff = fabs(const_vol_results.price - heston_results.price) /
                        const_vol_results.price;
    printf("Relative price difference: %.2f%%\n", price_diff * 100);
    assert(price_diff < 0.1);

    free_pricing_results(&const_vol_results);
    free_pricing_results(&heston_results);
    print_test_result("Monte Carlo pricing", 1);
}

// Test error handling
static void test_error_handling() {
    print_test_header("Error Handling");

    pricing_config_t config;
    init_test_config(&config);

    // Test invalid spot price
    config.option.spot_price = -100.0;
    pricing_results_t results = run_pricing_simulation(&config);
    assert(isnan(results.price));

    // Test invalid strike price
    config.option.spot_price = 100.0;
    config.option.strike_price = -100.0;
    results = run_pricing_simulation(&config);
    assert(isnan(results.price));

    // Test invalid volatility
    config.option.strike_price = 100.0;
    config.option.volatility = -0.2;
    results = run_pricing_simulation(&config);
    assert(isnan(results.price));

    // Test invalid Heston parameters
    config.vol_model = VOL_HESTON;
    config.option.volatility = 0.2;
    init_heston_params(&config.heston);
    config.heston.kappa = -1.0;  // Invalid mean reversion
    results = run_pricing_simulation(&config);
    assert(isnan(results.price));

    print_test_result("Error handling", 1);
}

// Performance test
static void test_performance() {
    print_test_header("Performance Test");

    pricing_config_t config;
    init_test_config(&config);

    // Measure constant volatility performance
    clock_t start = clock();
    pricing_results_t results = run_pricing_simulation(&config);
    clock_t end = clock();

    double const_vol_time = (double)(end - start) / CLOCKS_PER_SEC;
    if (const_vol_time > 0) {
        printf("Constant volatility: %.2f paths/second (%d paths, %d steps)\n",
               config.num_paths / const_vol_time, config.num_paths, config.time_steps);
    }

    free_pricing_results(&results);

    // Measure Heston model performance
    config.vol_model = VOL_HESTON;
    init_heston_params(&config.heston);

    start = clock();
    results = run_pricing_simulation(&config);
    end = clock();

    double heston_time = (double)(end - start) / CLOCKS_PER_SEC;
    if (heston_time > 0) {
        printf("Heston model: %.2f paths/second (%d paths, %d steps)\n",
               config.num_paths / heston_time, config.num_paths, config.time_steps);
    }

    free_pricing_results(&results);
    print_test_result("Performance test", 1);
}

int main() {
    printf("=== Options Pricing Test Suite ===\n");

    test_black_scholes();
    test_heston_simulation();
    test_option_greeks();
    test_monte_carlo_pricing();
    test_error_handling();
    test_performance();

    printf("\nAll tests completed successfully!\n");
    return 0;
}

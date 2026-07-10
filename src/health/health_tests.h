#ifndef HEALTH_TESTS_H
#define HEALTH_TESTS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file health_tests.h
 * @brief NIST SP 800-90B health tests for entropy sources
 * 
 * Implements continuous health testing as required by NIST SP 800-90B
 * for cryptographic random number generators:
 * 
 * 1. Repetition Count Test (RCT) - detects stuck-at faults
 * 2. Adaptive Proportion Test (APT) - detects loss of entropy
 * 3. Startup Tests - validates initial entropy source behavior
 * 
 * These tests are mandatory for FIPS 140-3 compliance and ensure
 * the entropy source is functioning correctly.
 */

/**
 * @brief Health test error codes
 */
typedef enum {
    HEALTH_SUCCESS = 0,
    HEALTH_ERROR_RCT_FAILURE = -1,      // Repetition count test failed
    HEALTH_ERROR_APT_FAILURE = -2,      // Adaptive proportion test failed
    HEALTH_ERROR_STARTUP_FAILURE = -3,  // Startup test failed
    HEALTH_ERROR_INVALID_PARAM = -4,    // Invalid parameters
    HEALTH_ERROR_NOT_INITIALIZED = -5   // Context not initialized
} health_error_t;

/**
 * @brief Health test configuration
 * 
 * Parameters are derived from NIST SP 800-90B Section 4.
 * Values depend on entropy source min-entropy estimate.
 */
typedef struct {
    // Repetition Count Test (RCT)
    uint32_t rct_cutoff;               // Maximum allowed repetitions
    
    // Adaptive Proportion Test (APT)
    uint32_t apt_cutoff;               // Maximum allowed count in window
    uint32_t apt_window_size;          // Window size (typically 512 or 1024)
    
    // Startup test
    uint32_t startup_test_samples;     // Samples for startup test
    
    // Min-entropy estimate (bits per sample)
    double min_entropy_estimate;       // Conservative entropy estimate
} health_test_config_t;

/**
 * @brief Health test statistics
 */
typedef struct {
    // Test results
    uint64_t rct_failures;             // RCT failure count
    uint64_t apt_failures;             // APT failure count
    uint64_t startup_failures;         // Startup failure count
    
    // Current state
    uint64_t samples_tested;           // Total samples tested
    uint64_t total_failures;           // Total test failures
    
    // RCT state
    uint8_t rct_last_sample;           // Last sample for RCT
    uint32_t rct_current_count;        // Current repetition count
    
    // APT state
    uint8_t apt_first_sample;          // First sample in window
    uint32_t apt_current_count;        // Count of first_sample in window
    uint32_t apt_window_pos;           // Position in window
    uint8_t *apt_window_buffer;        // Window buffer for APT
    
    // Status
    int startup_complete;              // 1 if startup tests passed
    int tests_enabled;                 // 1 if tests are active
} health_test_stats_t;

/**
 * @brief Health test context
 */
typedef struct {
    health_test_config_t config;
    health_test_stats_t stats;
    
    // Callback for test failures
    void (*failure_callback)(health_error_t error, void *user_data);
    void *callback_user_data;
} health_test_ctx_t;

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize health test context with default parameters
 * 
 * Sets conservative defaults based on NIST SP 800-90B recommendations:
 * - RCT cutoff: 31 (for H_min ≥ 4 bits/sample)
 * - APT cutoff: 354 (for H_min ≥ 4 bits/sample, W=512)
 * - APT window: 512 samples
 * 
 * @param ctx Health test context to initialize
 * @return HEALTH_SUCCESS or error code
 */
health_error_t health_tests_init(health_test_ctx_t *ctx);

/**
 * @brief Initialize with custom configuration
 * 
 * @param ctx Health test context
 * @param config Custom configuration
 * @return HEALTH_SUCCESS or error code
 */
health_error_t health_tests_init_custom(health_test_ctx_t *ctx, const health_test_config_t *config);

/**
 * @brief Free health test resources
 * 
 * @param ctx Health test context
 */
void health_tests_free(health_test_ctx_t *ctx);

/**
 * @brief Reset health test statistics
 * 
 * @param ctx Health test context
 */
void health_tests_reset(health_test_ctx_t *ctx);

// ============================================================================
// TEST EXECUTION
// ============================================================================

/**
 * @brief Run startup health tests
 * 
 * Performs initial testing of entropy source with larger sample size.
 * Must pass before allowing normal operation.
 * 
 * @param ctx Health test context
 * @param samples Sample data
 * @param num_samples Number of samples (typically ≥1024)
 * @return HEALTH_SUCCESS if tests pass, error code if failure
 */
health_error_t health_tests_startup(health_test_ctx_t *ctx, const uint8_t *samples, size_t num_samples);

/**
 * @brief Test single sample (continuous testing)
 * 
 * Runs RCT and APT on each sample during normal operation.
 * This function should be called for every entropy sample.
 * 
 * @param ctx Health test context
 * @param sample Sample to test
 * @return HEALTH_SUCCESS if tests pass, error code if failure
 */
health_error_t health_tests_run(health_test_ctx_t *ctx, uint8_t sample);

/**
 * @brief Test multiple samples (batch mode)
 * 
 * More efficient for testing multiple samples at once.
 * 
 * @param ctx Health test context
 * @param samples Array of samples
 * @param num_samples Number of samples
 * @return HEALTH_SUCCESS if all tests pass, error code on first failure
 */
health_error_t health_tests_run_batch(health_test_ctx_t *ctx, const uint8_t *samples, size_t num_samples);

// ============================================================================
// INDIVIDUAL TESTS
// ============================================================================

/**
 * @brief Repetition Count Test (RCT)
 * 
 * Detects stuck-at faults by counting consecutive identical samples.
 * Fails if count exceeds cutoff (typically 31 for H_min ≥ 4).
 * 
 * Formula: C = ceil(1 + (-log₂(2⁻ᴴ) / H))
 * where H is min-entropy per sample.
 * 
 * @param ctx Health test context
 * @param sample Current sample
 * @return HEALTH_SUCCESS or HEALTH_ERROR_RCT_FAILURE
 */
health_error_t health_test_rct(health_test_ctx_t *ctx, uint8_t sample);

/**
 * @brief Adaptive Proportion Test (APT)
 * 
 * Detects loss of entropy by counting occurrences of first sample
 * in a sliding window. Fails if count exceeds cutoff.
 * 
 * Formula: C = critbinom(W, 2⁻ᴴ, 2⁻³⁰)
 * where W is window size, H is min-entropy.
 * 
 * @param ctx Health test context
 * @param sample Current sample
 * @return HEALTH_SUCCESS or HEALTH_ERROR_APT_FAILURE
 */
health_error_t health_test_apt(health_test_ctx_t *ctx, uint8_t sample);

// ============================================================================
// CONFIGURATION HELPERS
// ============================================================================

/**
 * @brief Calculate RCT cutoff from min-entropy
 * 
 * Uses NIST SP 800-90B formula:
 * C = ceil(1 + (-log₂(2⁻ᴴ) / H))
 * 
 * @param min_entropy Min-entropy per sample in bits
 * @return RCT cutoff value
 */
uint32_t health_calculate_rct_cutoff(double min_entropy);

/**
 * @brief Calculate APT cutoff from min-entropy
 * 
 * Uses critical value from binomial distribution with
 * probability 2⁻ᴴ and confidence level 2⁻³⁰.
 * 
 * @param min_entropy Min-entropy per sample in bits
 * @param window_size APT window size
 * @return APT cutoff value
 */
uint32_t health_calculate_apt_cutoff(double min_entropy, uint32_t window_size);

/**
 * @brief Get recommended configuration for entropy estimate
 * 
 * Returns NIST-compliant configuration for given min-entropy.
 * 
 * @param min_entropy Estimated min-entropy (bits/sample)
 * @param config Output configuration
 */
void health_get_recommended_config(double min_entropy, health_test_config_t *config);

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

/**
 * @brief Get test statistics
 * 
 * @param ctx Health test context
 * @return Statistics structure
 */
health_test_stats_t health_tests_get_stats(const health_test_ctx_t *ctx);

/**
 * @brief Check if startup tests passed
 * 
 * @param ctx Health test context
 * @return 1 if passed, 0 otherwise
 */
int health_tests_startup_complete(const health_test_ctx_t *ctx);

/**
 * @brief Print health test statistics
 * 
 * @param ctx Health test context
 */
void health_tests_print_stats(const health_test_ctx_t *ctx);

/**
 * @brief Set failure callback
 * 
 * Callback is invoked whenever a test fails, allowing custom
 * handling (logging, alerts, shutdown, etc.)
 * 
 * @param ctx Health test context
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void health_tests_set_callback(
    health_test_ctx_t *ctx,
    void (*callback)(health_error_t error, void *user_data),
    void *user_data
);

/**
 * @brief Enable/disable tests
 * 
 * Allows temporary disabling of tests (e.g., during initialization).
 * Tests should be enabled for normal operation.
 * 
 * @param ctx Health test context
 * @param enabled 1 to enable, 0 to disable
 */
void health_tests_set_enabled(health_test_ctx_t *ctx, int enabled);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Get error string
 * 
 * @param error Health error code
 * @return Human-readable error description
 */
const char* health_error_string(health_error_t error);

/**
 * @brief Validate configuration parameters
 * 
 * Checks if configuration values are reasonable and NIST-compliant.
 * 
 * @param config Configuration to validate
 * @return 1 if valid, 0 otherwise
 */
int health_validate_config(const health_test_config_t *config);

#endif /* HEALTH_TESTS_H */
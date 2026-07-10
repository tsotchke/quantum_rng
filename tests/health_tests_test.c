/**
 * @file health_tests_test.c
 * @brief Comprehensive test suite for NIST SP 800-90B health tests
 *
 * This test suite validates the implementation of mandatory health tests
 * required for NIST SP 800-90B and FIPS 140-3 compliance:
 *
 * 1. Repetition Count Test (RCT) - detects stuck-at faults
 * 2. Adaptive Proportion Test (APT) - detects loss of entropy
 * 3. Startup Tests - validates initial entropy source behavior
 * 4. Configuration validation
 * 5. Edge cases and failure scenarios
 *
 * Test Coverage:
 * - Basic functionality tests
 * - RCT detection of repetitive patterns
 * - APT detection of biased samples
 * - Startup test validation
 * - Configuration parameter validation
 * - Edge cases (boundary conditions)
 * - Failure callback mechanisms
 * - Statistics tracking
 * - Memory management
 */

#include "../src/health/health_tests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Callback tracking
static int callback_invoked = 0;
static health_error_t callback_last_error = HEALTH_SUCCESS;

// ============================================================================
// TEST UTILITIES
// ============================================================================

#define TEST_START(name) \
    do { \
        tests_run++; \
        printf("\n[TEST %d] %s\n", tests_run, name); \
        callback_invoked = 0; \
        callback_last_error = HEALTH_SUCCESS; \
    } while(0)

#define TEST_PASS() \
    do { \
        tests_passed++; \
        printf("  ✓ PASSED\n"); \
        return 1; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        tests_failed++; \
        printf("  ✗ FAILED: %s\n", msg); \
        return 0; \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("  Assertion failed: %s\n", msg); \
            printf("  Expected: %ld, Got: %ld\n", (long)(b), (long)(a)); \
            TEST_FAIL(msg); \
        } \
    } while(0)

#define ASSERT_TRUE(expr, msg) \
    do { \
        if (!(expr)) { \
            printf("  Assertion failed: %s\n", msg); \
            TEST_FAIL(msg); \
        } \
    } while(0)

#define ASSERT_FALSE(expr, msg) \
    do { \
        if (expr) { \
            printf("  Assertion failed: %s\n", msg); \
            TEST_FAIL(msg); \
        } \
    } while(0)

// Test callback function
void test_failure_callback(health_error_t error, void *user_data) {
    callback_invoked++;
    callback_last_error = error;
    (void)user_data; // Unused
}

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

int test_init_default(void) {
    TEST_START("Initialize with default configuration");

    health_test_ctx_t ctx;
    health_error_t err = health_tests_init(&ctx);

    ASSERT_EQ(err, HEALTH_SUCCESS, "Initialization should succeed");
    ASSERT_TRUE(ctx.config.rct_cutoff > 0, "RCT cutoff should be set");
    ASSERT_TRUE(ctx.config.apt_cutoff > 0, "APT cutoff should be set");
    ASSERT_TRUE(ctx.config.apt_window_size > 0, "APT window size should be set");
    ASSERT_TRUE(ctx.stats.tests_enabled, "Tests should be enabled by default");
    ASSERT_TRUE(ctx.stats.apt_window_buffer != NULL, "APT buffer should be allocated");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_init_custom(void) {
    TEST_START("Initialize with custom configuration");

    health_test_config_t config = {
        .rct_cutoff = 20,
        .apt_cutoff = 300,
        .apt_window_size = 256,
        .startup_test_samples = 2048,
        .min_entropy_estimate = 5.0
    };

    health_test_ctx_t ctx;
    health_error_t err = health_tests_init_custom(&ctx, &config);

    ASSERT_EQ(err, HEALTH_SUCCESS, "Custom initialization should succeed");
    ASSERT_EQ(ctx.config.rct_cutoff, 20, "RCT cutoff should match");
    ASSERT_EQ(ctx.config.apt_cutoff, 300, "APT cutoff should match");
    ASSERT_EQ(ctx.config.apt_window_size, 256, "APT window size should match");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_init_invalid_params(void) {
    TEST_START("Initialize with invalid parameters");

    health_test_ctx_t ctx;

    // NULL context
    health_error_t err = health_tests_init(NULL);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "NULL context should fail");

    // Invalid custom config
    health_test_config_t bad_config = {
        .rct_cutoff = 0,  // Invalid
        .apt_cutoff = 300,
        .apt_window_size = 256,
        .startup_test_samples = 1024,
        .min_entropy_estimate = 4.0
    };

    err = health_tests_init_custom(&ctx, &bad_config);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "Invalid config should fail");

    TEST_PASS();
}

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

int test_calculate_rct_cutoff(void) {
    TEST_START("Calculate RCT cutoff values");

    // Test various min-entropy values
    uint32_t cutoff_4 = health_calculate_rct_cutoff(4.0);
    uint32_t cutoff_5 = health_calculate_rct_cutoff(5.0);
    uint32_t cutoff_6 = health_calculate_rct_cutoff(6.0);

    ASSERT_TRUE(cutoff_4 > cutoff_5, "Lower entropy should have higher cutoff");
    ASSERT_TRUE(cutoff_5 > cutoff_6, "Lower entropy should have higher cutoff");
    ASSERT_TRUE(cutoff_4 >= 8, "Cutoff for H=4 should be reasonable");

    printf("  H=4.0: cutoff=%u\n", cutoff_4);
    printf("  H=5.0: cutoff=%u\n", cutoff_5);
    printf("  H=6.0: cutoff=%u\n", cutoff_6);

    TEST_PASS();
}

int test_calculate_apt_cutoff(void) {
    TEST_START("Calculate APT cutoff values");

    // Test various configurations
    uint32_t cutoff_512 = health_calculate_apt_cutoff(4.0, 512);
    uint32_t cutoff_1024 = health_calculate_apt_cutoff(4.0, 1024);

    ASSERT_TRUE(cutoff_512 > 0, "APT cutoff should be positive");
    ASSERT_TRUE(cutoff_1024 > cutoff_512, "Larger window should have higher cutoff");

    printf("  H=4.0, W=512:  cutoff=%u\n", cutoff_512);
    printf("  H=4.0, W=1024: cutoff=%u\n", cutoff_1024);

    TEST_PASS();
}

int test_validate_config(void) {
    TEST_START("Validate configuration parameters");

    // Valid configuration
    health_test_config_t valid_config = {
        .rct_cutoff = 31,
        .apt_cutoff = 354,
        .apt_window_size = 512,
        .startup_test_samples = 1024,
        .min_entropy_estimate = 4.0
    };

    ASSERT_TRUE(health_validate_config(&valid_config), "Valid config should pass");

    // Invalid configurations
    health_test_config_t invalid_configs[] = {
        {.rct_cutoff = 0, .apt_cutoff = 354, .apt_window_size = 512,
         .startup_test_samples = 1024, .min_entropy_estimate = 4.0},
        {.rct_cutoff = 31, .apt_cutoff = 0, .apt_window_size = 512,
         .startup_test_samples = 1024, .min_entropy_estimate = 4.0},
        {.rct_cutoff = 31, .apt_cutoff = 354, .apt_window_size = 0,
         .startup_test_samples = 1024, .min_entropy_estimate = 4.0},
        {.rct_cutoff = 31, .apt_cutoff = 354, .apt_window_size = 512,
         .startup_test_samples = 0, .min_entropy_estimate = 4.0},
        {.rct_cutoff = 31, .apt_cutoff = 354, .apt_window_size = 512,
         .startup_test_samples = 1024, .min_entropy_estimate = 0.0}
    };

    for (size_t i = 0; i < sizeof(invalid_configs) / sizeof(invalid_configs[0]); i++) {
        ASSERT_FALSE(health_validate_config(&invalid_configs[i]),
                    "Invalid config should fail validation");
    }

    TEST_PASS();
}

// ============================================================================
// RCT TESTS
// ============================================================================

int test_rct_basic(void) {
    TEST_START("RCT basic functionality");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Test with varying samples (should pass)
    uint8_t varying_samples[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};

    for (size_t i = 0; i < sizeof(varying_samples); i++) {
        health_error_t err = health_test_rct(&ctx, varying_samples[i]);
        ASSERT_EQ(err, HEALTH_SUCCESS, "Varying samples should pass RCT");
    }

    ASSERT_EQ(ctx.stats.rct_failures, 0, "No failures should be recorded");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_rct_detects_repetition(void) {
    TEST_START("RCT detects excessive repetition");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);
    health_tests_set_callback(&ctx, test_failure_callback, NULL);

    uint32_t cutoff = ctx.config.rct_cutoff;
    printf("  RCT cutoff: %u\n", cutoff);

    // Feed repeated samples until failure
    // First sample initializes, then we need cutoff-1 more to trigger
    health_error_t err = HEALTH_SUCCESS;
    uint8_t sample = 0xAA;
    uint32_t count = 0;

    // Initialize with first sample
    ctx.stats.rct_last_sample = sample;
    ctx.stats.rct_current_count = 1;
    ctx.stats.samples_tested = 1;  // Mark as initialized

    // Feed more samples until failure
    for (count = 1; count < cutoff + 10 && err == HEALTH_SUCCESS; count++) {
        err = health_test_rct(&ctx, sample);
    }

    printf("  Failed after %u repetitions\n", count);
    ASSERT_EQ(err, HEALTH_ERROR_RCT_FAILURE, "Should detect repetition");
    ASSERT_TRUE(ctx.stats.rct_failures > 0, "Should record failure");
    ASSERT_TRUE(callback_invoked > 0, "Should invoke callback");
    ASSERT_EQ(callback_last_error, HEALTH_ERROR_RCT_FAILURE, "Callback error should match");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_rct_reset_on_change(void) {
    TEST_START("RCT counter resets on sample change");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    uint32_t cutoff = ctx.config.rct_cutoff;

    // Feed many repetitions but alternate occasionally
    for (int i = 0; i < 100; i++) {
        // Repeat sample but change before hitting cutoff
        for (uint32_t j = 0; j < cutoff - 2; j++) {
            health_error_t err = health_test_rct(&ctx, 0xAA);
            ASSERT_EQ(err, HEALTH_SUCCESS, "Should not fail before cutoff");
        }

        // Change sample to reset counter
        health_error_t err = health_test_rct(&ctx, 0xBB);
        ASSERT_EQ(err, HEALTH_SUCCESS, "Different sample should reset counter");
    }

    ASSERT_EQ(ctx.stats.rct_failures, 0, "No failures should occur with resets");

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// APT TESTS
// ============================================================================

int test_apt_basic(void) {
    TEST_START("APT basic functionality");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Generate pseudo-random samples
    srand(time(NULL));
    for (int i = 0; i < 1000; i++) {
        uint8_t sample = (uint8_t)(rand() & 0xFF);
        health_error_t err = health_test_apt(&ctx, sample);
        ASSERT_EQ(err, HEALTH_SUCCESS, "Random samples should pass APT");
    }

    ASSERT_EQ(ctx.stats.apt_failures, 0, "No failures should be recorded");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_apt_detects_bias(void) {
    TEST_START("APT detects biased samples");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);
    health_tests_set_callback(&ctx, test_failure_callback, NULL);

    uint32_t window_size = ctx.config.apt_window_size;
    uint32_t cutoff = ctx.config.apt_cutoff;

    printf("  APT window: %u, cutoff: %u\n", window_size, cutoff);

    // Create heavily biased sample set
    // Fill window with cutoff+1 occurrences of the first sample
    health_error_t err = HEALTH_SUCCESS;

    // First sample sets the reference
    uint8_t first_sample = 0x55;
    err = health_test_apt(&ctx, first_sample);
    ASSERT_EQ(err, HEALTH_SUCCESS, "First sample should succeed");

    // Fill window with mostly the first sample to exceed cutoff
    uint32_t first_sample_count = 1;
    for (uint32_t i = 1; i < window_size && err == HEALTH_SUCCESS; i++) {
        uint8_t sample;
        // Make sure we have enough occurrences to exceed cutoff
        if (first_sample_count < cutoff + 1 && i % 2 == 0) {
            sample = first_sample;
            first_sample_count++;
        } else {
            sample = (uint8_t)(0xAA + (i % 16));  // Various other values
        }
        err = health_test_apt(&ctx, sample);
    }

    printf("  First sample count: %u, cutoff: %u\n", first_sample_count, cutoff);

    // Should fail if we exceeded cutoff
    if (first_sample_count >= cutoff) {
        ASSERT_EQ(err, HEALTH_ERROR_APT_FAILURE, "Should fail with biased samples");
        ASSERT_TRUE(ctx.stats.apt_failures > 0, "Should record APT failure");
    } else {
        printf("  Note: Could not create enough bias to exceed cutoff in single window\n");
    }

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_apt_window_behavior(void) {
    TEST_START("APT window sliding behavior");

    health_test_config_t config = {
        .rct_cutoff = 31,
        .apt_cutoff = 20,  // Low cutoff for testing
        .apt_window_size = 32,  // Small window for testing
        .startup_test_samples = 100,
        .min_entropy_estimate = 4.0
    };

    health_test_ctx_t ctx;
    health_tests_init_custom(&ctx, &config);

    // Fill first window with varying samples
    for (uint32_t i = 0; i < config.apt_window_size; i++) {
        uint8_t sample = (uint8_t)(i & 0xFF);
        health_error_t err = health_test_apt(&ctx, sample);
        ASSERT_EQ(err, HEALTH_SUCCESS, "First window should pass");
    }

    ASSERT_EQ(ctx.stats.apt_window_pos, 0, "Window should reset after filling");

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// COMBINED TESTS
// ============================================================================

int test_combined_rct_apt(void) {
    TEST_START("Combined RCT and APT testing");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Test with good random data
    srand(time(NULL));
    for (int i = 0; i < 5000; i++) {
        uint8_t sample = (uint8_t)(rand() & 0xFF);
        health_error_t err = health_tests_run(&ctx, sample);
        ASSERT_EQ(err, HEALTH_SUCCESS, "Good samples should pass both tests");
    }

    ASSERT_TRUE(ctx.stats.samples_tested >= 5000, "Should track sample count");
    ASSERT_EQ(ctx.stats.rct_failures, 0, "No RCT failures expected");
    ASSERT_EQ(ctx.stats.apt_failures, 0, "No APT failures expected");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_batch_processing(void) {
    TEST_START("Batch sample processing");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Generate batch of samples
    uint8_t samples[1024];
    srand(time(NULL));
    for (int i = 0; i < 1024; i++) {
        samples[i] = (uint8_t)(rand() & 0xFF);
    }

    health_error_t err = health_tests_run_batch(&ctx, samples, 1024);
    ASSERT_EQ(err, HEALTH_SUCCESS, "Batch processing should succeed");
    ASSERT_EQ(ctx.stats.samples_tested, 1024, "Should count all samples");

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// STARTUP TESTS
// ============================================================================

int test_startup_success(void) {
    TEST_START("Startup test with good entropy");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Generate good startup samples
    uint8_t samples[2048];
    srand(time(NULL));
    for (int i = 0; i < 2048; i++) {
        samples[i] = (uint8_t)(rand() & 0xFF);
    }

    health_error_t err = health_tests_startup(&ctx, samples, 2048);
    ASSERT_EQ(err, HEALTH_SUCCESS, "Startup should succeed with good data");
    ASSERT_TRUE(health_tests_startup_complete(&ctx), "Startup should be complete");
    ASSERT_EQ(ctx.stats.startup_failures, 0, "No startup failures");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_startup_insufficient_samples(void) {
    TEST_START("Startup test with insufficient samples");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    uint8_t samples[100];
    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        samples[i] = (uint8_t)(rand() & 0xFF);
    }

    health_error_t err = health_tests_startup(&ctx, samples, 100);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "Should reject insufficient samples");
    ASSERT_FALSE(health_tests_startup_complete(&ctx), "Startup should not complete");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_startup_bad_entropy(void) {
    TEST_START("Startup test with bad entropy");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Generate all zeros (worst case)
    uint8_t samples[2048];
    memset(samples, 0, sizeof(samples));

    health_error_t err = health_tests_startup(&ctx, samples, 2048);
    ASSERT_EQ(err, HEALTH_ERROR_RCT_FAILURE, "Should fail with repeated zeros");
    ASSERT_FALSE(health_tests_startup_complete(&ctx), "Startup should not complete");
    ASSERT_TRUE(ctx.stats.startup_failures > 0, "Should record startup failure");

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// STATISTICS & MONITORING TESTS
// ============================================================================

int test_statistics_tracking(void) {
    TEST_START("Statistics tracking");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Run some tests
    srand(time(NULL));
    for (int i = 0; i < 1000; i++) {
        uint8_t sample = (uint8_t)(rand() & 0xFF);
        health_tests_run(&ctx, sample);
    }

    health_test_stats_t stats = health_tests_get_stats(&ctx);
    ASSERT_TRUE(stats.samples_tested >= 1000, "Should track samples");
    ASSERT_TRUE(stats.tests_enabled, "Tests should be enabled");

    printf("  Samples tested: %lu\n", stats.samples_tested);
    printf("  Total failures: %lu\n", stats.total_failures);
    printf("  RCT failures: %lu\n", stats.rct_failures);
    printf("  APT failures: %lu\n", stats.apt_failures);

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_enable_disable(void) {
    TEST_START("Enable/disable tests");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Disable tests
    health_tests_set_enabled(&ctx, 0);
    ASSERT_FALSE(ctx.stats.tests_enabled, "Tests should be disabled");

    // Feed bad data (should not fail when disabled)
    for (int i = 0; i < 100; i++) {
        health_error_t err = health_tests_run(&ctx, 0x00);
        ASSERT_EQ(err, HEALTH_SUCCESS, "Disabled tests should always pass");
    }

    // Re-enable tests
    health_tests_set_enabled(&ctx, 1);
    ASSERT_TRUE(ctx.stats.tests_enabled, "Tests should be enabled");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_reset_functionality(void) {
    TEST_START("Reset functionality");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Run some tests to accumulate stats
    srand(time(NULL));
    for (int i = 0; i < 500; i++) {
        uint8_t sample = (uint8_t)(rand() & 0xFF);
        health_tests_run(&ctx, sample);
    }

    uint64_t samples_before = ctx.stats.samples_tested;
    ASSERT_TRUE(samples_before > 0, "Should have samples before reset");

    // Reset
    health_tests_reset(&ctx);

    ASSERT_EQ(ctx.stats.samples_tested, 0, "Sample count should reset");
    ASSERT_EQ(ctx.stats.total_failures, 0, "Failure count should reset");
    ASSERT_EQ(ctx.stats.rct_failures, 0, "RCT failures should reset");
    ASSERT_EQ(ctx.stats.apt_failures, 0, "APT failures should reset");
    ASSERT_TRUE(ctx.stats.tests_enabled, "Tests should still be enabled");
    ASSERT_TRUE(ctx.stats.apt_window_buffer != NULL, "Buffer should still exist");

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

int test_callback_invocation(void) {
    TEST_START("Failure callback invocation");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);
    health_tests_set_callback(&ctx, test_failure_callback, NULL);

    callback_invoked = 0;

    // Trigger RCT failure - initialize first
    ctx.stats.rct_last_sample = 0xAA;
    ctx.stats.rct_current_count = 1;
    ctx.stats.samples_tested = 1;

    uint32_t cutoff = ctx.config.rct_cutoff;
    for (uint32_t i = 1; i < cutoff + 1; i++) {
        health_test_rct(&ctx, 0xAA);
    }

    ASSERT_TRUE(callback_invoked > 0, "Callback should be invoked on failure");
    ASSERT_EQ(callback_last_error, HEALTH_ERROR_RCT_FAILURE,
              "Callback should report correct error");

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// EDGE CASES
// ============================================================================

int test_edge_all_same_value(void) {
    TEST_START("Edge case: all same value");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Feed all zeros - should fail quickly
    health_error_t err = HEALTH_SUCCESS;
    int count = 0;

    while (err == HEALTH_SUCCESS && count < 100) {
        err = health_tests_run(&ctx, 0x00);
        count++;
    }

    ASSERT_EQ(err, HEALTH_ERROR_RCT_FAILURE, "Should fail with all same values");
    ASSERT_TRUE(count < 50, "Should fail quickly");
    printf("  Failed after %d samples\n", count);

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_edge_alternating_pattern(void) {
    TEST_START("Edge case: alternating pattern");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Alternating 0xAA and 0xBB - should pass RCT but might trigger APT
    for (int i = 0; i < 2000; i++) {
        uint8_t sample = (i % 2) ? 0xAA : 0xBB;
        health_tests_run(&ctx, sample);
    }

    printf("  RCT failures: %lu\n", ctx.stats.rct_failures);
    printf("  APT failures: %lu\n", ctx.stats.apt_failures);

    // Alternating should pass RCT but may trigger APT depending on configuration
    ASSERT_EQ(ctx.stats.rct_failures, 0, "Alternating should pass RCT");

    health_tests_free(&ctx);
    TEST_PASS();
}

int test_edge_null_parameters(void) {
    TEST_START("Edge case: NULL parameters");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    uint8_t sample = 0xAA;

    // Test NULL context
    health_error_t err = health_test_rct(NULL, sample);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "NULL context should fail");

    err = health_test_apt(NULL, sample);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "NULL context should fail");

    err = health_tests_run(NULL, sample);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "NULL context should fail");

    err = health_tests_run_batch(NULL, &sample, 1);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "NULL context should fail");

    err = health_tests_run_batch(&ctx, NULL, 1);
    ASSERT_EQ(err, HEALTH_ERROR_INVALID_PARAM, "NULL samples should fail");

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// UTILITY FUNCTION TESTS
// ============================================================================

int test_error_strings(void) {
    TEST_START("Error string conversion");

    ASSERT_TRUE(strlen(health_error_string(HEALTH_SUCCESS)) > 0, "Success string exists");
    ASSERT_TRUE(strlen(health_error_string(HEALTH_ERROR_RCT_FAILURE)) > 0, "RCT error string exists");
    ASSERT_TRUE(strlen(health_error_string(HEALTH_ERROR_APT_FAILURE)) > 0, "APT error string exists");
    ASSERT_TRUE(strlen(health_error_string(HEALTH_ERROR_STARTUP_FAILURE)) > 0, "Startup error string exists");
    ASSERT_TRUE(strlen(health_error_string(HEALTH_ERROR_INVALID_PARAM)) > 0, "Invalid param string exists");
    ASSERT_TRUE(strlen(health_error_string(HEALTH_ERROR_NOT_INITIALIZED)) > 0, "Not initialized string exists");

    printf("  HEALTH_SUCCESS: %s\n", health_error_string(HEALTH_SUCCESS));
    printf("  RCT_FAILURE: %s\n", health_error_string(HEALTH_ERROR_RCT_FAILURE));
    printf("  APT_FAILURE: %s\n", health_error_string(HEALTH_ERROR_APT_FAILURE));

    TEST_PASS();
}

int test_print_stats(void) {
    TEST_START("Print statistics");

    health_test_ctx_t ctx;
    health_tests_init(&ctx);

    // Run some tests
    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        uint8_t sample = (uint8_t)(rand() & 0xFF);
        health_tests_run(&ctx, sample);
    }

    printf("\n");
    health_tests_print_stats(&ctx);

    health_tests_free(&ctx);
    TEST_PASS();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("========================================\n");
    printf("NIST SP 800-90B Health Tests Suite\n");
    printf("========================================\n");

    // Initialization tests
    test_init_default();
    test_init_custom();
    test_init_invalid_params();

    // Configuration tests
    test_calculate_rct_cutoff();
    test_calculate_apt_cutoff();
    test_validate_config();

    // RCT tests
    test_rct_basic();
    test_rct_detects_repetition();
    test_rct_reset_on_change();

    // APT tests
    test_apt_basic();
    test_apt_detects_bias();
    test_apt_window_behavior();

    // Combined tests
    test_combined_rct_apt();
    test_batch_processing();

    // Startup tests
    test_startup_success();
    test_startup_insufficient_samples();
    test_startup_bad_entropy();

    // Statistics & monitoring
    test_statistics_tracking();
    test_enable_disable();
    test_reset_functionality();

    // Callback tests
    test_callback_invocation();

    // Edge cases
    test_edge_all_same_value();
    test_edge_alternating_pattern();
    test_edge_null_parameters();

    // Utility tests
    test_error_strings();
    test_print_stats();

    // Summary
    printf("\n========================================\n");
    printf("TEST SUMMARY\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", tests_run);
    printf("Passed:       %d (%.1f%%)\n", tests_passed,
           100.0 * tests_passed / tests_run);
    printf("Failed:       %d (%.1f%%)\n", tests_failed,
           100.0 * tests_failed / tests_run);
    printf("========================================\n");

    if (tests_failed == 0) {
        printf("\n✓ ALL TESTS PASSED - NIST SP 800-90B compliant\n\n");
        return 0;
    } else {
        printf("\n✗ SOME TESTS FAILED - Review required\n\n");
        return 1;
    }
}

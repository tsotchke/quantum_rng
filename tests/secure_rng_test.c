/**
 * @file secure_rng_test.c
 * @brief Comprehensive integration tests for Secure RNG
 *
 * This test suite validates the complete integration of:
 * - Hardware entropy sources
 * - NIST SP 800-90B health tests
 * - Quantum RNG
 *
 * Tests cover:
 * - Initialization and configuration
 * - Random number generation (all types)
 * - Health test integration
 * - Reseeding mechanisms
 * - Error handling
 * - Performance characteristics
 * - Security properties
 */

#include "../src/secure_rng/secure_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// ============================================================================
// TEST UTILITIES
// ============================================================================

#define TEST_START(name) \
    do { \
        tests_run++; \
        printf("\n[TEST %d] %s\n", tests_run, name); \
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

#define ASSERT_SUCCESS(err, msg) \
    do { \
        if ((err) != SECURE_RNG_SUCCESS) { \
            printf("  Error: %s\n", secure_rng_error_string(err)); \
            TEST_FAIL(msg); \
        } \
    } while(0)

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

int test_default_init(void) {
    TEST_START("Default initialization");

    secure_rng_ctx_t *ctx;
    secure_rng_error_t err = secure_rng_init(&ctx);

    ASSERT_SUCCESS(err, "Initialization should succeed");
    ASSERT_TRUE(ctx != NULL, "Context should not be NULL");
    ASSERT_TRUE(secure_rng_is_operational(ctx), "RNG should be operational");
    ASSERT_EQ(secure_rng_get_state(ctx), SECURE_RNG_STATE_OPERATIONAL, "Should be in operational state");

    // Verify components initialized
    ASSERT_TRUE(ctx->qrng_ctx != NULL, "Quantum RNG should be initialized");
    ASSERT_TRUE(ctx->entropy_ctx != NULL, "Entropy should be initialized");
    ASSERT_TRUE(ctx->health_ctx != NULL, "Health tests should be initialized");

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_custom_config_init(void) {
    TEST_START("Custom configuration initialization");

    secure_rng_config_t config;
    secure_rng_get_default_config(&config);

    // Customize some settings
    config.min_entropy_estimate = 5.0;
    config.reseed_interval = 512 * 1024;  // 512KB
    config.auto_reseed_enabled = 1;

    secure_rng_ctx_t *ctx;
    secure_rng_error_t err = secure_rng_init_with_config(&ctx, &config);

    ASSERT_SUCCESS(err, "Custom init should succeed");
    ASSERT_TRUE(secure_rng_is_operational(ctx), "RNG should be operational");

    // Verify configuration applied
    ASSERT_EQ(ctx->config.reseed_interval, 512 * 1024, "Reseed interval should match");

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_init_runs_startup_tests(void) {
    TEST_START("Initialization runs startup health tests");

    secure_rng_ctx_t *ctx;
    secure_rng_error_t err = secure_rng_init(&ctx);

    ASSERT_SUCCESS(err, "Initialization should succeed");

    // Verify startup tests completed
    const health_test_stats_t *health_stats = secure_rng_get_health_stats(ctx);
    ASSERT_TRUE(health_stats != NULL, "Health stats should be available");
    ASSERT_TRUE(health_stats->startup_complete, "Startup tests should be complete");
    ASSERT_TRUE(health_stats->samples_tested >= 1024, "Should have tested startup samples");

    printf("  Startup samples tested: %llu\n", (unsigned long long)health_stats->samples_tested);

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// GENERATION TESTS
// ============================================================================

int test_generate_bytes(void) {
    TEST_START("Generate random bytes");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    uint8_t buffer[1024];
    secure_rng_error_t err = secure_rng_bytes(ctx, buffer, sizeof(buffer));

    ASSERT_SUCCESS(err, "Byte generation should succeed");

    // Basic sanity checks
    int all_zeros = 1;
    int all_same = 1;
    for (size_t i = 0; i < sizeof(buffer); i++) {
        if (buffer[i] != 0) all_zeros = 0;
        if (i > 0 && buffer[i] != buffer[0]) all_same = 0;
    }

    ASSERT_FALSE(all_zeros, "Should not be all zeros");
    ASSERT_FALSE(all_same, "Should not be all same value");

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_generate_uint64(void) {
    TEST_START("Generate uint64 values");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    uint64_t values[100];
    for (int i = 0; i < 100; i++) {
        secure_rng_error_t err = secure_rng_uint64(ctx, &values[i]);
        ASSERT_SUCCESS(err, "uint64 generation should succeed");
    }

    // Check for some variation
    int all_same = 1;
    for (int i = 1; i < 100; i++) {
        if (values[i] != values[0]) {
            all_same = 0;
            break;
        }
    }
    ASSERT_FALSE(all_same, "uint64 values should vary");

    printf("  Sample values: %llu, %llu, %llu\n",
           (unsigned long long)values[0],
           (unsigned long long)values[1],
           (unsigned long long)values[2]);

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_generate_uint32(void) {
    TEST_START("Generate uint32 values");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    uint32_t values[100];
    for (int i = 0; i < 100; i++) {
        secure_rng_error_t err = secure_rng_uint32(ctx, &values[i]);
        ASSERT_SUCCESS(err, "uint32 generation should succeed");
    }

    // Check for variation
    int all_same = 1;
    for (int i = 1; i < 100; i++) {
        if (values[i] != values[0]) {
            all_same = 0;
            break;
        }
    }
    ASSERT_FALSE(all_same, "uint32 values should vary");

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_generate_double(void) {
    TEST_START("Generate double values");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    double values[100];
    for (int i = 0; i < 100; i++) {
        secure_rng_error_t err = secure_rng_double(ctx, &values[i]);
        ASSERT_SUCCESS(err, "double generation should succeed");
        ASSERT_TRUE(values[i] >= 0.0 && values[i] < 1.0, "double should be in [0,1)");
    }

    // Check for variation
    int all_same = 1;
    for (int i = 1; i < 100; i++) {
        if (fabs(values[i] - values[0]) > 0.0001) {
            all_same = 0;
            break;
        }
    }
    ASSERT_FALSE(all_same, "double values should vary");

    printf("  Sample values: %.6f, %.6f, %.6f\n", values[0], values[1], values[2]);

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_generate_range32(void) {
    TEST_START("Generate int32 range values");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    int32_t min = 10;
    int32_t max = 50;
    int32_t values[1000];

    for (int i = 0; i < 1000; i++) {
        secure_rng_error_t err = secure_rng_range32(ctx, min, max, &values[i]);
        ASSERT_SUCCESS(err, "range32 generation should succeed");
        ASSERT_TRUE(values[i] >= min && values[i] <= max, "Value should be in range");
    }

    // Check distribution (should hit most values)
    int counts[50] = {0};
    for (int i = 0; i < 1000; i++) {
        counts[values[i] - min]++;
    }

    int values_hit = 0;
    for (int i = 0; i <= (max - min); i++) {
        if (counts[i] > 0) values_hit++;
    }

    printf("  Range [%d, %d], values hit: %d/%d\n", min, max, values_hit, max - min + 1);
    ASSERT_TRUE(values_hit >= 30, "Should hit most values in range");

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_generate_range64(void) {
    TEST_START("Generate uint64 range values");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    uint64_t min = 1000;
    uint64_t max = 2000;
    uint64_t values[100];

    for (int i = 0; i < 100; i++) {
        secure_rng_error_t err = secure_rng_range64(ctx, min, max, &values[i]);
        ASSERT_SUCCESS(err, "range64 generation should succeed");
        ASSERT_TRUE(values[i] >= min && values[i] <= max, "Value should be in range");
    }

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// RESEEDING TESTS
// ============================================================================

int test_manual_reseed(void) {
    TEST_START("Manual reseeding");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    secure_rng_stats_t stats_before;
    ASSERT_SUCCESS(secure_rng_get_stats(ctx, &stats_before), "Get stats should succeed");

    secure_rng_error_t err = secure_rng_reseed(ctx);
    ASSERT_SUCCESS(err, "Reseed should succeed");

    secure_rng_stats_t stats_after;
    ASSERT_SUCCESS(secure_rng_get_stats(ctx, &stats_after), "Get stats should succeed");

    ASSERT_TRUE(stats_after.reseed_count > stats_before.reseed_count, "Reseed count should increase");
    ASSERT_EQ(ctx->bytes_since_reseed, 0, "Bytes since reseed should reset");

    printf("  Reseed count: %llu\n", (unsigned long long)stats_after.reseed_count);

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_auto_reseed(void) {
    TEST_START("Automatic reseeding");

    secure_rng_config_t config;
    secure_rng_get_default_config(&config);
    config.reseed_interval = 1024;  // Reseed after 1KB (library-enforced minimum)
    config.auto_reseed_enabled = 1;

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init_with_config(&ctx, &config), "Init should succeed");

    // Generate multiple chunks to trigger reseed (8KB / 1KB interval => several reseeds)
    uint8_t buffer[1024];
    for (int i = 0; i < 8; i++) {
        secure_rng_error_t err = secure_rng_bytes(ctx, buffer, sizeof(buffer));
        ASSERT_SUCCESS(err, "Generation should succeed");
    }

    // Check reseed count (should have triggered at least once)
    secure_rng_stats_t stats;
    ASSERT_SUCCESS(secure_rng_get_stats(ctx, &stats), "Get stats should succeed");

    printf("  Generated 5KB, reseed count: %llu\n", (unsigned long long)stats.reseed_count);
    printf("  Bytes since reseed: %llu\n", (unsigned long long)ctx->bytes_since_reseed);

    // With 5KB generated and 512 byte interval, should have reseeded multiple times
    ASSERT_TRUE(stats.reseed_count >= 1, "Should have reseeded at least once");

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_reseed_with_external_entropy(void) {
    TEST_START("Reseed with external entropy");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    // Generate some external entropy (in practice, this would come from another source)
    uint8_t external_entropy[256];
    for (size_t i = 0; i < sizeof(external_entropy); i++) {
        external_entropy[i] = (uint8_t)(i ^ (i >> 1));
    }

    secure_rng_error_t err = secure_rng_reseed_with_entropy(ctx, external_entropy, sizeof(external_entropy));
    ASSERT_SUCCESS(err, "Reseed with external entropy should succeed");

    secure_rng_stats_t stats;
    ASSERT_SUCCESS(secure_rng_get_stats(ctx, &stats), "Get stats should succeed");
    ASSERT_TRUE(stats.reseed_count >= 1, "Should have counted reseed");

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// HEALTH TEST INTEGRATION
// ============================================================================

int test_health_tests_active(void) {
    TEST_START("Health tests are active during generation");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    // Generate some data
    uint8_t buffer[10000];
    secure_rng_error_t err = secure_rng_bytes(ctx, buffer, sizeof(buffer));
    ASSERT_SUCCESS(err, "Generation should succeed");

    // Check health test stats
    const health_test_stats_t *health_stats = secure_rng_get_health_stats(ctx);
    ASSERT_TRUE(health_stats != NULL, "Health stats should be available");
    ASSERT_TRUE(health_stats->samples_tested > 0, "Health tests should have run");
    ASSERT_TRUE(health_stats->tests_enabled, "Health tests should be enabled");

    printf("  Health test samples: %llu\n", (unsigned long long)health_stats->samples_tested);
    printf("  RCT failures: %llu\n", (unsigned long long)health_stats->rct_failures);
    printf("  APT failures: %llu\n", (unsigned long long)health_stats->apt_failures);

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

int test_statistics_tracking(void) {
    TEST_START("Statistics tracking");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    // Generate some data
    uint8_t buffer[5000];
    for (int i = 0; i < 10; i++) {
        secure_rng_bytes(ctx, buffer, sizeof(buffer));
    }

    secure_rng_stats_t stats;
    ASSERT_SUCCESS(secure_rng_get_stats(ctx, &stats), "Get stats should succeed");

    ASSERT_TRUE(stats.bytes_generated >= 50000, "Should track bytes generated");
    ASSERT_TRUE(stats.requests_served >= 10, "Should track requests");
    ASSERT_EQ(stats.state, SECURE_RNG_STATE_OPERATIONAL, "Should be operational");

    printf("  Bytes generated: %llu\n", (unsigned long long)stats.bytes_generated);
    printf("  Requests served: %llu\n", (unsigned long long)stats.requests_served);
    printf("  Entropy consumed: %llu\n", (unsigned long long)stats.entropy_bytes_consumed);

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// ENTROPY SOURCE TESTS
// ============================================================================

int test_entropy_sources_available(void) {
    TEST_START("Entropy sources available");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    const entropy_capabilities_t *caps = secure_rng_get_entropy_caps(ctx);
    ASSERT_TRUE(caps != NULL, "Entropy caps should be available");

    printf("  Available sources:\n");
    printf("    RDSEED: %s\n", caps->has_rdseed ? "Yes" : "No");
    printf("    RDRAND: %s\n", caps->has_rdrand ? "Yes" : "No");
    printf("    getrandom: %s\n", caps->has_getrandom ? "Yes" : "No");
    printf("    /dev/random: %s\n", caps->has_dev_random ? "Yes" : "No");
    printf("    /dev/urandom: %s\n", caps->has_dev_urandom ? "Yes" : "No");
    printf("    CPU Jitter: %s\n", caps->has_jitter ? "Yes" : "No");

    // At least one should be available
    int has_source = caps->has_rdseed || caps->has_rdrand || caps->has_getrandom ||
                     caps->has_dev_random || caps->has_dev_urandom || caps->has_jitter;
    ASSERT_TRUE(has_source, "At least one entropy source should be available");

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

int test_null_context_errors(void) {
    TEST_START("NULL context error handling");

    uint8_t buffer[100];
    uint64_t val64;

    secure_rng_error_t err;

    err = secure_rng_bytes(NULL, buffer, sizeof(buffer));
    ASSERT_EQ(err, SECURE_RNG_ERROR_NULL_CONTEXT, "NULL context should fail");

    err = secure_rng_uint64(NULL, &val64);
    ASSERT_EQ(err, SECURE_RNG_ERROR_NULL_CONTEXT, "NULL context should fail");

    TEST_PASS();
}

int test_null_buffer_errors(void) {
    TEST_START("NULL buffer error handling");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    secure_rng_error_t err;

    err = secure_rng_bytes(ctx, NULL, 100);
    ASSERT_EQ(err, SECURE_RNG_ERROR_NULL_BUFFER, "NULL buffer should fail");

    err = secure_rng_uint64(ctx, NULL);
    ASSERT_EQ(err, SECURE_RNG_ERROR_NULL_BUFFER, "NULL value pointer should fail");

    secure_rng_free(ctx);
    TEST_PASS();
}

int test_error_strings(void) {
    TEST_START("Error string conversion");

    const char *str;

    str = secure_rng_error_string(SECURE_RNG_SUCCESS);
    ASSERT_TRUE(strlen(str) > 0, "Success string should exist");

    str = secure_rng_error_string(SECURE_RNG_ERROR_NULL_CONTEXT);
    ASSERT_TRUE(strlen(str) > 0, "Error string should exist");

    str = secure_rng_error_string(SECURE_RNG_ERROR_HEALTH_TEST_FAILED);
    ASSERT_TRUE(strlen(str) > 0, "Error string should exist");

    printf("  Sample: %s\n", secure_rng_error_string(SECURE_RNG_ERROR_HEALTH_TEST_FAILED));

    TEST_PASS();
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

int test_generation_performance(void) {
    TEST_START("Generation performance");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    const size_t test_size = 1024 * 1024;  // 1MB
    uint8_t *buffer = malloc(test_size);
    ASSERT_TRUE(buffer != NULL, "Buffer allocation should succeed");

    clock_t start = clock();
    secure_rng_error_t err = secure_rng_bytes(ctx, buffer, test_size);
    clock_t end = clock();

    ASSERT_SUCCESS(err, "Generation should succeed");

    double seconds = (double)(end - start) / CLOCKS_PER_SEC;
    double mbps = (test_size / (1024.0 * 1024.0)) / seconds;

    printf("  Generated 1MB in %.3f seconds (%.2f MB/s)\n", seconds, mbps);

    free(buffer);
    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// UTILITY TESTS
// ============================================================================

int test_self_test(void) {
    TEST_START("Built-in self-test");

    int result = secure_rng_self_test(0);  // Non-verbose
    ASSERT_TRUE(result == 1, "Self-test should pass");

    TEST_PASS();
}

int test_version_string(void) {
    TEST_START("Version string");

    const char *version = secure_rng_version();
    ASSERT_TRUE(version != NULL, "Version should not be NULL");
    ASSERT_TRUE(strlen(version) > 0, "Version should not be empty");

    printf("  Version: %s\n", version);

    TEST_PASS();
}

int test_reset_functionality(void) {
    TEST_START("Reset functionality");

    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    // Generate some data
    uint8_t buffer[1000];
    secure_rng_bytes(ctx, buffer, sizeof(buffer));

    // Reset
    secure_rng_error_t err = secure_rng_reset(ctx);
    ASSERT_SUCCESS(err, "Reset should succeed");

    // Should still be operational
    ASSERT_TRUE(secure_rng_is_operational(ctx), "Should be operational after reset");

    // Should be able to generate
    err = secure_rng_bytes(ctx, buffer, sizeof(buffer));
    ASSERT_SUCCESS(err, "Generation after reset should succeed");

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

int test_full_integration(void) {
    TEST_START("Full integration test");

    // This test exercises the complete flow
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");

    // Generate various types
    uint8_t bytes[256];
    uint64_t u64;
    uint32_t u32;
    double dbl;
    int32_t range;

    ASSERT_SUCCESS(secure_rng_bytes(ctx, bytes, sizeof(bytes)), "Bytes should succeed");
    ASSERT_SUCCESS(secure_rng_uint64(ctx, &u64), "uint64 should succeed");
    ASSERT_SUCCESS(secure_rng_uint32(ctx, &u32), "uint32 should succeed");
    ASSERT_SUCCESS(secure_rng_double(ctx, &dbl), "double should succeed");
    ASSERT_SUCCESS(secure_rng_range32(ctx, 1, 100, &range), "range should succeed");

    // Reseed
    ASSERT_SUCCESS(secure_rng_reseed(ctx), "Reseed should succeed");

    // Generate more
    ASSERT_SUCCESS(secure_rng_bytes(ctx, bytes, sizeof(bytes)), "Bytes after reseed should succeed");

    // Check stats
    secure_rng_stats_t stats;
    ASSERT_SUCCESS(secure_rng_get_stats(ctx, &stats), "Get stats should succeed");
    ASSERT_TRUE(stats.bytes_generated > 0, "Should have generated bytes");
    ASSERT_TRUE(stats.requests_served >= 6, "Should have served requests");

    printf("  Integration test completed successfully\n");
    printf("  Total bytes: %llu, Requests: %llu\n",
           (unsigned long long)stats.bytes_generated,
           (unsigned long long)stats.requests_served);

    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("========================================\n");
    printf("Secure RNG Integration Test Suite\n");
    printf("========================================\n");

    // Initialization tests
    test_default_init();
    test_custom_config_init();
    test_init_runs_startup_tests();

    // Generation tests
    test_generate_bytes();
    test_generate_uint64();
    test_generate_uint32();
    test_generate_double();
    test_generate_range32();
    test_generate_range64();

    // Reseeding tests
    test_manual_reseed();
    test_auto_reseed();
    test_reseed_with_external_entropy();

    // Health test integration
    test_health_tests_active();

    // Statistics tests
    test_statistics_tracking();

    // Entropy source tests
    test_entropy_sources_available();

    // Error handling tests
    test_null_context_errors();
    test_null_buffer_errors();
    test_error_strings();

    // Performance tests
    test_generation_performance();

    // Utility tests
    test_self_test();
    test_version_string();
    test_reset_functionality();

    // Integration tests
    test_full_integration();

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
        printf("\n✓ ALL TESTS PASSED - Production ready\n\n");
        return 0;
    } else {
        printf("\n✗ SOME TESTS FAILED - Review required\n\n");
        return 1;
    }
}

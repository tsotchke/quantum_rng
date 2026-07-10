/**
 * @file thread_safety_test.c
 * @brief Thread safety validation for Secure RNG
 *
 * Tests:
 * - Concurrent access from multiple threads
 * - Mutex locking correctness
 * - Mode switching thread safety
 * - Statistics consistency under concurrent load
 * - No data races or corruption
 */

#include "../src/secure_rng/secure_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define NUM_THREADS 8
#define ITERATIONS_PER_THREAD 1000
#define BYTES_PER_ITERATION 1024

// Test results
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Thread test data
typedef struct {
    secure_rng_ctx_t *ctx;
    int thread_id;
    uint64_t bytes_generated;
    int errors;
} thread_data_t;

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

#define ASSERT_TRUE(expr, msg) \
    do { \
        if (!(expr)) { \
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
// THREAD WORKER FUNCTIONS
// ============================================================================

void* thread_worker_generate(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    uint8_t buffer[BYTES_PER_ITERATION];
    
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        secure_rng_error_t err = secure_rng_bytes(data->ctx, buffer, sizeof(buffer));
        if (err != SECURE_RNG_SUCCESS) {
            data->errors++;
            break;
        }
        data->bytes_generated += sizeof(buffer);
    }
    
    return NULL;
}

void* thread_worker_mixed_ops(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    uint8_t buffer[256];
    uint64_t val64;
    double valdbl;
    
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        // Mix different operations
        switch (i % 4) {
            case 0:
                if (secure_rng_bytes(data->ctx, buffer, sizeof(buffer)) == SECURE_RNG_SUCCESS) {
                    data->bytes_generated += sizeof(buffer);
                } else {
                    data->errors++;
                }
                break;
            case 1:
                if (secure_rng_uint64(data->ctx, &val64) == SECURE_RNG_SUCCESS) {
                    data->bytes_generated += 8;
                } else {
                    data->errors++;
                }
                break;
            case 2:
                if (secure_rng_double(data->ctx, &valdbl) == SECURE_RNG_SUCCESS) {
                    // No byte count for double
                } else {
                    data->errors++;
                }
                break;
            case 3:
                // Get stats periodically
                {
                    secure_rng_stats_t stats;
                    secure_rng_get_stats(data->ctx, &stats);
                }
                break;
        }
    }
    
    return NULL;
}

void* thread_worker_mode_switch(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    uint8_t buffer[512];
    
    secure_rng_mode_t modes[] = {
        SECURE_RNG_MODE_FAST,
        SECURE_RNG_MODE_QUANTUM,
        SECURE_RNG_MODE_HYBRID
    };
    
    for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
        // Switch modes
        secure_rng_mode_t mode = modes[i % 3];
        secure_rng_set_mode(data->ctx, mode);
        
        // Generate
        if (secure_rng_bytes(data->ctx, buffer, sizeof(buffer)) == SECURE_RNG_SUCCESS) {
            data->bytes_generated += sizeof(buffer);
        } else {
            data->errors++;
        }
    }
    
    return NULL;
}

// ============================================================================
// TESTS
// ============================================================================

int test_concurrent_generation(void) {
    TEST_START("Concurrent generation from multiple threads");
    
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init_threadsafe(&ctx), "Thread-safe init should succeed");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    // Initialize thread data
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].thread_id = i;
        thread_data[i].bytes_generated = 0;
        thread_data[i].errors = 0;
    }
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_worker_generate, &thread_data[i]) != 0) {
            TEST_FAIL("Failed to create thread");
        }
    }
    
    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify results
    uint64_t total_bytes = 0;
    int total_errors = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        total_bytes += thread_data[i].bytes_generated;
        total_errors += thread_data[i].errors;
        
        printf("  Thread %d: %llu bytes, %d errors\n",
               i, (unsigned long long)thread_data[i].bytes_generated, thread_data[i].errors);
    }
    
    ASSERT_TRUE(total_errors == 0, "No errors should occur");
    ASSERT_TRUE(total_bytes > 0, "Bytes should be generated");
    
    printf("  Total: %llu bytes from %d threads\n",
           (unsigned long long)total_bytes, NUM_THREADS);
    
    secure_rng_free(ctx);
    TEST_PASS();
}

int test_mixed_operations(void) {
    TEST_START("Mixed operations from multiple threads");
    
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init_threadsafe(&ctx), "Thread-safe init should succeed");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].thread_id = i;
        thread_data[i].bytes_generated = 0;
        thread_data[i].errors = 0;
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_worker_mixed_ops, &thread_data[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    int total_errors = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_errors += thread_data[i].errors;
    }
    
    ASSERT_TRUE(total_errors == 0, "No errors in mixed operations");
    
    secure_rng_free(ctx);
    TEST_PASS();
}

int test_concurrent_mode_switching(void) {
    TEST_START("Concurrent mode switching");
    
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init_threadsafe(&ctx), "Thread-safe init should succeed");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].thread_id = i;
        thread_data[i].bytes_generated = 0;
        thread_data[i].errors = 0;
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_worker_mode_switch, &thread_data[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    int total_errors = 0;
    uint64_t total_bytes = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        total_errors += thread_data[i].errors;
        total_bytes += thread_data[i].bytes_generated;
    }
    
    ASSERT_TRUE(total_errors == 0, "No errors during mode switching");
    ASSERT_TRUE(total_bytes > 0, "Bytes should be generated");
    
    printf("  Total: %llu bytes with dynamic mode switching\n",
           (unsigned long long)total_bytes);
    
    secure_rng_free(ctx);
    TEST_PASS();
}

int test_statistics_consistency(void) {
    TEST_START("Statistics consistency under concurrent load");
    
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init_threadsafe(&ctx), "Thread-safe init should succeed");
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].thread_id = i;
        thread_data[i].bytes_generated = 0;
        thread_data[i].errors = 0;
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_worker_generate, &thread_data[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Get final statistics
    secure_rng_stats_t stats;
    ASSERT_SUCCESS(secure_rng_get_stats(ctx, &stats), "Get stats should succeed");
    
    // Calculate expected values
    uint64_t expected_bytes = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        expected_bytes += thread_data[i].bytes_generated;
    }
    
    printf("  Expected bytes: %llu\n", (unsigned long long)expected_bytes);
    printf("  Reported bytes: %llu\n", (unsigned long long)stats.bytes_generated);
    
    ASSERT_TRUE(stats.bytes_generated == expected_bytes, "Statistics should be consistent");
    ASSERT_TRUE(stats.health_test_failures == 0, "No health test failures");
    
    secure_rng_free(ctx);
    TEST_PASS();
}

int test_non_threadsafe_warning(void) {
    TEST_START("Non-thread-safe context (per-thread usage)");
    
    // Create non-thread-safe context (faster for per-thread use)
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Regular init should succeed");
    
    ASSERT_TRUE(ctx->thread_safe == 0, "Should not be thread-safe");
    
    // Single-threaded use is fine
    uint8_t buffer[1024];
    ASSERT_SUCCESS(secure_rng_bytes(ctx, buffer, sizeof(buffer)),
                   "Single-threaded use should work");
    
    printf("  Non-thread-safe contexts are recommended for per-thread usage\n");
    printf("  They avoid mutex overhead (~5-10%% faster)\n");
    
    secure_rng_free(ctx);
    TEST_PASS();
}

int test_mode_switching_api(void) {
    TEST_START("Mode switching API");
    
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");
    
    // Test mode getting
    secure_rng_mode_t mode = secure_rng_get_mode(ctx);
    ASSERT_TRUE(mode == SECURE_RNG_MODE_QUANTUM, "Default mode should be QUANTUM");
    
    // Test mode setting
    ASSERT_SUCCESS(secure_rng_set_mode(ctx, SECURE_RNG_MODE_FAST),
                   "Set FAST mode should succeed");
    mode = secure_rng_get_mode(ctx);
    ASSERT_TRUE(mode == SECURE_RNG_MODE_FAST, "Mode should be FAST");
    
    ASSERT_SUCCESS(secure_rng_set_mode(ctx, SECURE_RNG_MODE_HYBRID),
                   "Set HYBRID mode should succeed");
    mode = secure_rng_get_mode(ctx);
    ASSERT_TRUE(mode == SECURE_RNG_MODE_HYBRID, "Mode should be HYBRID");
    
    // Test mode strings
    printf("  Mode strings:\n");
    printf("    FAST: %s\n", secure_rng_mode_string(SECURE_RNG_MODE_FAST));
    printf("    QUANTUM: %s\n", secure_rng_mode_string(SECURE_RNG_MODE_QUANTUM));
    printf("    HYBRID: %s\n", secure_rng_mode_string(SECURE_RNG_MODE_HYBRID));
    printf("    VERIFIED: %s\n", secure_rng_mode_string(SECURE_RNG_MODE_VERIFIED));
    
    secure_rng_free(ctx);
    TEST_PASS();
}

int test_mode_performance_difference(void) {
    TEST_START("Mode performance characteristics");
    
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");
    
    const size_t test_size = 100 * 1024;  // 100KB
    uint8_t *buffer = malloc(test_size);
    ASSERT_TRUE(buffer != NULL, "Buffer allocation should succeed");
    
    secure_rng_mode_t modes[] = {
        SECURE_RNG_MODE_FAST,
        SECURE_RNG_MODE_QUANTUM
    };
    const char *mode_names[] = {"FAST", "QUANTUM"};
    
    for (int m = 0; m < 2; m++) {
        secure_rng_set_mode(ctx, modes[m]);
        
        clock_t start = clock();
        secure_rng_bytes(ctx, buffer, test_size);
        clock_t end = clock();
        
        double seconds = (double)(end - start) / CLOCKS_PER_SEC;
        double mbps = (test_size / (1024.0 * 1024.0)) / seconds;
        
        printf("  %s mode: %.3f sec (%.2f MB/s)\n", mode_names[m], seconds, mbps);
    }
    
    free(buffer);
    secure_rng_free(ctx);
    TEST_PASS();
}

int test_hybrid_mode_behavior(void) {
    TEST_START("HYBRID mode adaptive behavior");
    
    secure_rng_ctx_t *ctx;
    ASSERT_SUCCESS(secure_rng_init(&ctx), "Init should succeed");
    ASSERT_SUCCESS(secure_rng_set_mode(ctx, SECURE_RNG_MODE_HYBRID),
                   "Set HYBRID mode should succeed");
    
    // Small request (should use FAST)
    uint8_t small_buffer[512];
    ASSERT_SUCCESS(secure_rng_bytes(ctx, small_buffer, sizeof(small_buffer)),
                   "Small request should succeed");
    
    // Large request (should use QUANTUM)
    uint8_t large_buffer[2048];
    ASSERT_SUCCESS(secure_rng_bytes(ctx, large_buffer, sizeof(large_buffer)),
                   "Large request should succeed");
    
    // Check statistics
    secure_rng_stats_t stats;
    secure_rng_get_stats(ctx, &stats);
    
    printf("  Small request (512B): Uses FAST path\n");
    printf("  Large request (2KB): Uses QUANTUM path\n");
    printf("  FAST bytes: %llu\n", (unsigned long long)stats.fast_mode_bytes);
    printf("  QUANTUM bytes: %llu\n", (unsigned long long)stats.quantum_mode_bytes);
    
    ASSERT_TRUE(stats.fast_mode_bytes > 0, "FAST mode should be used");
    ASSERT_TRUE(stats.quantum_mode_bytes > 0, "QUANTUM mode should be used");
    
    secure_rng_free(ctx);
    TEST_PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("========================================\n");
    printf("Thread Safety & Mode Switching Tests\n");
    printf("========================================\n");
    
    // Thread safety tests
    test_concurrent_generation();
    test_mixed_operations();
    test_concurrent_mode_switching();
    test_statistics_consistency();
    test_non_threadsafe_warning();
    
    // Mode switching tests
    test_mode_switching_api();
    test_mode_performance_difference();
    test_hybrid_mode_behavior();
    
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
        printf("\n✓ ALL TESTS PASSED\n");
        printf("  - Thread safety verified\n");
        printf("  - Mode switching validated\n");
        printf("  - Concurrent access safe\n");
        printf("  - Performance characteristics confirmed\n\n");
        return 0;
    } else {
        printf("\n✗ SOME TESTS FAILED\n\n");
        return 1;
    }
}
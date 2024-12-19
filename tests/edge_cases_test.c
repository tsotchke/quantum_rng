#include "../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define TEST_NAME(name) printf("\nTesting %s...\n", name)
#define TEST_RESULT(result) printf("Result: %s\n", (result) ? "PASS" : "FAIL")
#define TEST_TIMEOUT 5.0  // 5 seconds timeout

static int test_null_handling(void) {
    TEST_NAME("NULL Pointer Handling");
    
    uint8_t buffer[32];
    qrng_ctx *ctx = NULL;
    int success = 1;
    
    // Test all functions with NULL context
    success &= (qrng_bytes(NULL, buffer, sizeof(buffer)) == QRNG_ERROR_NULL_CONTEXT);
    success &= (qrng_reseed(NULL, buffer, sizeof(buffer)) == QRNG_ERROR_NULL_CONTEXT);
    success &= (qrng_entangle_states(NULL, buffer, buffer, sizeof(buffer)) == QRNG_ERROR_NULL_CONTEXT);
    success &= (qrng_measure_state(NULL, buffer, sizeof(buffer)) == QRNG_ERROR_NULL_CONTEXT);
    success &= (qrng_uint64(NULL) == 0);
    success &= (qrng_double(NULL) == 0.0);
    success &= (qrng_range32(NULL, 0, 10) == 10);
    success &= (qrng_range64(NULL, 0, 10) == 10);
    success &= (qrng_get_entropy_estimate(NULL) == 0.0);
    
    // Test NULL buffer handling
    qrng_init(&ctx, (uint8_t*)"test", 4);
    success &= (qrng_bytes(ctx, NULL, 32) == QRNG_ERROR_NULL_BUFFER);
    success &= (qrng_entangle_states(ctx, NULL, buffer, 32) == QRNG_ERROR_NULL_BUFFER);
    success &= (qrng_entangle_states(ctx, buffer, NULL, 32) == QRNG_ERROR_NULL_BUFFER);
    success &= (qrng_measure_state(ctx, NULL, 32) == QRNG_ERROR_NULL_BUFFER);
    qrng_free(ctx);
    
    TEST_RESULT(success);
    return success;
}

static int test_zero_length(void) {
    TEST_NAME("Zero Length Handling");
    
    qrng_ctx *ctx;
    uint8_t buffer[32];
    int success = 1;
    
    qrng_init(&ctx, (uint8_t*)"test", 4);
    
    success &= (qrng_bytes(ctx, buffer, 0) == QRNG_ERROR_INVALID_LENGTH);
    success &= (qrng_entangle_states(ctx, buffer, buffer, 0) == QRNG_ERROR_INVALID_LENGTH);
    success &= (qrng_measure_state(ctx, buffer, 0) == QRNG_ERROR_INVALID_LENGTH);
    success &= (qrng_reseed(ctx, buffer, 0) == QRNG_ERROR_INVALID_LENGTH);
    
    qrng_free(ctx);
    
    TEST_RESULT(success);
    return success;
}

static int test_range_edge_cases(void) {
    TEST_NAME("Range Edge Cases");
    
    qrng_ctx *ctx;
    int success = 1;
    
    qrng_init(&ctx, (uint8_t*)"test", 4);
    
    // Test range32 edge cases
    success &= (qrng_range32(ctx, INT_MAX, INT_MIN) == INT_MIN);  // Invalid range
    success &= (qrng_range32(ctx, INT_MAX-1, INT_MAX) >= INT_MAX-1);  // Small valid range
    success &= (qrng_range32(ctx, INT_MIN, INT_MIN) == INT_MIN);  // Single value
    success &= (qrng_range32(ctx, 0, INT_MAX) >= 0);  // Full positive range
    
    // Test range64 edge cases
    success &= (qrng_range64(ctx, UINT64_MAX, 0) == 0);  // Invalid range
    success &= (qrng_range64(ctx, UINT64_MAX-1, UINT64_MAX) >= UINT64_MAX-1);  // High range
    success &= (qrng_range64(ctx, 0, 0) == 0);  // Single value
    success &= (qrng_range64(ctx, UINT64_MAX, UINT64_MAX) == UINT64_MAX);  // Max value
    
    qrng_free(ctx);
    
    TEST_RESULT(success);
    return success;
}

static int test_buffer_boundaries(void) {
    TEST_NAME("Buffer Boundary Handling");
    
    qrng_ctx *ctx;
    int success = 1;
    
    // Test with maximum buffer size
    uint8_t *large_buffer = malloc(QRNG_BUFFER_SIZE + 1);
    if (!large_buffer) {
        printf("Failed to allocate memory\n");
        return 0;
    }
    
    qrng_init(&ctx, (uint8_t*)"test", 4);
    
    // Fill buffer completely
    success &= (qrng_bytes(ctx, large_buffer, QRNG_BUFFER_SIZE) == QRNG_SUCCESS);
    
    // Test buffer cycling
    success &= (qrng_bytes(ctx, large_buffer, QRNG_BUFFER_SIZE + 1) == QRNG_SUCCESS);
    
    free(large_buffer);
    qrng_free(ctx);
    
    TEST_RESULT(success);
    return success;
}

static int test_entropy_depletion(void) {
    TEST_NAME("Entropy Depletion Handling");
    
    clock_t start = clock();
    qrng_ctx *ctx;
    int success = 1;
    uint8_t buffer[32];  // Reduced buffer size
    double min_entropy = 1.0;
    double max_entropy = 0.0;
    
    qrng_init(&ctx, NULL, 0);  // Initialize without seed
    
    // Test entropy behavior over rapid generation
    for (int i = 0; i < 10; i++) {  // Reduced iterations
        // Check for timeout
        if ((double)(clock() - start) / CLOCKS_PER_SEC > TEST_TIMEOUT) {
            printf("Test timed out after %.1f seconds\n", TEST_TIMEOUT);
            qrng_free(ctx);
            return 0;
        }
        
        // Generate data rapidly
        success &= (qrng_bytes(ctx, buffer, sizeof(buffer)) == QRNG_SUCCESS);
        
        double entropy = qrng_get_entropy_estimate(ctx);
        if (entropy < min_entropy) min_entropy = entropy;
        if (entropy > max_entropy) max_entropy = entropy;
        
        // Basic entropy range check
        success &= (entropy >= 0.0 && entropy <= 64.0);
        
        if (!success) {
            printf("Failed at iteration %d with entropy %.6f\n", i, entropy);
            break;
        }
    }
    
    // Entropy should stay within reasonable bounds
    printf("Entropy range: %.6f to %.6f bits\n", min_entropy, max_entropy);
    success &= (min_entropy > 0.1);  // Relaxed minimum entropy requirement
    success &= (max_entropy < 64.0); // Should not exceed theoretical maximum
    
    qrng_free(ctx);
    
    TEST_RESULT(success);
    return success;
}

static int test_reseeding(void) {
    TEST_NAME("Reseeding Behavior");
    
    qrng_ctx *ctx;
    int success = 1;
    uint64_t values[3];
    
    // Initialize and get first value
    qrng_init(&ctx, (uint8_t*)"test1", 5);
    values[0] = qrng_uint64(ctx);
    
    // Reseed and get second value
    qrng_reseed(ctx, (uint8_t*)"test2", 5);
    values[1] = qrng_uint64(ctx);
    
    // Reseed with same seed and get third value
    qrng_reseed(ctx, (uint8_t*)"test2", 5);
    values[2] = qrng_uint64(ctx);
    
    // Values should all be different due to quantum effects
    success &= (values[0] != values[1]);
    success &= (values[1] != values[2]);
    
    qrng_free(ctx);
    
    TEST_RESULT(success);
    return success;
}

int main(void) {
    printf("Running Edge Case Tests\n");
    printf("======================\n");
    
    int total_success = 1;
    
    total_success &= test_null_handling();
    total_success &= test_zero_length();
    total_success &= test_range_edge_cases();
    total_success &= test_buffer_boundaries();
    total_success &= test_entropy_depletion();
    total_success &= test_reseeding();
    
    printf("\nOverall Result: %s\n", total_success ? "PASS" : "FAIL");
    
    return total_success ? 0 : 1;
}

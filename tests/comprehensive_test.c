#include "../src/quantum_rng/quantum_rng.h"
#include "statistical/statistical_tests.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define NUM_SAMPLES 10000000
#define TEST_SEED "comprehensive_test_seed"
#define EPSILON 1e-10

typedef struct {
    const char *name;
    int passed;
    const char *message;
} test_result_t;

// Test core functions
test_result_t test_init_free() {
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    if(err != QRNG_SUCCESS) {
        return (test_result_t){"Init/Free", 0, "Failed to initialize context"};
    }
    
    qrng_free(ctx);
    return (test_result_t){"Init/Free", 1, "Success"};
}

test_result_t test_reseed() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    uint64_t before = qrng_uint64(ctx);
    qrng_error err = qrng_reseed(ctx, (uint8_t*)"new_seed", 8);
    uint64_t after = qrng_uint64(ctx);
    
    qrng_free(ctx);
    
    if(err != QRNG_SUCCESS) {
        return (test_result_t){"Reseed", 0, "Reseed operation failed"};
    }
    
    if(before == after) {
        return (test_result_t){"Reseed", 0, "Output unchanged after reseed"};
    }
    
    return (test_result_t){"Reseed", 1, "Success"};
}

test_result_t test_bytes_generation() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    uint8_t buffer[1024];
    qrng_error err = qrng_bytes(ctx, buffer, sizeof(buffer));
    
    // Check for obvious patterns
    size_t zero_count = 0;  // Changed from int to size_t to match buffer size type
    for(size_t i = 0; i < sizeof(buffer); i++) {
        if(buffer[i] == 0) zero_count++;
    }
    
    qrng_free(ctx);
    
    if(err != QRNG_SUCCESS) {
        return (test_result_t){"Bytes Generation", 0, "Failed to generate bytes"};
    }
    
    if(zero_count > sizeof(buffer)/4) {
        return (test_result_t){"Bytes Generation", 0, "Too many zero bytes"};
    }
    
    return (test_result_t){"Bytes Generation", 1, "Success"};
}

test_result_t test_uint64_distribution() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    uint64_t buckets[16] = {0};
    const int num_samples = 1000000;
    
    for(int i = 0; i < num_samples; i++) {
        uint64_t val = qrng_uint64(ctx);
        buckets[val % 16]++;
    }
    
    // Chi-square test for uniformity
    double expected = num_samples / 16.0;
    double chi_square = 0;
    
    for(int i = 0; i < 16; i++) {
        double diff = buckets[i] - expected;
        chi_square += (diff * diff) / expected;
    }
    
    qrng_free(ctx);
    
    // Critical value for 15 degrees of freedom at 0.01 significance
    if(chi_square > 30.578) {
        return (test_result_t){"uint64 Distribution", 0, "Failed uniformity test"};
    }
    
    return (test_result_t){"uint64 Distribution", 1, "Success"};
}

test_result_t test_double_distribution() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    const int num_samples = 1000000;
    double sum = 0;
    double sum_squared = 0;
    double min_val = 1.0;
    double max_val = 0.0;
    
    for(int i = 0; i < num_samples; i++) {
        double val = qrng_double(ctx);
        sum += val;
        sum_squared += val * val;
        if(val < min_val) min_val = val;
        if(val > max_val) max_val = val;
        
        // Check range
        if(val < 0.0 || val >= 1.0) {
            qrng_free(ctx);
            return (test_result_t){"Double Distribution", 0, "Value outside [0,1)"};
        }
    }
    
    double mean = sum / num_samples;
    double variance = (sum_squared / num_samples) - (mean * mean);
    
    qrng_free(ctx);
    
    // Check statistics (should be close to uniform[0,1))
    if(fabs(mean - 0.5) > 0.01 || fabs(variance - 1.0/12.0) > 0.01) {
        return (test_result_t){"Double Distribution", 0, "Failed statistical tests"};
    }
    
    return (test_result_t){"Double Distribution", 1, "Success"};
}

test_result_t test_entanglement() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    uint8_t state1[32], state2[32];
    memset(state1, 0xAA, sizeof(state1));  // 10101010...
    memset(state2, 0x55, sizeof(state2));  // 01010101...
    
    qrng_error err = qrng_entangle_states(ctx, state1, state2, sizeof(state1));
    
    // Check that states are different but correlated
    int differences = 0;
    int correlations = 0;
    
    for(size_t i = 0; i < sizeof(state1); i++) {
        if(state1[i] != 0xAA) differences++;
        if((state1[i] ^ state2[i]) != 0xFF) correlations++;
    }
    
    qrng_free(ctx);
    
    if(err != QRNG_SUCCESS) {
        return (test_result_t){"Entanglement", 0, "Failed to entangle states"};
    }
    
    if(differences == 0) {
        return (test_result_t){"Entanglement", 0, "States unchanged"};
    }
    
    if(correlations == 0) {
        return (test_result_t){"Entanglement", 0, "No correlation between states"};
    }
    
    return (test_result_t){"Entanglement", 1, "Success"};
}

test_result_t test_error_handling() {
    qrng_ctx *ctx;
    uint8_t buffer[32];
    
    // Test null context
    if(qrng_bytes(NULL, buffer, sizeof(buffer)) != QRNG_ERROR_NULL_CONTEXT) {
        return (test_result_t){"Error Handling", 0, "Failed to catch null context"};
    }
    
    // Test null buffer
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    if(qrng_bytes(ctx, NULL, 32) != QRNG_ERROR_NULL_BUFFER) {
        qrng_free(ctx);
        return (test_result_t){"Error Handling", 0, "Failed to catch null buffer"};
    }
    
    // Test zero length
    if(qrng_bytes(ctx, buffer, 0) != QRNG_ERROR_INVALID_LENGTH) {
        qrng_free(ctx);
        return (test_result_t){"Error Handling", 0, "Failed to catch zero length"};
    }
    
    qrng_free(ctx);
    return (test_result_t){"Error Handling", 1, "Success"};
}

test_result_t test_entropy() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    // Generate some data to build entropy
    for(int i = 0; i < 1000; i++) {
        qrng_uint64(ctx);
    }
    
    double entropy = qrng_get_entropy_estimate(ctx);
    qrng_free(ctx);
    
    if(entropy < 0.0 || entropy > 64.0) {
        return (test_result_t){"Entropy", 0, "Entropy estimate out of range"};
    }
    
    return (test_result_t){"Entropy", 1, "Success"};
}

test_result_t test_memory_safety() {
    const int iterations = 1000;
    
    for(int i = 0; i < iterations; i++) {
        qrng_ctx *ctx;
        qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
        
        uint8_t buffer[1024];
        qrng_bytes(ctx, buffer, sizeof(buffer));
        qrng_uint64(ctx);
        qrng_double(ctx);
        
        qrng_free(ctx);
    }
    
    return (test_result_t){"Memory Safety", 1, "No obvious leaks"};
}

test_result_t test_state_measurement() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    uint8_t state[32];
    memset(state, 0xAA, sizeof(state));
    
    qrng_error err = qrng_measure_state(ctx, state, sizeof(state));
    
    // Check that measurement changed the state
    int changes = 0;
    for(size_t i = 0; i < sizeof(state); i++) {
        if(state[i] != 0xAA) changes++;
    }
    
    qrng_free(ctx);
    
    if(err != QRNG_SUCCESS) {
        return (test_result_t){"State Measurement", 0, "Measurement failed"};
    }
    
    if(changes == 0) {
        return (test_result_t){"State Measurement", 0, "State unchanged by measurement"};
    }
    
    return (test_result_t){"State Measurement", 1, "Success"};
}

test_result_t test_version_info() {
    const char *version = qrng_version();
    if(!version || strlen(version) == 0) {
        return (test_result_t){"Version Info", 0, "Invalid version string"};
    }
    
    // Check version format (x.y.z)
    int major, minor, patch;
    if(sscanf(version, "%d.%d.%d", &major, &minor, &patch) != 3) {
        return (test_result_t){"Version Info", 0, "Invalid version format"};
    }
    
    return (test_result_t){"Version Info", 1, "Success"};
}

void run_comprehensive_tests() {
    test_result_t tests[] = {
        test_init_free(),
        test_reseed(),
        test_bytes_generation(),
        test_uint64_distribution(),
        test_double_distribution(),
        test_entanglement(),
        test_error_handling(),
        test_entropy(),
        test_memory_safety(),
        test_state_measurement(),
        test_version_info()
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;
    
    printf("Running Comprehensive Tests\n");
    printf("==========================\n\n");
    
    for(int i = 0; i < num_tests; i++) {
        printf("Test: %s\n", tests[i].name);
        printf("Result: %s\n", tests[i].passed ? "PASS" : "FAIL");
        if(!tests[i].passed) {
            printf("Message: %s\n", tests[i].message);
        }
        printf("\n");
        
        if(tests[i].passed) passed++;
    }
    
    printf("Test Summary\n");
    printf("============\n");
    printf("Passed: %d/%d (%.1f%%)\n", 
           passed, num_tests, 
           100.0 * passed / num_tests);
    
    // Run statistical tests
    printf("\nRunning Statistical Tests\n");
    printf("========================\n");
    
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)TEST_SEED, strlen(TEST_SEED));
    
    statistical_results stats = run_statistical_suite(ctx, NUM_SAMPLES);
    print_statistical_results(&stats);
    
    qrng_free(ctx);
}

int main() {
    run_comprehensive_tests();
    return 0;
}

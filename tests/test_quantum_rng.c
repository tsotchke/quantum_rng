#include "../src/quantum_rng/quantum_rng.h"
#include "../tests/statistical/statistical_tests.h"
#include <stdio.h>
#include <time.h>

#define NUM_SAMPLES 10000000

void benchmark_quantum_rng(qrng_ctx *ctx) {
    clock_t start = clock();
    uint64_t sum = 0;  // Use sum instead of dummy XOR
    
    for(int i = 0; i < NUM_SAMPLES; i++) {
        sum += qrng_uint64(ctx);  // Actually use the numbers
    }
    
    double duration = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    printf("\nPerformance Metrics:\n");
    printf("Numbers/sec: %.2f M\n", NUM_SAMPLES / (duration * 1000000));
    printf("Throughput: %.2f MB/sec\n", (NUM_SAMPLES * sizeof(uint64_t)) / (duration * 1024 * 1024));
    
    // Use sum to prevent compiler optimization
    if (sum == 0) printf("(Extremely unlikely sum of %d random numbers is zero)\n", NUM_SAMPLES);
}

void test_quantum_properties(qrng_ctx *ctx) {
    uint64_t results[10] = {0};
    uint8_t seed[32] = {1};
    
    // Test quantum non-determinism
    for(int i = 0; i < 10; i++) {
        qrng_ctx *new_ctx;
        qrng_init(&new_ctx, seed, sizeof(seed));
        results[i] = qrng_uint64(new_ctx);
        qrng_free(new_ctx);
    }
    
    int matches = 0;
    for(int i = 1; i < 10; i++) {
        if(results[i] == results[0]) matches++;
    }
    
    printf("\nQuantum Properties:\n");
    printf("Non-determinism test: %d/9 unique values\n", 9 - matches);
    printf("Entropy estimate: %.6f bits\n", qrng_get_entropy_estimate(ctx));
}

int main() {
    qrng_ctx *ctx;
    uint8_t seed[32] = {1};
    qrng_error err;
    
    err = qrng_init(&ctx, seed, sizeof(seed));
    if(err != QRNG_SUCCESS) {
        printf("Initialization failed: %s\n", qrng_error_string(err));
        return 1;
    }
    
    printf("Quantum RNG Test Suite v%s\n", qrng_version());
    printf("==============================\n");
    
    // Run statistical tests
    statistical_results stats = run_statistical_suite(ctx, NUM_SAMPLES);
    print_statistical_results(&stats);
    
    // Performance benchmarks
    benchmark_quantum_rng(ctx);
    
    // Quantum property tests
    test_quantum_properties(ctx);
    
    qrng_free(ctx);
    return 0;
}

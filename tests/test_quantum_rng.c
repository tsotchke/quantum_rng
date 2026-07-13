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
    printf("Entropy estimate: %.6f bits/byte (max 8.0)\n", qrng_get_entropy_estimate(ctx));
}

/* Regression test for the qrng_get_entropy_estimate() contract:
 *   1. PURITY  -- calling the getter must NOT mutate the caller's RNG ctx.
 *   2. RANGE   -- the returned Shannon entropy is in [0, 8] bits/byte.
 * Returns 0 if the contract holds, 1 otherwise (gating).
 *
 * NOTE: the generator folds runtime entropy (time/pid) into the stream on
 * every draw, so two independently-seeded contexts do NOT produce identical
 * draws. Purity is therefore tested by snapshotting the live ctx, calling the
 * getter, and asserting the ctx is byte-identical afterwards -- NOT by comparing
 * its draws to a twin (the twin would draw fresh runtime entropy too). */
int test_entropy_contract(qrng_ctx *ctx) {
    printf("\nEntropy Getter Contract:\n");

    /* Advance the live stream a little, then snapshot. */
    (void)qrng_uint64(ctx);
    qrng_ctx snap;
    memcpy(&snap, ctx, sizeof(snap));

    double e = qrng_get_entropy_estimate(ctx);   /* must not mutate ctx */

    int purity_ok = (memcmp(&snap, ctx, sizeof(qrng_ctx)) == 0);
    printf("  stream unchanged by getter : %s\n", purity_ok ? "yes" : "NO");
    printf("  entropy = %.6f bits/byte  (range [0,8])\n", e);
    int range_ok = (e >= 0.0) && (e <= 8.0);
    printf("  range [0,8]              : %s\n", range_ok ? "yes" : "NO");

    if (!purity_ok || !range_ok) {
        printf("  RESULT: FAIL (entropy-getter contract violated)\n");
        return 1;
    }
    printf("  RESULT: PASS\n");
    return 0;
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

    // Entropy-getter contract (gating regression)
    int contract_fail = test_entropy_contract(ctx);

    qrng_free(ctx);
    return contract_fail ? 1 : 0;
}

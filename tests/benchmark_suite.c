#include "../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

#define BENCH_ITERATIONS 1000000
#define BULK_SIZE (1024 * 1024)  // 1MB
#define SMALL_BATCH_SIZE 64
#define ENTANGLE_SIZE 256
#define NUM_WARMUP_ITERATIONS 1000

typedef struct {
    double ops_per_sec;
    double mb_per_sec;
    double entropy_bits;
    double latency_us;
    double memory_mb;
} benchmark_result;

typedef struct {
    const char *name;
    double time_sec;
    double ops_per_sec;
    double bytes_per_sec;
    double avg_latency_us;
} function_benchmark;

// Memory tracking (basic implementation)
static size_t peak_memory = 0;
static size_t current_memory = 0;

void *tracked_malloc(size_t size) {
    void *ptr = malloc(size);
    if(ptr) {
        current_memory += size;
        if(current_memory > peak_memory) {
            peak_memory = current_memory;
        }
    }
    return ptr;
}

void tracked_free(void *ptr, size_t size) {
    free(ptr);
    current_memory -= size;
}

// Timing utilities
double get_time_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

// Individual function benchmarks
function_benchmark benchmark_uint64(qrng_ctx *ctx) {
    double start = get_time_us();
    volatile uint64_t dummy = 0;
    
    for(int i = 0; i < BENCH_ITERATIONS; i++) {
        dummy ^= qrng_uint64(ctx);
    }
    
    double time_us = get_time_us() - start;
    
    return (function_benchmark){
        .name = "uint64",
        .time_sec = time_us / 1000000.0,
        .ops_per_sec = BENCH_ITERATIONS / (time_us / 1000000.0),
        .bytes_per_sec = (BENCH_ITERATIONS * sizeof(uint64_t)) / (time_us / 1000000.0),
        .avg_latency_us = time_us / BENCH_ITERATIONS
    };
}

function_benchmark benchmark_double(qrng_ctx *ctx) {
    double start = get_time_us();
    volatile double dummy = 0;
    
    for(int i = 0; i < BENCH_ITERATIONS; i++) {
        dummy += qrng_double(ctx);
    }
    
    double time_us = get_time_us() - start;
    
    return (function_benchmark){
        .name = "double",
        .time_sec = time_us / 1000000.0,
        .ops_per_sec = BENCH_ITERATIONS / (time_us / 1000000.0),
        .bytes_per_sec = (BENCH_ITERATIONS * sizeof(double)) / (time_us / 1000000.0),
        .avg_latency_us = time_us / BENCH_ITERATIONS
    };
}

function_benchmark benchmark_bytes_bulk(qrng_ctx *ctx) {
    uint8_t *buffer = tracked_malloc(BULK_SIZE);
    double start = get_time_us();
    
    for(int i = 0; i < 100; i++) {
        qrng_bytes(ctx, buffer, BULK_SIZE);
    }
    
    double time_us = get_time_us() - start;
    
    tracked_free(buffer, BULK_SIZE);
    
    return (function_benchmark){
        .name = "bytes_bulk",
        .time_sec = time_us / 1000000.0,
        .ops_per_sec = 100 / (time_us / 1000000.0),
        .bytes_per_sec = (100.0 * BULK_SIZE) / (time_us / 1000000.0),
        .avg_latency_us = time_us / 100
    };
}

function_benchmark benchmark_bytes_small(qrng_ctx *ctx) {
    uint8_t buffer[SMALL_BATCH_SIZE];
    double start = get_time_us();
    
    for(int i = 0; i < BENCH_ITERATIONS; i++) {
        qrng_bytes(ctx, buffer, SMALL_BATCH_SIZE);
    }
    
    double time_us = get_time_us() - start;
    
    return (function_benchmark){
        .name = "bytes_small",
        .time_sec = time_us / 1000000.0,
        .ops_per_sec = BENCH_ITERATIONS / (time_us / 1000000.0),
        .bytes_per_sec = (BENCH_ITERATIONS * SMALL_BATCH_SIZE) / (time_us / 1000000.0),
        .avg_latency_us = time_us / BENCH_ITERATIONS
    };
}

function_benchmark benchmark_entanglement(qrng_ctx *ctx) {
    uint8_t *state1 = tracked_malloc(ENTANGLE_SIZE);
    uint8_t *state2 = tracked_malloc(ENTANGLE_SIZE);
    double start = get_time_us();
    
    for(int i = 0; i < BENCH_ITERATIONS; i++) {
        qrng_entangle_states(ctx, state1, state2, ENTANGLE_SIZE);
    }
    
    double time_us = get_time_us() - start;
    
    tracked_free(state1, ENTANGLE_SIZE);
    tracked_free(state2, ENTANGLE_SIZE);
    
    return (function_benchmark){
        .name = "entanglement",
        .time_sec = time_us / 1000000.0,
        .ops_per_sec = BENCH_ITERATIONS / (time_us / 1000000.0),
        .bytes_per_sec = (BENCH_ITERATIONS * ENTANGLE_SIZE * 2) / (time_us / 1000000.0),
        .avg_latency_us = time_us / BENCH_ITERATIONS
    };
}

// Run all benchmarks
benchmark_result run_benchmark_suite(qrng_ctx *ctx) {
    benchmark_result result = {0};
    function_benchmark benchmarks[5];
    
    // Reset memory tracking
    peak_memory = 0;
    current_memory = 0;
    
    // Run individual benchmarks
    benchmarks[0] = benchmark_uint64(ctx);
    benchmarks[1] = benchmark_double(ctx);
    benchmarks[2] = benchmark_bytes_bulk(ctx);
    benchmarks[3] = benchmark_bytes_small(ctx);
    benchmarks[4] = benchmark_entanglement(ctx);
    
    // Calculate aggregate results
    result.ops_per_sec = benchmarks[0].ops_per_sec;  // Use uint64 as reference
    result.mb_per_sec = benchmarks[2].bytes_per_sec / (1024.0 * 1024.0);
    result.entropy_bits = qrng_get_entropy_estimate(ctx);
    result.latency_us = benchmarks[0].avg_latency_us;
    result.memory_mb = peak_memory / (1024.0 * 1024.0);
    
    // Print detailed results
    printf("\nDetailed Function Benchmarks:\n");
    printf("---------------------------\n");
    for(int i = 0; i < 5; i++) {
        printf("\n%s:\n", benchmarks[i].name);
        printf("  Operations/sec: %.2f M\n", benchmarks[i].ops_per_sec / 1000000.0);
        printf("  Throughput: %.2f MB/sec\n", benchmarks[i].bytes_per_sec / (1024.0 * 1024.0));
        printf("  Latency: %.3f µs\n", benchmarks[i].avg_latency_us);
    }
    
    return result;
}

void print_benchmark_comparison(const char *label,
                              const benchmark_result *baseline,
                              const benchmark_result *optimized) {
    printf("\n%s:\n", label);
    printf("                     Baseline    Optimized    Improvement\n");
    printf("Operations/sec:     %7.2f M   %7.2f M    %7.2f%%\n",
           baseline->ops_per_sec / 1000000.0,
           optimized->ops_per_sec / 1000000.0,
           ((optimized->ops_per_sec / baseline->ops_per_sec) - 1.0) * 100.0);
    
    printf("Throughput:         %7.2f MB   %7.2f MB    %7.2f%%\n",
           baseline->mb_per_sec,
           optimized->mb_per_sec,
           ((optimized->mb_per_sec / baseline->mb_per_sec) - 1.0) * 100.0);
    
    printf("Latency:           %7.3f µs   %7.3f µs    %7.2f%%\n",
           baseline->latency_us,
           optimized->latency_us,
           ((baseline->latency_us / optimized->latency_us) - 1.0) * 100.0);
    
    printf("Entropy:           %7.3f bits %7.3f bits  %7.2f%%\n",
           baseline->entropy_bits,
           optimized->entropy_bits,
           ((optimized->entropy_bits / baseline->entropy_bits) - 1.0) * 100.0);
    
    printf("Peak Memory:       %7.2f MB   %7.2f MB    %7.2f%%\n",
           baseline->memory_mb,
           optimized->memory_mb,
           ((baseline->memory_mb / optimized->memory_mb) - 1.0) * 100.0);
}

int main() {
    qrng_ctx *ctx;
    uint8_t seed[32] = {1};
    benchmark_result baseline, optimized;
    
    printf("Quantum RNG Benchmark Suite\n");
    printf("==========================\n\n");
    
    printf("System Information:\n");
    printf("------------------\n");
    #ifdef __GNUC__
        printf("Compiler: GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    #endif
    printf("Optimization: %s\n", 
    #ifdef __OPTIMIZE__
        "enabled"
    #else
        "disabled"
    #endif
    );
    printf("QRNG Version: %s\n", qrng_version());
    
    // Baseline test (cold start)
    printf("\nRunning baseline benchmarks...\n");
    qrng_init(&ctx, seed, sizeof(seed));
    baseline = run_benchmark_suite(ctx);
    qrng_free(ctx);
    
    // Optimized test with warmed up state
    printf("\nRunning optimized benchmarks...\n");
    qrng_init(&ctx, seed, sizeof(seed));
    // Warm up
    for(int i = 0; i < NUM_WARMUP_ITERATIONS; i++) {
        qrng_uint64(ctx);
    }
    optimized = run_benchmark_suite(ctx);
    qrng_free(ctx);
    
    // Print comparison
    print_benchmark_comparison("Performance Comparison", &baseline, &optimized);
    
    return 0;
}

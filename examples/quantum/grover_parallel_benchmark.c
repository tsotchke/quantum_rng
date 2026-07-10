/**
 * @file grover_parallel_benchmark.c
 * @brief M2 Ultra 24-Core Parallel Grover Benchmark
 * 
 * Tests parallel Grover search implementation targeting 20-30x speedup
 * on Mac Studio M2 Ultra with 24 cores.
 * 
 * PERFORMANCE TARGET:
 * - Baseline: 0.135s for 16-qubit Grover (SIMD optimized)
 * - Target with 24-core parallel: 0.0045s (30x speedup)
 * 
 * This demonstrates Phase 1 of M2 Ultra extreme optimization.
 */

#include "../../src/quantum_rng/grover_parallel.h"
#include "../../src/quantum_rng/grover.h"
#include "../../src/quantum_rng/quantum_state.h"
#include "../../src/entropy/hardware_entropy.h"
#include "../../src/entropy/entropy_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void print_system_info(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     M2 ULTRA PARALLEL GROVER BENCHMARK                    ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║                                                           ║\n");
    printf("║  System Information:                                      ║\n");
    
#ifdef __APPLE__
    printf("║    Platform:            macOS (M2 Ultra optimized)        ║\n");
#else
    printf("║    Platform:            Other                             ║\n");
#endif
    
#ifdef _OPENMP
    printf("║    OpenMP:              ENABLED                           ║\n");
    size_t optimal_batch = grover_parallel_get_optimal_batch_size();
    printf("║    Available cores:     %6zu                            ║\n", optimal_batch);
#else
    printf("║    OpenMP:              DISABLED (sequential only)        ║\n");
    printf("║    Compile with -fopenmp for parallel execution!         ║\n");
#endif
    
    printf("║                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void benchmark_single_search(quantum_entropy_ctx_t *entropy) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  BASELINE: Single 16-Qubit Grover Search\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    quantum_state_t state;
    quantum_state_init(&state, 16);
    
    grover_config_t config = {
        .num_qubits = 16,
        .marked_state = 12345,
        .num_iterations = 0,
        .use_optimal_iterations = 1
    };
    
    // Warm up
    grover_search(&state, &config, entropy);
    
    // Measure
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    grover_result_t result = grover_search(&state, &config, entropy);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double time_taken = (end.tv_sec - start.tv_sec) + 
                       (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Search completed in %.6f seconds\n", time_taken);
    printf("Found state: %llu (target: %llu)\n", 
           result.found_state, config.marked_state);
    printf("Success: %s\n", result.found_marked_state ? "YES" : "NO");
    printf("Success probability: %.4f\n", result.success_probability);
    printf("Oracle calls: %zu\n", result.oracle_calls);
    printf("\n");
    
    quantum_state_free(&state);
}

void benchmark_parallel_batch(size_t batch_size, quantum_entropy_ctx_t *entropy) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  PARALLEL: %zu Concurrent 16-Qubit Searches\n", batch_size);
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    grover_parallel_result_t result = grover_parallel_random_batch(
        batch_size,
        16,
        entropy
    );
    
    if (!result.results) {
        printf("ERROR: Parallel batch failed!\n\n");
        return;
    }
    
    printf("Batch completed:\n");
    printf("  Total time:     %.6f seconds\n", result.total_time_seconds);
    printf("  Time per search: %.6f seconds\n", result.total_time_seconds / batch_size);
    printf("  Throughput:     %.2f searches/second\n", result.searches_per_second);
    
    // Calculate success rate
    size_t successes = 0;
    for (size_t i = 0; i < result.num_results; i++) {
        if (result.results[i].found_marked_state) {
            successes++;
        }
    }
    
    printf("  Success rate:   %.1f%% (%zu/%zu)\n",
           100.0 * successes / result.num_results,
           successes, result.num_results);
    printf("  Best probability: %.4f\n", result.best_result.success_probability);
    printf("\n");
    
    grover_parallel_free_result(&result);
}

void run_scaling_analysis(quantum_entropy_ctx_t *entropy) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  SCALING ANALYSIS: Speedup vs Core Count\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    size_t max_cores = grover_parallel_get_optimal_batch_size();
    size_t test_sizes[] = {1, 2, 4, 8, 12, 16, 24};
    size_t num_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    // Get baseline time (single search)
    quantum_state_t state;
    quantum_state_init(&state, 12);  // Use 12 qubits for faster testing
    
    grover_config_t config = {
        .num_qubits = 12,
        .marked_state = 100,
        .num_iterations = 0,
        .use_optimal_iterations = 1
    };
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    grover_search(&state, &config, entropy);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double baseline_time = (end.tv_sec - start.tv_sec) + 
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    
    quantum_state_free(&state);
    
    printf("Baseline (1 search): %.6f seconds\n\n", baseline_time);
    printf("Cores | Time (s)  | Speedup | Efficiency\n");
    printf("------+-----------+---------+-----------\n");
    
    for (size_t i = 0; i < num_tests; i++) {
        size_t cores = test_sizes[i];
        if (cores > max_cores) continue;
        
        grover_parallel_result_t result = grover_parallel_random_batch(
            cores,
            12,
            entropy
        );
        
        if (!result.results) continue;
        
        double speedup = (baseline_time * cores) / result.total_time_seconds;
        double efficiency = speedup / cores;
        
        printf("%5zu | %.7f | %6.2fx | %7.1f%%\n",
               cores,
               result.total_time_seconds,
               speedup,
               efficiency * 100.0);
        
        grover_parallel_free_result(&result);
    }
    
    printf("\n");
}

void run_full_benchmark(quantum_entropy_ctx_t *entropy) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  FULL BENCHMARK: Sequential vs Parallel\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    size_t num_searches = 24;
    size_t num_qubits = 16;
    
    grover_parallel_benchmark_t benchmark = grover_parallel_benchmark(
        num_searches,
        num_qubits,
        entropy
    );
    
    grover_parallel_print_benchmark(&benchmark);
    
    // Performance evaluation
    printf("PERFORMANCE ANALYSIS:\n");
    printf("─────────────────────────────────────────────────────────\n");
    
    if (benchmark.speedup >= 20.0) {
        printf("✓ EXCELLENT: Achieved target 20-30x speedup!\n");
        printf("  M2 Ultra 24-core parallelization working optimally.\n");
    } else if (benchmark.speedup >= 10.0) {
        printf("✓ GOOD: Strong multi-core performance (%.1fx speedup).\n", benchmark.speedup);
        printf("  Further optimization possible with tuning.\n");
    } else if (benchmark.speedup >= 5.0) {
        printf("⚠ MODERATE: Decent speedup (%.1fx) but below target.\n", benchmark.speedup);
        printf("  Check OpenMP configuration and system load.\n");
    } else {
        printf("✗ POOR: Insufficient parallelization (%.1fx speedup).\n", benchmark.speedup);
#ifndef _OPENMP
        printf("  ISSUE: OpenMP not enabled! Compile with -fopenmp\n");
#else
        printf("  ISSUE: Check thread contention or system resources.\n");
#endif
    }
    
    printf("\n");
}

int main(int argc, char **argv) {
    // Parse arguments
    int run_full = 0;
    int run_scaling = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--full") == 0) {
            run_full = 1;
        } else if (strcmp(argv[i], "--scaling") == 0) {
            run_scaling = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  --full      Run full sequential vs parallel benchmark\n");
            printf("  --scaling   Run scaling analysis across core counts\n");
            printf("  --help      Show this help message\n");
            printf("\nDefault: Run quick parallel batch benchmarks\n");
            return 0;
        }
    }
    
    print_system_info();
    
    // Initialize entropy pool
    entropy_pool_ctx_t *entropy_pool;
    entropy_pool_config_t pool_config = {
        .pool_size = 64 * 1024,
        .refill_threshold = 16 * 1024,
        .chunk_size = 4096,
        .enable_background_thread = 1,
        .min_entropy = 4.0
    };
    
    if (entropy_pool_init_with_config(&entropy_pool, &pool_config) != 0) {
        fprintf(stderr, "Failed to initialize entropy pool\n");
        return 1;
    }
    
    // Create entropy context
    quantum_entropy_ctx_t entropy_ctx;
    quantum_entropy_init(&entropy_ctx, 
                        (int (*)(void*, uint8_t*, size_t))entropy_pool_get_bytes,
                        entropy_pool);
    
    if (run_full) {
        // Full benchmark
        run_full_benchmark(&entropy_ctx);
    } else if (run_scaling) {
        // Scaling analysis
        run_scaling_analysis(&entropy_ctx);
    } else {
        // Quick benchmarks
        benchmark_single_search(&entropy_ctx);
        
        size_t batch_sizes[] = {4, 8, 12, 24};
        for (size_t i = 0; i < sizeof(batch_sizes) / sizeof(batch_sizes[0]); i++) {
            size_t batch = batch_sizes[i];
            if (batch <= grover_parallel_get_optimal_batch_size()) {
                benchmark_parallel_batch(batch, &entropy_ctx);
            }
        }
    }
    
    // Cleanup
    entropy_pool_free(entropy_pool);
    
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Benchmark Complete\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    return 0;
}
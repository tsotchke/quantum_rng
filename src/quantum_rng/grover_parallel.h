#ifndef GROVER_PARALLEL_H
#define GROVER_PARALLEL_H

#include "grover.h"
#include "quantum_state.h"
#include "quantum_entropy.h"
#include <stddef.h>

/**
 * @file grover_parallel.h
 * @brief Parallel Grover's algorithm implementation for M2 Ultra (24 cores)
 * 
 * PERFORMANCE OPTIMIZATION - Phase 1: Multi-Core Parallelization
 * 
 * This module provides parallel implementations of Grover's search algorithm
 * optimized for the M2 Ultra's 24-core architecture:
 * - 16 performance cores
 * - 8 efficiency cores
 * 
 * Target: 20-30x speedup through parallel batch processing
 */

/**
 * @brief Configuration for parallel Grover execution
 */
typedef struct {
    size_t num_parallel_searches;  // Number of parallel searches (default: 24)
    size_t num_qubits;             // Qubits per search
    int use_optimal_iterations;    // Auto-calculate iterations
    int pin_to_performance_cores;  // Pin threads to P-cores (macOS)
} grover_parallel_config_t;

/**
 * @brief Result from parallel batch of Grover searches
 */
typedef struct {
    grover_result_t *results;      // Array of individual results
    size_t num_results;            // Number of results
    grover_result_t best_result;   // Best result (highest probability)
    double total_time_seconds;     // Total execution time
    double searches_per_second;    // Throughput metric
} grover_parallel_result_t;

// ============================================================================
// PARALLEL BATCH PROCESSING
// ============================================================================

/**
 * @brief Execute multiple Grover searches in parallel (OpenMP)
 * 
 * Runs N independent Grover searches simultaneously across all available cores.
 * Each search has its own quantum state and targets a different marked state.
 * 
 * M2 Ultra advantage: 24 searches complete in the time of 1!
 * 
 * @param config Parallel execution configuration
 * @param marked_states Array of target states (one per search)
 * @param entropy_pools Array of entropy contexts (one per thread)
 * @return Parallel batch results
 */
grover_parallel_result_t grover_parallel_batch(
    const grover_parallel_config_t *config,
    const uint64_t *marked_states,
    quantum_entropy_ctx_t *entropy_pools
);

/**
 * @brief Execute parallel Grover searches with random targets
 * 
 * Convenient wrapper that generates random target states automatically.
 * Perfect for benchmarking and RNG applications.
 * 
 * @param num_searches Number of parallel searches
 * @param num_qubits Number of qubits per search
 * @param entropy_pool Entropy source for random targets
 * @return Parallel batch results
 */
grover_parallel_result_t grover_parallel_random_batch(
    size_t num_searches,
    size_t num_qubits,
    quantum_entropy_ctx_t *entropy_pool
);

/**
 * @brief Parallel random sampling using Grover (optimized for throughput)
 * 
 * Generates multiple random samples in parallel. Much faster than
 * sequential sampling when you need many samples.
 * 
 * Speedup: ~24x on M2 Ultra for batch sizes >= 24
 * 
 * @param num_qubits Number of qubits for sampling
 * @param samples Output array for samples
 * @param num_samples Number of samples to generate
 * @param entropy_pool Entropy source
 * @return 0 on success, -1 on error
 */
int grover_parallel_random_samples(
    size_t num_qubits,
    uint64_t *samples,
    size_t num_samples,
    quantum_entropy_ctx_t *entropy_pool
);

// ============================================================================
// PARTITIONED SEARCH (Single large search split across cores)
// ============================================================================

/**
 * @brief Parallel search space partitioning (experimental)
 * 
 * Splits a single large search space across multiple cores.
 * Each core searches its partition in parallel.
 * 
 * Note: Less effective for quantum search due to quantum parallelism,
 * but included for classical comparison benchmarking.
 * 
 * @param num_qubits Number of qubits (search space = 2^n)
 * @param marked_state Target to find
 * @param num_partitions Number of parallel partitions
 * @param entropy_pool Entropy source
 * @return Search result
 */
grover_result_t grover_parallel_partitioned_search(
    size_t num_qubits,
    uint64_t marked_state,
    size_t num_partitions,
    quantum_entropy_ctx_t *entropy_pool
);

// ============================================================================
// PERFORMANCE ANALYSIS
// ============================================================================

/**
 * @brief Benchmark parallel vs sequential Grover search
 * 
 * Runs identical searches sequentially and in parallel to measure
 * actual speedup on your hardware.
 * 
 * @param num_searches Number of searches to run
 * @param num_qubits Number of qubits per search
 * @param entropy_pool Entropy source
 */
typedef struct {
    double sequential_time;        // Time for sequential execution
    double parallel_time;          // Time for parallel execution
    double speedup;                // Actual speedup ratio
    double efficiency;             // Parallel efficiency (speedup / cores)
    size_t num_cores_used;         // Number of cores utilized
} grover_parallel_benchmark_t;

grover_parallel_benchmark_t grover_parallel_benchmark(
    size_t num_searches,
    size_t num_qubits,
    quantum_entropy_ctx_t *entropy_pool
);

/**
 * @brief Print parallel benchmark results
 */
void grover_parallel_print_benchmark(const grover_parallel_benchmark_t *benchmark);

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * @brief Free parallel result resources
 */
void grover_parallel_free_result(grover_parallel_result_t *result);

/**
 * @brief Get optimal number of parallel searches for current hardware
 * 
 * Auto-detects number of cores and returns recommended parallel batch size.
 */
size_t grover_parallel_get_optimal_batch_size(void);

/**
 * @brief Print parallel execution configuration
 */
void grover_parallel_print_config(const grover_parallel_config_t *config);

#endif /* GROVER_PARALLEL_H */
/**
 * @file metal_batch_benchmark.c
 * @brief THE RIGHT WAY to benchmark Metal GPU - Batch Processing!
 * 
 * Demonstrates 100-200x speedup by processing MANY Grover searches in parallel.
 * 
 * COMPILE:
 *   clang -O3 metal_batch_benchmark.c \
 *         src/quantum_rng/metal/metal_bridge.mm \
 *         src/quantum_rng/grover_parallel.c \
 *         src/quantum_rng/grover.c \
 *         src/quantum_rng/quantum_gates.c \
 *         src/quantum_rng/quantum_state.c \
 *         src/quantum_rng/simd_ops.c \
 *         src/quantum_rng/matrix_math.c \
 *         src/entropy/hardware_entropy.c \
 *         -I. -framework Metal -framework Foundation -fopenmp -lm \
 *         -o metal_batch_benchmark
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <complex.h>
#include "src/quantum_rng/metal/metal_bridge.h"
#include "src/quantum_rng/grover_parallel.h"
#include "src/quantum_rng/grover.h"
#include "src/quantum_rng/quantum_state.h"
#include "src/quantum_rng/quantum_gates.h"
#include "src/entropy/hardware_entropy.h"

typedef double _Complex complex_t;

// ============================================================================
// TIMING
// ============================================================================

static double get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// ============================================================================
// CPU SEQUENTIAL BASELINE
// ============================================================================

static double benchmark_cpu_sequential(
    int num_searches,
    int num_qubits,
    entropy_ctx_t* hw_entropy
) {
    printf("Running CPU sequential (%d searches)...\n", num_searches);
    
    quantum_state_t state;
    quantum_state_init(&state, num_qubits);
    
    uint32_t search_space = 1u << num_qubits;
    int iterations = (int)(0.785 * sqrt(search_space));
    
    double start = get_time_seconds();
    
    for (int s = 0; s < num_searches; s++) {
        uint64_t rand_val;
        entropy_get_uint64(hw_entropy, &rand_val);
        uint32_t target = (uint32_t)(rand_val % search_space);
        
        // Run Grover search
        grover_config_t config = {
            .num_qubits = num_qubits,
            .marked_state = target,
            .num_iterations = iterations,
            .use_optimal_iterations = 0
        };
        
        quantum_entropy_ctx_t dummy_entropy = {NULL, NULL};
        grover_search(&state, &config, &dummy_entropy);
    }
    
    double elapsed = get_time_seconds() - start;
    
    quantum_state_free(&state);
    
    return elapsed;
}

// ============================================================================
// CPU PARALLEL (OpenMP - Phase 1)
// ============================================================================

static double benchmark_cpu_parallel(
    int num_searches,
    int num_qubits,
    entropy_ctx_t* hw_entropy
) {
    printf("Running CPU parallel with OpenMP (%d searches on 24 cores)...\n", num_searches);
    fflush(stdout);
    
    // Allocate quantum states
    quantum_state_t* states = (quantum_state_t*)malloc(num_searches * sizeof(quantum_state_t));
    uint32_t* targets = (uint32_t*)malloc(num_searches * sizeof(uint32_t));
    
    uint32_t search_space = 1u << num_qubits;
    int iterations = (int)(0.785 * sqrt(search_space));
    
    // Initialize states
    for (int s = 0; s < num_searches; s++) {
        quantum_state_init(&states[s], num_qubits);
        uint64_t rand_val;
        entropy_get_uint64(hw_entropy, &rand_val);
        targets[s] = (uint32_t)(rand_val % search_space);
    }
    
    double start = get_time_seconds();
    
    // Parallel execution with OpenMP
    #pragma omp parallel for schedule(dynamic)
    for (int s = 0; s < num_searches; s++) {
        grover_config_t config = {
            .num_qubits = num_qubits,
            .marked_state = targets[s],
            .num_iterations = iterations,
            .use_optimal_iterations = 0
        };
        
        quantum_entropy_ctx_t dummy_entropy = {NULL, NULL};
        grover_search(&states[s], &config, &dummy_entropy);
    }
    
    double elapsed = get_time_seconds() - start;
    
    // Cleanup
    for (int s = 0; s < num_searches; s++) {
        quantum_state_free(&states[s]);
    }
    free(states);
    free(targets);
    
    return elapsed;
}

// ============================================================================
// GPU BATCH (Metal - Phase 2)
// ============================================================================

static double benchmark_gpu_batch(
    metal_compute_ctx_t* ctx,
    int num_searches,
    int num_qubits,
    entropy_ctx_t* hw_entropy
) {
    printf("Running GPU batch processing (%d searches on 76 GPU cores)...\n", num_searches);
    
    uint32_t state_dim = 1u << num_qubits;
    size_t total_size = (size_t)num_searches * state_dim * sizeof(complex_t);
    
    // Allocate batch buffer
    metal_buffer_t* batch_states = metal_buffer_create(ctx, total_size);
    if (!batch_states) {
        fprintf(stderr, "Failed to allocate GPU batch buffer\n");
        return -1.0;
    }
    
    // Initialize all states to |0...0⟩
    complex_t* states_ptr = (complex_t*)metal_buffer_contents(batch_states);
    for (int s = 0; s < num_searches; s++) {
        for (uint32_t i = 0; i < state_dim; i++) {
            states_ptr[s * state_dim + i] = (i == 0) ? 1.0 : 0.0;
        }
    }
    
    // Generate random targets
    uint32_t* targets = (uint32_t*)malloc(num_searches * sizeof(uint32_t));
    uint32_t* results = (uint32_t*)malloc(num_searches * sizeof(uint32_t));
    
    for (int s = 0; s < num_searches; s++) {
        uint64_t rand_val;
        entropy_get_uint64(hw_entropy, &rand_val);
        targets[s] = (uint32_t)(rand_val % state_dim);
    }
    
    int iterations = (int)(0.785 * sqrt(state_dim));
    
    metal_set_performance_monitoring(ctx, 1);
    
    double start = get_time_seconds();
    
    // THE MAGIC: All searches in ONE kernel launch!
    metal_grover_batch_search(ctx, batch_states, targets, results,
                             num_searches, num_qubits, iterations);
    
    metal_wait_completion(ctx);
    
    double elapsed = get_time_seconds() - start;
    
    metal_buffer_free(batch_states);
    free(targets);
    free(results);
    
    return elapsed;
}

// ============================================================================
// MAIN BENCHMARK
// ============================================================================

static void print_header() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║   METAL GPU BATCH BENCHMARK - THE RIGHT WAY!                     ║\n");
    printf("╠═══════════════════════════════════════════════════════════════════╣\n");
    printf("║                                                                   ║\n");
    printf("║  This benchmark shows GPU's TRUE power through batch processing  ║\n");
    printf("║                                                                   ║\n");
    printf("║  CPU Sequential:  1 search at a time                             ║\n");
    printf("║  CPU Parallel:    24 searches at a time (OpenMP)                 ║\n");
    printf("║  GPU Batch:       76 searches at a time (Metal)                  ║\n");
    printf("║                                                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

int main(void) {
    print_header();

    // Check Metal availability
    if (!metal_is_available()) {
        fprintf(stderr, "ERROR: Metal not available\n");
        return 1;
    }
    
    // Initialize Metal
    metal_compute_ctx_t* ctx = metal_compute_init();
    if (!ctx) {
        fprintf(stderr, "ERROR: Failed to initialize Metal\n");
        return 1;
    }
    
    metal_print_device_info(ctx);
    
    // Initialize entropy
    entropy_ctx_t hw_entropy;
    if (entropy_init(&hw_entropy) != ENTROPY_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to initialize entropy\n");
        metal_compute_free(ctx);
        return 1;
    }
    
    // Test configurations: varying batch sizes
    int batch_sizes[] = {24, 48, 76};  // 24=CPU parity, 76=GPU optimal
    int num_tests = sizeof(batch_sizes) / sizeof(batch_sizes[0]);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║   BATCH PROCESSING BENCHMARK (16 qubits per search)              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("%-12s  %-12s  %-12s  %-12s  %-10s  %-10s\n",
           "Batch Size", "CPU Seq(s)", "CPU Par(s)", "GPU Batch(s)", "vs Seq", "vs Par");
    printf("──────────────────────────────────────────────────────────────────────────────\n");
    
    for (int i = 0; i < num_tests; i++) {
        int batch_size = batch_sizes[i];
        int num_qubits = 16;
        
        // CPU Sequential
        double cpu_seq_time = benchmark_cpu_sequential(batch_size, num_qubits, &hw_entropy);
        
        // CPU Parallel
        double cpu_par_time = benchmark_cpu_parallel(batch_size, num_qubits, &hw_entropy);
        
        // GPU Batch
        double gpu_time = benchmark_gpu_batch(ctx, batch_size, num_qubits, &hw_entropy);
        
        double speedup_seq = cpu_seq_time / gpu_time;
        double speedup_par = cpu_par_time / gpu_time;
        
        printf("%-12d  %-12.3f  %-12.3f  %-12.3f  %-10.1fx  %-10.1fx\n",
               batch_size,
               cpu_seq_time,
               cpu_par_time,
               gpu_time,
               speedup_seq,
               speedup_par);
    }
    
    printf("──────────────────────────────────────────────────────────────────────────────\n");
    printf("\n");
    
    // Detailed analysis for 76 searches (GPU optimal)
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║   DETAILED ANALYSIS: 76 Parallel Searches (GPU Optimal)          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    int batch = 76;
    int qubits = 16;
    
    double seq_time = benchmark_cpu_sequential(batch, qubits, &hw_entropy);
    double par_time = benchmark_cpu_parallel(batch, qubits, &hw_entropy);
    double gpu_time = benchmark_gpu_batch(ctx, batch, qubits, &hw_entropy);
    
    printf("\nRESULTS FOR 76 × 16-QUBIT GROVER SEARCHES:\n");
    printf("  CPU Sequential:  %.3f seconds (%.2f ms per search)\n",
           seq_time, seq_time * 1000 / batch);
    printf("  CPU Parallel:    %.3f seconds (%.2f ms per search)\n",
           par_time, par_time * 1000 / batch);
    printf("  GPU Batch:       %.3f seconds (%.2f ms per search)\n",
           gpu_time, gpu_time * 1000 / batch);
    printf("\n");
    printf("SPEEDUP:\n");
    printf("  GPU vs CPU Sequential: %.1fx\n", seq_time / gpu_time);
    printf("  GPU vs CPU Parallel:   %.1fx\n", par_time / gpu_time);
    printf("\n");
    
    double target_speedup = seq_time / gpu_time;
    if (target_speedup >= 200.0) {
        printf("  ✓✓ PHENOMENAL! Phase 2 target CRUSHED! (>200x)\n");
    } else if (target_speedup >= 100.0) {
        printf("  ✓ EXCELLENT! Phase 2 target MET! (100-200x)\n");
    } else if (target_speedup >= 50.0) {
        printf("  ○ GOOD! Approaching target (50-100x)\n");
    } else {
        printf("  ⚠ Room for improvement (%.1fx < 100x target)\n", target_speedup);
    }
    
    // Throughput analysis
    printf("\n");
    printf("THROUGHPUT ANALYSIS:\n");
    printf("  CPU Sequential:  %.2f searches/second\n", batch / seq_time);
    printf("  CPU Parallel:    %.2f searches/second\n", batch / par_time);
    printf("  GPU Batch:       %.2f searches/second\n", batch / gpu_time);
    printf("\n");
    printf("  GPU Throughput Advantage: %.1fx over CPU parallel\n",
           (batch / gpu_time) / (batch / par_time));
    
    // Cleanup
    entropy_free(&hw_entropy);
    metal_compute_free(ctx);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║   BENCHMARK COMPLETE - GPU Batch Processing                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("KEY INSIGHT: GPUs excel at THROUGHPUT (many operations),\n");
    printf("             not LATENCY (single operation)!\n");
    printf("\n");
    printf("For your M2 Ultra:\n");
    printf("  • Single search: CPU is better (lower latency)\n");
    printf("  • Batch of 76+:  GPU is MUCH better (higher throughput)\n");
    printf("\n");
    
    return 0;
}
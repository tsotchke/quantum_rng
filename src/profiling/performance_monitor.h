#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file performance_monitor.h
 * @brief Real-time performance monitoring for Quantum RNG
 * 
 * Provides low-overhead (<1%) performance tracking with:
 * - High-resolution timing (CPU cycles or nanoseconds)
 * - Operation breakdowns
 * - Throughput measurement
 * - Latency distribution histograms
 */

// ============================================================================
// OPERATION TYPES
// ============================================================================

typedef enum {
    PERF_OP_ENTROPY_COLLECTION,
    PERF_OP_HEALTH_TEST,
    PERF_OP_QUANTUM_MIXING,
    PERF_OP_OUTPUT_GENERATION,
    PERF_OP_MAX
} perf_operation_t;

// ============================================================================
// CONTEXT
// ============================================================================

/**
 * @brief Performance monitoring context
 */
typedef struct {
    // Timing
    uint64_t start_time;            /**< Start timestamp (cycles) */
    uint64_t op_start_cycles;       /**< Current operation start */
    perf_operation_t current_op;    /**< Current operation type */
    
    // Totals
    uint64_t total_operations;      /**< Total operations monitored */
    uint64_t total_cycles;          /**< Total cycles elapsed */
    uint64_t bytes_processed;       /**< Total bytes processed */
    
    // Latency statistics
    uint64_t min_latency_cycles;    /**< Minimum latency */
    uint64_t max_latency_cycles;    /**< Maximum latency */
    
    // Per-operation breakdowns
    uint64_t entropy_collection_cycles;
    uint64_t health_test_cycles;
    uint64_t quantum_mixing_cycles;
    uint64_t output_generation_cycles;
    
    uint64_t entropy_operations;
    uint64_t health_operations;
    uint64_t quantum_operations;
    uint64_t output_operations;
    
    // Throughput
    double current_throughput_mbps;
    double peak_throughput_mbps;
    
    // Latency histogram (20 buckets, logarithmic)
    uint64_t latency_histogram[20];
    
    // CPU info
    double cpu_mhz;                 /**< CPU frequency in MHz */
} perf_monitor_ctx_t;

/**
 * @brief Performance statistics structure
 */
typedef struct {
    uint64_t total_operations;
    uint64_t bytes_processed;
    
    uint64_t avg_latency_cycles;
    uint64_t min_latency_cycles;
    uint64_t max_latency_cycles;
    
    double avg_latency_ns;
    double min_latency_ns;
    double max_latency_ns;
    
    double current_throughput_mbps;
    double peak_throughput_mbps;
    
    double entropy_percent;
    double health_percent;
    double quantum_percent;
    double output_percent;
} perf_stats_t;

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize performance monitor
 * 
 * @param ctx Output context pointer
 * @return 0 on success, -1 on error
 */
int perf_monitor_init(perf_monitor_ctx_t **ctx);

/**
 * @brief Free performance monitor
 * 
 * @param ctx Monitor context
 */
void perf_monitor_free(perf_monitor_ctx_t *ctx);

/**
 * @brief Reset performance statistics
 * 
 * @param ctx Monitor context
 */
void perf_monitor_reset(perf_monitor_ctx_t *ctx);

// ============================================================================
// TIMING OPERATIONS
// ============================================================================

/**
 * @brief Start timing an operation
 * 
 * @param ctx Monitor context
 * @param op Operation type
 */
void perf_monitor_start_operation(perf_monitor_ctx_t *ctx, perf_operation_t op);

/**
 * @brief End timing current operation
 * 
 * @param ctx Monitor context
 */
void perf_monitor_end_operation(perf_monitor_ctx_t *ctx);

/**
 * @brief Record bytes processed (for throughput calculation)
 * 
 * @param ctx Monitor context
 * @param bytes Number of bytes
 */
void perf_monitor_record_bytes(perf_monitor_ctx_t *ctx, size_t bytes);

// ============================================================================
// STATISTICS
// ============================================================================

/**
 * @brief Get performance statistics
 * 
 * @param ctx Monitor context
 * @param stats Output statistics structure
 */
void perf_monitor_get_stats(const perf_monitor_ctx_t *ctx, perf_stats_t *stats);

/**
 * @brief Print detailed performance statistics
 * 
 * @param ctx Monitor context
 */
void perf_monitor_print_stats(const perf_monitor_ctx_t *ctx);

/**
 * @brief Get monitoring overhead percentage
 * 
 * @param ctx Monitor context
 * @return Estimated overhead as percentage
 */
double perf_monitor_get_overhead_percent(const perf_monitor_ctx_t *ctx);

#endif /* PERFORMANCE_MONITOR_H */
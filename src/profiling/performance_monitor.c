#include "performance_monitor.h"
#include "../common/secure_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Platform-specific high-resolution timing
#ifdef __x86_64__
    static inline uint64_t get_cycles(void) {
        return __builtin_ia32_rdtsc();
    }
#elif defined(__aarch64__)
    static inline uint64_t get_cycles(void) {
        uint64_t val;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
        return val;
    }
#else
    #include <sys/time.h>
    static inline uint64_t get_cycles(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }
#endif

/**
 * @file performance_monitor.c
 * @brief Real-time performance monitoring for Quantum RNG
 * 
 * Provides low-overhead performance tracking with:
 * - Sub-microsecond timing precision
 * - Operation breakdowns
 * - Throughput measurement
 * - Latency histograms
 */

// ============================================================================
// CONTEXT MANAGEMENT
// ============================================================================

int perf_monitor_init(perf_monitor_ctx_t **ctx_out) {
    if (!ctx_out) return -1;
    
    perf_monitor_ctx_t *ctx = calloc(1, sizeof(perf_monitor_ctx_t));
    if (!ctx) return -1;
    
    // Initialize timing
    ctx->start_time = get_cycles();
    ctx->min_latency_cycles = UINT64_MAX;
    
    // Get CPU frequency for time conversion (approximate)
    #ifdef __linux__
        // Try to read from /proc/cpuinfo
        FILE *f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "cpu MHz", 7) == 0) {
                    sscanf(line, "cpu MHz : %lf", &ctx->cpu_mhz);
                    break;
                }
            }
            fclose(f);
        }
    #endif
    
    // Fallback: Assume 2.5 GHz (will be calibrated during runtime)
    if (ctx->cpu_mhz == 0.0) {
        ctx->cpu_mhz = 2500.0;
    }
    
    *ctx_out = ctx;
    return 0;
}

void perf_monitor_free(perf_monitor_ctx_t *ctx) {
    if (!ctx) return;
    secure_memzero(ctx, sizeof(*ctx));
    free(ctx);
}

void perf_monitor_reset(perf_monitor_ctx_t *ctx) {
    if (!ctx) return;
    
    // Save CPU frequency
    double saved_mhz = ctx->cpu_mhz;
    
    // Reset all stats
    secure_memzero(ctx, sizeof(*ctx));
    
    // Restore
    ctx->cpu_mhz = saved_mhz;
    ctx->start_time = get_cycles();
    ctx->min_latency_cycles = UINT64_MAX;
}

// ============================================================================
// TIMING OPERATIONS
// ============================================================================

void perf_monitor_start_operation(perf_monitor_ctx_t *ctx, perf_operation_t op) {
    if (!ctx || op >= PERF_OP_MAX) return;
    
    ctx->current_op = op;
    ctx->op_start_cycles = get_cycles();
}

void perf_monitor_end_operation(perf_monitor_ctx_t *ctx) {
    if (!ctx) return;
    
    uint64_t end_cycles = get_cycles();
    uint64_t elapsed = end_cycles - ctx->op_start_cycles;
    
    // Update totals
    ctx->total_operations++;
    ctx->total_cycles += elapsed;
    
    // Update operation-specific counters
    switch (ctx->current_op) {
        case PERF_OP_ENTROPY_COLLECTION:
            ctx->entropy_collection_cycles += elapsed;
            ctx->entropy_operations++;
            break;
        case PERF_OP_HEALTH_TEST:
            ctx->health_test_cycles += elapsed;
            ctx->health_operations++;
            break;
        case PERF_OP_QUANTUM_MIXING:
            ctx->quantum_mixing_cycles += elapsed;
            ctx->quantum_operations++;
            break;
        case PERF_OP_OUTPUT_GENERATION:
            ctx->output_generation_cycles += elapsed;
            ctx->output_operations++;
            break;
        default:
            break;
    }
    
    // Update min/max latency
    if (elapsed < ctx->min_latency_cycles) {
        ctx->min_latency_cycles = elapsed;
    }
    if (elapsed > ctx->max_latency_cycles) {
        ctx->max_latency_cycles = elapsed;
    }
    
    // Update latency histogram (20 buckets, logarithmic scale)
    int bucket = 0;
    uint64_t threshold = 1000;  // Start at 1000 cycles
    while (bucket < 19 && elapsed >= threshold) {
        bucket++;
        threshold *= 2;  // Logarithmic buckets
    }
    ctx->latency_histogram[bucket]++;
}

void perf_monitor_record_bytes(perf_monitor_ctx_t *ctx, size_t bytes) {
    if (!ctx) return;
    
    ctx->bytes_processed += bytes;
    
    // Calculate throughput
    uint64_t elapsed_cycles = get_cycles() - ctx->start_time;
    if (elapsed_cycles > 0) {
        // Convert cycles to seconds
        double elapsed_seconds = elapsed_cycles / (ctx->cpu_mhz * 1000000.0);
        if (elapsed_seconds > 0.0) {
            double mbytes = ctx->bytes_processed / (1024.0 * 1024.0);
            ctx->current_throughput_mbps = mbytes / elapsed_seconds;
            
            if (ctx->current_throughput_mbps > ctx->peak_throughput_mbps) {
                ctx->peak_throughput_mbps = ctx->current_throughput_mbps;
            }
        }
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

void perf_monitor_get_stats(const perf_monitor_ctx_t *ctx, perf_stats_t *stats) {
    if (!ctx || !stats) return;
    
    memset(stats, 0, sizeof(*stats));
    
    // Basic stats
    stats->total_operations = ctx->total_operations;
    stats->bytes_processed = ctx->bytes_processed;
    
    // Calculate average latency
    if (ctx->total_operations > 0) {
        stats->avg_latency_cycles = ctx->total_cycles / ctx->total_operations;
        stats->avg_latency_ns = (double)stats->avg_latency_cycles / ctx->cpu_mhz * 1000.0;
    }
    
    // Min/max latency
    stats->min_latency_cycles = ctx->min_latency_cycles;
    stats->max_latency_cycles = ctx->max_latency_cycles;
    stats->min_latency_ns = (double)stats->min_latency_cycles / ctx->cpu_mhz * 1000.0;
    stats->max_latency_ns = (double)stats->max_latency_cycles / ctx->cpu_mhz * 1000.0;
    
    // Throughput
    stats->current_throughput_mbps = ctx->current_throughput_mbps;
    stats->peak_throughput_mbps = ctx->peak_throughput_mbps;
    
    // Operation breakdowns
    stats->entropy_percent = ctx->total_cycles > 0 ?
        100.0 * ctx->entropy_collection_cycles / ctx->total_cycles : 0.0;
    stats->health_percent = ctx->total_cycles > 0 ?
        100.0 * ctx->health_test_cycles / ctx->total_cycles : 0.0;
    stats->quantum_percent = ctx->total_cycles > 0 ?
        100.0 * ctx->quantum_mixing_cycles / ctx->total_cycles : 0.0;
    stats->output_percent = ctx->total_cycles > 0 ?
        100.0 * ctx->output_generation_cycles / ctx->total_cycles : 0.0;
}

void perf_monitor_print_stats(const perf_monitor_ctx_t *ctx) {
    if (!ctx) return;
    
    perf_stats_t stats;
    perf_monitor_get_stats(ctx, &stats);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         PERFORMANCE MONITORING STATISTICS                 ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║                                                           ║\n");
    printf("║  Operations:                                              ║\n");
    printf("║    Total operations:    %10llu                         ║\n", 
           (unsigned long long)stats.total_operations);
    printf("║    Bytes processed:     %10llu                         ║\n",
           (unsigned long long)stats.bytes_processed);
    printf("║                                                           ║\n");
    printf("║  Latency (per operation):                                 ║\n");
    printf("║    Average:             %10.2f ns                      ║\n", stats.avg_latency_ns);
    printf("║    Minimum:             %10.2f ns                      ║\n", stats.min_latency_ns);
    printf("║    Maximum:             %10.2f ns                      ║\n", stats.max_latency_ns);
    printf("║                                                           ║\n");
    printf("║  Throughput:                                              ║\n");
    printf("║    Current:             %10.2f MB/s                    ║\n", stats.current_throughput_mbps);
    printf("║    Peak:                %10.2f MB/s                    ║\n", stats.peak_throughput_mbps);
    printf("║                                                           ║\n");
    printf("║  Time Distribution:                                       ║\n");
    printf("║    Entropy collection:  %10.1f%%                        ║\n", stats.entropy_percent);
    printf("║    Health testing:      %10.1f%%                        ║\n", stats.health_percent);
    printf("║    Quantum mixing:      %10.1f%%                        ║\n", stats.quantum_percent);
    printf("║    Output generation:   %10.1f%%                        ║\n", stats.output_percent);
    printf("║                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Print latency histogram
    printf("Latency Distribution:\n");
    printf("─────────────────────────────────────────────────────────\n");
    
    uint64_t threshold = 1000;
    for (int i = 0; i < 20; i++) {
        if (ctx->latency_histogram[i] > 0) {
            double ns = threshold / ctx->cpu_mhz * 1000.0;
            printf("  <%8.1f ns: %10llu ops\n",
                   ns, (unsigned long long)ctx->latency_histogram[i]);
        }
        threshold *= 2;
    }
    printf("\n");
}

double perf_monitor_get_overhead_percent(const perf_monitor_ctx_t *ctx) {
    if (!ctx || ctx->total_cycles == 0) return 0.0;
    
    // Estimate monitoring overhead (timing calls, bookkeeping)
    // Typically <1% for production code
    uint64_t monitoring_cycles = ctx->total_operations * 50;  // ~50 cycles per operation
    
    return 100.0 * monitoring_cycles / ctx->total_cycles;
}
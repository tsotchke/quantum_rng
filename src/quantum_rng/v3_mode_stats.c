/*
 * v3_mode_stats.c - mode control, statistics retrieval and pretty-printing
 */
#include "quantum_rng_v3.h"
#include "../common/validation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// MODE CONTROL
// ============================================================================

qrng_v3_error_t qrng_v3_set_mode(qrng_v3_ctx_t *ctx, qrng_v3_mode_t mode) {
    VALIDATE_NOT_NULL(ctx, QRNG_V3_ERROR_NULL_CONTEXT);
    
    ctx->config.mode = mode;
    return QRNG_V3_SUCCESS;
}

qrng_v3_mode_t qrng_v3_get_mode(const qrng_v3_ctx_t *ctx) {
    if (!ctx) return QRNG_V3_MODE_DIRECT;
    return ctx->config.mode;
}

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

qrng_v3_error_t qrng_v3_get_stats(
    const qrng_v3_ctx_t *ctx,
    qrng_v3_stats_t *stats
) {
    VALIDATE_NOT_NULL(ctx, QRNG_V3_ERROR_NULL_CONTEXT);
    VALIDATE_NOT_NULL(stats, QRNG_V3_ERROR_NULL_BUFFER);
    
    memcpy(stats, &ctx->stats, sizeof(*stats));
    
    // Calculate current entanglement
    stats->current_entanglement_entropy = qrng_v3_get_entanglement_entropy(ctx);
    
    return QRNG_V3_SUCCESS;
}

void qrng_v3_print_stats(const qrng_v3_ctx_t *ctx) {
    if (!ctx) return;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         QUANTUM RNG v3.0 STATISTICS                       ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║                                                           ║\n");
    printf("║  Generation:                                              ║\n");
    printf("║    Bytes generated:     %10llu                         ║\n",
           (unsigned long long)ctx->stats.bytes_generated);
    printf("║    Quantum measurements:%10llu                         ║\n",
           (unsigned long long)ctx->stats.quantum_measurements);
    printf("║    Grover searches:     %10llu                         ║\n",
           (unsigned long long)ctx->stats.grover_searches);
    printf("║                                                           ║\n");
    printf("║  Quantum Verification:                                    ║\n");
    printf("║    Bell tests performed:%10llu                         ║\n",
           (unsigned long long)ctx->stats.bell_tests_performed);
    printf("║    Bell tests passed:   %10llu                         ║\n",
           (unsigned long long)ctx->stats.bell_tests_passed);
    
    if (ctx->stats.bell_tests_performed > 0) {
        printf("║    Average CHSH:        %10.4f                         ║\n",
               ctx->stats.average_chsh);
        printf("║    Min CHSH:            %10.4f                         ║\n",
               ctx->stats.min_chsh);
        printf("║    Max CHSH:            %10.4f                         ║\n",
               ctx->stats.max_chsh);
        
        double pass_rate = 100.0 * ctx->stats.bell_tests_passed / ctx->stats.bell_tests_performed;
        printf("║    Pass rate:           %9.1f%%                        ║\n", pass_rate);
    }
    
    printf("║                                                           ║\n");
    printf("║  Quantum Properties:                                      ║\n");
    
    double ent_entropy = qrng_v3_get_entanglement_entropy(ctx);
    printf("║    Entanglement entropy:%10.4f bits                    ║\n", ent_entropy);
    printf("║    State entropy:       %10.4f bits                    ║\n",
           quantum_state_entropy(ctx->quantum_state));
    printf("║    Purity:              %10.4f                         ║\n",
           quantum_state_purity(ctx->quantum_state));
    
    printf("║                                                           ║\n");
    printf("║  Mode: %-50s ║\n", qrng_v3_mode_string(ctx->config.mode));
    printf("║  Quantum Verified: %-42s ║\n",
           qrng_v3_is_quantum_verified(ctx) ? "✓ YES" : "✗ NO");
    
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Print entropy pool stats
    if (ctx->entropy_pool) {
        entropy_pool_print_stats(ctx->entropy_pool);
    }
    
    // Print performance stats
    if (ctx->perf_monitor) {
        perf_monitor_print_stats(ctx->perf_monitor);
    }
}

const bell_test_monitor_t* qrng_v3_get_bell_history(const qrng_v3_ctx_t *ctx) {
    if (!ctx) return NULL;
    return ctx->bell_monitor;
}

/*
 * v3_verify.c - quantum verification: Bell-CHSH testing and entanglement entropy
 */
#include "quantum_rng_v3.h"
#include "../common/validation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

bell_test_result_t qrng_v3_verify_quantum(
    qrng_v3_ctx_t *ctx,
    size_t num_measurements
) {
    bell_test_result_t result = {0};
    
    if (!ctx || !ctx->initialized || !ctx->quantum_state) {
        return result;
    }
    
    // Run Bell test on current quantum state
    result = bell_test_chsh(
        ctx->quantum_state,
        0,  // Qubit A
        1,  // Qubit B
        num_measurements,
        NULL,  // Use optimal settings
        &ctx->entropy_ctx
    );
    
    // Update statistics
    if (result.chsh_value > ctx->stats.max_chsh) {
        ctx->stats.max_chsh = result.chsh_value;
    }
    if (ctx->stats.min_chsh == 0.0 || result.chsh_value < ctx->stats.min_chsh) {
        ctx->stats.min_chsh = result.chsh_value;
    }
    
    // Update running average
    double total = ctx->stats.average_chsh * ctx->stats.bell_tests_performed;
    total += result.chsh_value;
    ctx->stats.bell_tests_performed++;
    ctx->stats.average_chsh = total / ctx->stats.bell_tests_performed;
    
    return result;
}

double qrng_v3_get_entanglement_entropy(const qrng_v3_ctx_t *ctx) {
    if (!ctx || !ctx->quantum_state) return 0.0;
    
    // Calculate entanglement between first half and second half of qubits
    size_t half = ctx->config.num_qubits / 2;
    if (half == 0) return 0.0;
    
    int *subsystem_a = calloc(half, sizeof(int));
    if (!subsystem_a) return 0.0;
    
    for (size_t i = 0; i < half; i++) {
        subsystem_a[i] = i;
    }
    
    double entropy = quantum_state_entanglement_entropy(
        ctx->quantum_state,
        subsystem_a,
        half
    );
    
    free(subsystem_a);
    
    // Numerical safety: entanglement entropy must be non-negative
    if (entropy < 0.0 || isnan(entropy)) {
        entropy = 0.0;  // Physically valid lower bound
    }
    
    return entropy;
}

int qrng_v3_is_quantum_verified(const qrng_v3_ctx_t *ctx) {
    if (!ctx || !ctx->bell_monitor) return 0;
    
    // Check if recent Bell tests confirm quantum behavior
    if (ctx->stats.bell_tests_performed == 0) return 0;
    
    // At least 80% of tests should pass
    double pass_rate = (double)ctx->stats.bell_tests_passed / ctx->stats.bell_tests_performed;
    if (pass_rate < 0.8) return 0;
    
    // Average CHSH should be above classical bound
    if (ctx->stats.average_chsh <= 2.0) return 0;
    
    return 1;
}

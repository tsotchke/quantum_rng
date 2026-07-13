/*
 * v3_init.c - context lifecycle: entropy callback, init, free
 *
 * Resolves the entropy circular dependency through layered design:
 *   Layer 1 (base):  hardware entropy pool
 *   Layer 2 (quantum): quantum simulation, fed by Layer 1
 *   Layer 3 (output):  conditioned random output
 */
#include "quantum_rng_v3.h"
#include "quantum_constants.h"
#include "simd_ops.h"
#include "../common/secure_memory.h"
#include "../common/validation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/*
 * @brief Entropy callback for quantum measurements
 *
 * Provides hardware entropy to the quantum simulation layer.
 * Key: Uses entropy_pool (Layer 1) for quantum measurements (Layer 2).
 * No circular dependency because entropy_pool doesn't use quantum simulation.
 */
V3_INTERNAL int qrng_v3_entropy_pool_callback(void *user_data, uint8_t *buffer, size_t size) {
    entropy_pool_ctx_t *pool = (entropy_pool_ctx_t *)user_data;
    if (!pool) return -1;
    
    return entropy_pool_get_bytes(pool, buffer, size);
}

qrng_v3_error_t qrng_v3_init(qrng_v3_ctx_t **ctx_out) {
    qrng_v3_config_t config;
    qrng_v3_get_default_config(&config);
    return qrng_v3_init_with_config(ctx_out, &config);
}

qrng_v3_error_t qrng_v3_init_with_config(
    qrng_v3_ctx_t **ctx_out,
    const qrng_v3_config_t *config
) {
    VALIDATE_NOT_NULL(ctx_out, QRNG_V3_ERROR_NULL_CONTEXT);
    VALIDATE_NOT_NULL(config, QRNG_V3_ERROR_INVALID_PARAM);
    
    // Validate configuration
    if (config->num_qubits < 2 || config->num_qubits > MAX_QUBITS) {
        return QRNG_V3_ERROR_INVALID_PARAM;
    }
    
    // Allocate context
    qrng_v3_ctx_t *ctx = calloc(1, sizeof(qrng_v3_ctx_t));
    if (!ctx) return QRNG_V3_ERROR_OUT_OF_MEMORY;
    
    // Copy configuration
    memcpy(&ctx->config, config, sizeof(qrng_v3_config_t));
    
    // LAYER 1: Initialize hardware entropy pool (base layer)
    entropy_pool_config_t pool_config = {
        .pool_size = config->entropy_pool_size,
        .refill_threshold = config->entropy_pool_size / 4,
        .chunk_size = 4096,
        .enable_background_thread = config->enable_background_entropy,
        .min_entropy = 4.0
    };
    
    int pool_err = entropy_pool_init_with_config(&ctx->entropy_pool, &pool_config);
    if (pool_err != 0) {
        free(ctx);
        return QRNG_V3_ERROR_ENTROPY_FAILURE;
    }
    
    // LAYER 2: Initialize quantum simulation engine
    ctx->quantum_state = calloc(1, sizeof(quantum_state_t));
    if (!ctx->quantum_state) {
        entropy_pool_free(ctx->entropy_pool);
        free(ctx);
        return QRNG_V3_ERROR_OUT_OF_MEMORY;
    }
    
    qs_error_t qs_err = quantum_state_init(ctx->quantum_state, config->num_qubits);
    if (qs_err != QS_SUCCESS) {
        free(ctx->quantum_state);
        entropy_pool_free(ctx->entropy_pool);
        free(ctx);
        return QRNG_V3_ERROR_QUANTUM_INIT;
    }
    
    // Connect entropy pool to quantum measurements (resolves circular dependency!)
    quantum_entropy_init(
        &ctx->entropy_ctx,
        qrng_v3_entropy_pool_callback,
        ctx->entropy_pool
    );
    
    // LAYER 3: Initialize output buffer (larger = better performance)
    ctx->output_buffer_size = config->output_buffer_size > 0 ?
                               config->output_buffer_size : 65536;  // Default 64KB
    ctx->output_buffer = calloc(1, ctx->output_buffer_size);
    if (!ctx->output_buffer) {
        quantum_state_free(ctx->quantum_state);
        free(ctx->quantum_state);
        entropy_pool_free(ctx->entropy_pool);
        free(ctx);
        return QRNG_V3_ERROR_OUT_OF_MEMORY;
    }
    ctx->buffer_pos = ctx->output_buffer_size;  // Force initial fill
    
    // Initialize Bell test monitoring
    if (config->enable_bell_monitoring) {
        ctx->bell_monitor = calloc(1, sizeof(bell_test_monitor_t));
        if (ctx->bell_monitor) {
            bell_monitor_init(ctx->bell_monitor, 100);  // Track last 100 tests
        }
    }
    
    // Initialize Grover cache
    if (config->enable_grover_cache && config->grover_cache_size > 0) {
        ctx->grover_cache = calloc(config->grover_cache_size, sizeof(uint64_t));
    }
    
    // Initialize performance monitoring
    if (config->enable_performance_monitoring) {
        perf_monitor_init(&ctx->perf_monitor);
    }
    
    ctx->initialized = 1;
    *ctx_out = ctx;
    
    return QRNG_V3_SUCCESS;
}

void qrng_v3_free(qrng_v3_ctx_t *ctx) {
    if (!ctx) return;
    
    // Free quantum state
    if (ctx->quantum_state) {
        quantum_state_free(ctx->quantum_state);
        free(ctx->quantum_state);
    }
    
    // Free entropy pool
    if (ctx->entropy_pool) {
        entropy_pool_free(ctx->entropy_pool);
    }
    
    // Free Bell monitor
    if (ctx->bell_monitor) {
        bell_monitor_free(ctx->bell_monitor);
        free(ctx->bell_monitor);
    }
    
    // Free output buffer
    if (ctx->output_buffer) {
        secure_memzero(ctx->output_buffer, ctx->output_buffer_size);
        free(ctx->output_buffer);
    }
    
    // Free Grover cache
    if (ctx->grover_cache) {
        secure_memzero(ctx->grover_cache, ctx->config.grover_cache_size * sizeof(uint64_t));
        free(ctx->grover_cache);
    }
    
    // Free performance monitor
    if (ctx->perf_monitor) {
        perf_monitor_free(ctx->perf_monitor);
    }
    
    // Zero context
    secure_memzero(ctx, sizeof(*ctx));
    free(ctx);
}

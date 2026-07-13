/*
 * v3_generation.c - random number output generation (bytes / uint64 / double / range)
 */
#include "quantum_rng_v3.h"
#include "v3_internal.h"
#include "../common/validation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

qrng_v3_error_t qrng_v3_bytes(
    qrng_v3_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
) {
    VALIDATE_NOT_NULL(ctx, QRNG_V3_ERROR_NULL_CONTEXT);
    VALIDATE_BUFFER(buffer, size, QRNG_V3_ERROR_NULL_BUFFER);
    
    if (!ctx->initialized) {
        return QRNG_V3_ERROR_NOT_INITIALIZED;
    }
    
    // Performance monitoring
    if (ctx->perf_monitor) {
        perf_monitor_start_operation(ctx->perf_monitor, PERF_OP_OUTPUT_GENERATION);
    }
    
    size_t bytes_copied = 0;
    
    while (bytes_copied < size) {
        // Refill buffer if needed
        if (ctx->buffer_pos >= ctx->output_buffer_size) {
            // Generate fresh quantum entropy
            int err;
            
            switch (ctx->config.mode) {
                case QRNG_V3_MODE_DIRECT:
                    err = qrng_v3_extract_quantum_entropy(ctx, ctx->output_buffer, ctx->output_buffer_size);
                    break;
                    
                case QRNG_V3_MODE_GROVER:
                    // Grover-based entropy extraction
                    err = qrng_v3_extract_grover_entropy(ctx, ctx->output_buffer, ctx->output_buffer_size);
                    break;
                    
                case QRNG_V3_MODE_BELL_VERIFIED:
                    err = qrng_v3_extract_quantum_entropy(ctx, ctx->output_buffer, ctx->output_buffer_size);
                    break;
                    
                default:
                    return QRNG_V3_ERROR_INVALID_PARAM;
            }
            
            if (err != 0) {
                if (ctx->perf_monitor) {
                    perf_monitor_end_operation(ctx->perf_monitor);
                }
                return QRNG_V3_ERROR_ENTROPY_FAILURE;
            }
            
            ctx->buffer_pos = 0;
        }
        
        // Copy from buffer
        size_t copy_size = ctx->output_buffer_size - ctx->buffer_pos;
        if (copy_size > size - bytes_copied) {
            copy_size = size - bytes_copied;
        }
        
        memcpy(buffer + bytes_copied, ctx->output_buffer + ctx->buffer_pos, copy_size);
        ctx->buffer_pos += copy_size;
        bytes_copied += copy_size;
    }
    
    // Update statistics
    ctx->stats.bytes_generated += size;
    ctx->bytes_since_bell_test += size;
    
    // Bell test monitoring
    if (ctx->config.enable_bell_monitoring && 
        ctx->config.bell_test_interval > 0 &&
        ctx->bytes_since_bell_test >= ctx->config.bell_test_interval) {
        
        // Use enough measurements that the CHSH estimate is statistically
        // tight (SE ~ 2/sqrt(N)); a small N made monitored generation
        // spuriously trip the min-CHSH check on an unlucky draw.
        bell_test_result_t result = qrng_v3_verify_quantum(ctx, 4000);

        if (ctx->bell_monitor) {
            bell_monitor_add_result(ctx->bell_monitor, &result);
        }
        
        ctx->stats.bell_tests_performed++;
        if (bell_test_confirms_quantum(&result)) {
            ctx->stats.bell_tests_passed++;
        }
        
        // Check if quantum behavior is maintained
        if (result.chsh_value < ctx->config.min_acceptable_chsh) {
            // Quantum behavior degraded - this shouldn't happen in simulation
            // but good to check
            return QRNG_V3_ERROR_BELL_TEST_FAILED;
        }
        
        ctx->bytes_since_bell_test = 0;
    }
    
    if (ctx->perf_monitor) {
        perf_monitor_end_operation(ctx->perf_monitor);
        perf_monitor_record_bytes(ctx->perf_monitor, size);
    }
    
    return QRNG_V3_SUCCESS;
}

qrng_v3_error_t qrng_v3_uint64(qrng_v3_ctx_t *ctx, uint64_t *value) {
    VALIDATE_NOT_NULL(ctx, QRNG_V3_ERROR_NULL_CONTEXT);
    VALIDATE_NOT_NULL(value, QRNG_V3_ERROR_NULL_BUFFER);
    
    return qrng_v3_bytes(ctx, (uint8_t*)value, sizeof(*value));
}

qrng_v3_error_t qrng_v3_double(qrng_v3_ctx_t *ctx, double *value) {
    VALIDATE_NOT_NULL(ctx, QRNG_V3_ERROR_NULL_CONTEXT);
    VALIDATE_NOT_NULL(value, QRNG_V3_ERROR_NULL_BUFFER);
    
    uint64_t random_bits;
    qrng_v3_error_t err = qrng_v3_uint64(ctx, &random_bits);
    if (err != QRNG_V3_SUCCESS) return err;
    
    // Convert to double in [0, 1) using 53 bits of precision
    *value = (double)(random_bits >> 11) * 0x1.0p-53;
    
    return QRNG_V3_SUCCESS;
}

qrng_v3_error_t qrng_v3_range(
    qrng_v3_ctx_t *ctx,
    uint64_t min,
    uint64_t max,
    uint64_t *value
) {
    VALIDATE_NOT_NULL(ctx, QRNG_V3_ERROR_NULL_CONTEXT);
    VALIDATE_NOT_NULL(value, QRNG_V3_ERROR_NULL_BUFFER);
    
    if (min > max) return QRNG_V3_ERROR_INVALID_PARAM;
    if (min == max) {
        *value = min;
        return QRNG_V3_SUCCESS;
    }
    
    uint64_t range = max - min + 1;
    if (range == 0) {  // Overflow case
        qrng_v3_uint64(ctx, value);
        return QRNG_V3_SUCCESS;
    }
    
    // Unbiased sampling
    uint64_t threshold = (UINT64_MAX - range + 1) % range;
    uint64_t r;
    
    do {
        qrng_v3_error_t err = qrng_v3_uint64(ctx, &r);
        if (err != QRNG_V3_SUCCESS) return err;
    } while (r < threshold);
    
    *value = min + (r % range);
    return QRNG_V3_SUCCESS;
}

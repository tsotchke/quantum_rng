/*
 * secure_generate.c - random output generation (bytes / uint64 / uint32 /
 * double / range32 / range64) and VERIFIED-mode Bell certification gating.
 */
#include "secure_internal.h"

secure_rng_error_t secure_rng_bytes(
    secure_rng_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!buffer || size == 0) return SECURE_RNG_ERROR_NULL_BUFFER;
    
    secure_rng_error_t lock_err = lock_write(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        unlock(ctx);
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    secure_rng_mode_t effective_mode = ctx->config.mode;
    if (effective_mode == SECURE_RNG_MODE_HYBRID) {
        effective_mode = (size < ctx->config.hybrid_threshold) ?
                         SECURE_RNG_MODE_FAST : SECURE_RNG_MODE_QUANTUM;
    }

    if (reseed_needed(ctx)) {
        secure_rng_error_t err = secure_rng_reseed(ctx);
        if (err != SECURE_RNG_SUCCESS) {
            if (ctx->config.zeroize_on_error) {
                secure_memzero(buffer, size);
            }
            unlock(ctx);
            return err;
        }
    }

    secure_rng_error_t result = SECURE_RNG_SUCCESS;
    
    switch (effective_mode) {
        case SECURE_RNG_MODE_FAST:
            result = collect_tested_entropy(ctx, buffer, size);
            if (result == SECURE_RNG_SUCCESS) {
                ctx->stats.fast_mode_bytes += size;
            }
            break;
            
        case SECURE_RNG_MODE_QUANTUM:
            {
                qrng_error qrng_err = qrng_bytes(ctx->qrng_ctx, buffer, size);
                if (qrng_err != QRNG_SUCCESS) {
                    if (ctx->config.zeroize_on_error) {
                        secure_memzero(buffer, size);
                    }
                    result = SECURE_RNG_ERROR_INITIALIZATION;
                } else {
                    ctx->stats.quantum_mode_bytes += size;
                }
            }
            break;
            
        case SECURE_RNG_MODE_VERIFIED:
            {
                if (!ctx->bell_certified) {
                    secure_rng_error_t cert = run_bell_certification(ctx);
                    if (cert != SECURE_RNG_SUCCESS) {
                        ctx->state = SECURE_RNG_STATE_ERROR;
                        ctx->stats.health_test_failures++;
                        if (ctx->config.zeroize_on_error) {
                            secure_memzero(buffer, size);
                        }
                        result = cert;
                        break;
                    }
                }
                qrng_error qrng_err = qrng_bytes(ctx->qrng_ctx, buffer, size);
                if (qrng_err != QRNG_SUCCESS) {
                    if (ctx->config.zeroize_on_error) {
                        secure_memzero(buffer, size);
                    }
                    result = SECURE_RNG_ERROR_INITIALIZATION;
                } else {
                    ctx->stats.verified_mode_bytes += size;
                }
            }
            break;
            
        case SECURE_RNG_MODE_HYBRID:
            result = SECURE_RNG_ERROR_INVALID_PARAM;
            break;
    }

    if (result == SECURE_RNG_SUCCESS) {
        ctx->stats.bytes_generated += size;
        ctx->stats.requests_served++;
        ctx->bytes_since_reseed += size;
    }

    unlock(ctx);
    return result;
}

secure_rng_error_t secure_rng_uint64(secure_rng_ctx_t *ctx, uint64_t *value) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!value) return SECURE_RNG_ERROR_NULL_BUFFER;

    return secure_rng_bytes(ctx, (uint8_t*)value, sizeof(*value));
}

secure_rng_error_t secure_rng_uint32(secure_rng_ctx_t *ctx, uint32_t *value) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!value) return SECURE_RNG_ERROR_NULL_BUFFER;

    return secure_rng_bytes(ctx, (uint8_t*)value, sizeof(*value));
}

secure_rng_error_t secure_rng_double(secure_rng_ctx_t *ctx, double *value) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!value) return SECURE_RNG_ERROR_NULL_BUFFER;
    
    secure_rng_error_t lock_err = lock_write(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        unlock(ctx);
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    *value = qrng_double(ctx->qrng_ctx);
    ctx->stats.requests_served++;

    unlock(ctx);
    return SECURE_RNG_SUCCESS;
}

secure_rng_error_t secure_rng_range32(
    secure_rng_ctx_t *ctx,
    int32_t min,
    int32_t max,
    int32_t *value
) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!value) return SECURE_RNG_ERROR_NULL_BUFFER;
    if (min > max) return SECURE_RNG_ERROR_INVALID_RANGE;
    
    secure_rng_error_t lock_err = lock_write(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        unlock(ctx);
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    *value = qrng_range32(ctx->qrng_ctx, min, max);
    ctx->stats.requests_served++;

    unlock(ctx);
    return SECURE_RNG_SUCCESS;
}

secure_rng_error_t secure_rng_range64(
    secure_rng_ctx_t *ctx,
    uint64_t min,
    uint64_t max,
    uint64_t *value
) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!value) return SECURE_RNG_ERROR_NULL_BUFFER;
    if (min > max) return SECURE_RNG_ERROR_INVALID_RANGE;
    
    secure_rng_error_t lock_err = lock_write(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        unlock(ctx);
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    *value = qrng_range64(ctx->qrng_ctx, min, max);
    ctx->stats.requests_served++;

    unlock(ctx);
    return SECURE_RNG_SUCCESS;
}

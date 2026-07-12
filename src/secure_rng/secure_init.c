/*
 * secure_init.c - lifecycle: thread-safety locks, entropy/health helpers,
 * configuration, init family, reset and reseed.
 */
#include "secure_internal.h"

// ============================================================================
// THREAD SAFETY HELPERS
// ============================================================================

secure_rng_error_t lock_write(secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!ctx->thread_safe) return SECURE_RNG_SUCCESS;
    
    if (pthread_rwlock_wrlock(&ctx->rwlock) != 0) {
        return SECURE_RNG_ERROR_MUTEX_LOCK;
    }
    return SECURE_RNG_SUCCESS;
}

secure_rng_error_t lock_read(const secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!ctx->thread_safe) return SECURE_RNG_SUCCESS;
    
    secure_rng_ctx_t *mutable_ctx = (secure_rng_ctx_t *)ctx;
    if (pthread_rwlock_rdlock(&mutable_ctx->rwlock) != 0) {
        return SECURE_RNG_ERROR_MUTEX_LOCK;
    }
    return SECURE_RNG_SUCCESS;
}

secure_rng_error_t unlock(const secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!ctx->thread_safe) return SECURE_RNG_SUCCESS;
    
    secure_rng_ctx_t *mutable_ctx = (secure_rng_ctx_t *)ctx;
    if (pthread_rwlock_unlock(&mutable_ctx->rwlock) != 0) {
        return SECURE_RNG_ERROR_MUTEX_UNLOCK;
    }
    return SECURE_RNG_SUCCESS;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void invoke_error_callback(
    secure_rng_ctx_t *ctx,
    secure_rng_error_t error,
    const char *message
) {
    if (ctx && ctx->error_callback) {
        ctx->error_callback(error, message, ctx->callback_user_data);
    }
}

secure_rng_error_t collect_tested_entropy(
    secure_rng_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
) {
    if (!ctx || !buffer || size == 0) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }

    entropy_error_t entropy_err = entropy_get_bytes(ctx->entropy_ctx, buffer, size);
    if (entropy_err != ENTROPY_SUCCESS) {
        invoke_error_callback(ctx, SECURE_RNG_ERROR_ENTROPY_FAILURE,
                             "Failed to collect entropy from hardware sources");
        return SECURE_RNG_ERROR_ENTROPY_FAILURE;
    }

    for (size_t i = 0; i < size; i++) {
        health_error_t health_err = health_tests_run(ctx->health_ctx, buffer[i]);

        if (health_err != HEALTH_SUCCESS) {
            ctx->stats.health_test_failures++;
            ctx->state = SECURE_RNG_STATE_ERROR;

            if (health_err == HEALTH_ERROR_RCT_FAILURE) {
                ctx->stats.rct_failures++;
                invoke_error_callback(ctx, SECURE_RNG_ERROR_HEALTH_TEST_FAILED,
                                    "Repetition Count Test failed - entropy source may be stuck");
            } else if (health_err == HEALTH_ERROR_APT_FAILURE) {
                ctx->stats.apt_failures++;
                invoke_error_callback(ctx, SECURE_RNG_ERROR_HEALTH_TEST_FAILED,
                                    "Adaptive Proportion Test failed - loss of entropy detected");
            }

            if (ctx->config.zeroize_on_error) {
                secure_memzero(buffer, size);
            }

            return SECURE_RNG_ERROR_HEALTH_TEST_FAILED;
        }
    }

    ctx->stats.entropy_bytes_consumed += size;
    return SECURE_RNG_SUCCESS;
}

int reseed_needed(const secure_rng_ctx_t *ctx) {
    if (!ctx || !ctx->config.auto_reseed_enabled) {
        return 0;
    }

    if (ctx->config.reseed_interval == 0) {
        return 0;  // Never reseed automatically
    }

    return (ctx->bytes_since_reseed >= ctx->config.reseed_interval);
}

/*
 * VERIFIED-mode Bell certification (see header for the full contract).
 */
#define SECURE_RNG_BELL_SAMPLES 2000
secure_rng_error_t run_bell_certification(secure_rng_ctx_t *ctx) {
    quantum_state_t state;
    if (quantum_state_init(&state, 2) != QS_SUCCESS) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    quantum_entropy_ctx_t qec;
    quantum_entropy_init(&qec,
        (quantum_entropy_fn)entropy_get_bytes,
        ctx->entropy_ctx);

    bell_test_result_t r = bell_test_chsh(&state, 0, 1,
        SECURE_RNG_BELL_SAMPLES, NULL, &qec);
    quantum_state_free(&state);

    ctx->last_chsh_value = r.chsh_value;
    if (!r.violates_classical) {
        return SECURE_RNG_ERROR_HEALTH_TEST_FAILED;
    }
    ctx->bell_certified = 1;
    return SECURE_RNG_SUCCESS;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void secure_rng_get_default_config(secure_rng_config_t *config) {
    if (!config) return;

    config->mode = SECURE_RNG_MODE_QUANTUM;
    config->hybrid_threshold = DEFAULT_HYBRID_THRESHOLD;

    config->min_entropy_estimate = 4.0;
    config->rct_cutoff = health_calculate_rct_cutoff(4.0);
    config->apt_cutoff = health_calculate_apt_cutoff(4.0, 512);
    config->apt_window_size = 512;
    config->startup_test_samples = 1024;

    config->reseed_interval = DEFAULT_RESEED_INTERVAL;
    config->auto_reseed_enabled = 1;

    config->preferred_source = ENTROPY_SOURCE_RDSEED;
    config->use_multiple_sources = 0;

    config->require_hardware_entropy = 1;
    config->zeroize_on_error = 1;

    config->entropy_cache_size = 0;

    config->enable_thread_safety = 0;
}

static secure_rng_error_t validate_config(const secure_rng_config_t *config) {
    if (!config) return SECURE_RNG_ERROR_INVALID_PARAM;
    
    if (config->mode < SECURE_RNG_MODE_FAST || config->mode > SECURE_RNG_MODE_VERIFIED) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    if (config->hybrid_threshold == 0 || config->hybrid_threshold > (100 * 1024 * 1024)) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    if (!validate_health_config(
            config->rct_cutoff,
            config->apt_cutoff,
            config->apt_window_size,
            config->min_entropy_estimate)) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    if (config->reseed_interval > 0 && config->reseed_interval < 1024) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    if (config->entropy_cache_size > (100 * 1024 * 1024)) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    return SECURE_RNG_SUCCESS;
}

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

secure_rng_error_t secure_rng_init(secure_rng_ctx_t **ctx) {
    secure_rng_config_t config;
    secure_rng_get_default_config(&config);
    return secure_rng_init_with_config(ctx, &config);
}

secure_rng_error_t secure_rng_init_with_config(
    secure_rng_ctx_t **ctx_out,
    const secure_rng_config_t *config
) {
    if (!ctx_out || !config) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    secure_rng_error_t validation_err = validate_config(config);
    if (validation_err != SECURE_RNG_SUCCESS) {
        return validation_err;
    }

    secure_rng_ctx_t *ctx = calloc(1, sizeof(secure_rng_ctx_t));
    if (!ctx) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    memcpy(&ctx->config, config, sizeof(secure_rng_config_t));
    ctx->state = SECURE_RNG_STATE_STARTUP;

    ctx->entropy_ctx = calloc(1, sizeof(entropy_ctx_t));
    if (!ctx->entropy_ctx) {
        free(ctx);
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    entropy_error_t entropy_err = entropy_init(ctx->entropy_ctx);
    if (entropy_err != ENTROPY_SUCCESS) {
        free(ctx->entropy_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_ENTROPY_FAILURE;
    }

    if (config->require_hardware_entropy) {
        entropy_capabilities_t caps = entropy_get_capabilities(ctx->entropy_ctx);
        if (!caps.has_rdrand && !caps.has_rdseed && !caps.has_getrandom &&
            !caps.has_dev_random && !caps.has_dev_urandom) {
            entropy_free(ctx->entropy_ctx);
            free(ctx->entropy_ctx);
            free(ctx);
            return SECURE_RNG_ERROR_ENTROPY_FAILURE;
        }
    }

    ctx->health_ctx = calloc(1, sizeof(health_test_ctx_t));
    if (!ctx->health_ctx) {
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    health_test_config_t health_config = {
        .rct_cutoff = config->rct_cutoff,
        .apt_cutoff = config->apt_cutoff,
        .apt_window_size = config->apt_window_size,
        .startup_test_samples = config->startup_test_samples,
        .min_entropy_estimate = config->min_entropy_estimate
    };

    health_error_t health_err = health_tests_init_custom(ctx->health_ctx, &health_config);
    if (health_err != HEALTH_SUCCESS) {
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        free(ctx->health_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    uint8_t *startup_entropy = calloc(1, STARTUP_ENTROPY_SIZE);
    if (!startup_entropy) {
        health_tests_free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->health_ctx);
        free(ctx->entropy_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    entropy_err = entropy_get_bytes(ctx->entropy_ctx, startup_entropy, STARTUP_ENTROPY_SIZE);
    if (entropy_err != ENTROPY_SUCCESS) {
        secure_memzero(startup_entropy, STARTUP_ENTROPY_SIZE);
        free(startup_entropy);
        health_tests_free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->health_ctx);
        free(ctx->entropy_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_ENTROPY_FAILURE;
    }

    health_err = health_tests_startup(ctx->health_ctx, startup_entropy, STARTUP_ENTROPY_SIZE);
    if (health_err != HEALTH_SUCCESS) {
        secure_memzero(startup_entropy, STARTUP_ENTROPY_SIZE);
        free(startup_entropy);
        health_tests_free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->health_ctx);
        free(ctx->entropy_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_STARTUP_FAILED;
    }

    qrng_error qrng_err = qrng_init(&ctx->qrng_ctx, startup_entropy, STARTUP_ENTROPY_SIZE);
    secure_memzero(startup_entropy, STARTUP_ENTROPY_SIZE);
    free(startup_entropy);

    if (qrng_err != QRNG_SUCCESS) {
        health_tests_free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->health_ctx);
        free(ctx->entropy_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    if (config->entropy_cache_size > 0) {
        ctx->entropy_cache = calloc(1, config->entropy_cache_size);
        if (ctx->entropy_cache) {
            ctx->cache_size = config->entropy_cache_size;
            ctx->cache_used = 0;
        }
    }

    if (config->enable_thread_safety) {
        if (pthread_rwlock_init(&ctx->rwlock, NULL) != 0) {
            qrng_free(ctx->qrng_ctx);
            health_tests_free(ctx->health_ctx);
            entropy_free(ctx->entropy_ctx);
            free(ctx->health_ctx);
            free(ctx->entropy_ctx);
            if (ctx->entropy_cache) free(ctx->entropy_cache);
            free(ctx);
            return SECURE_RNG_ERROR_INITIALIZATION;
        }
        ctx->thread_safe = 1;
        ctx->rwlock_initialized = 1;
    }

    ctx->stats.state = SECURE_RNG_STATE_OPERATIONAL;
    ctx->stats.current_mode = config->mode;
    ctx->stats.primary_source = ctx->entropy_ctx->caps.preferred_source;
    ctx->stats.last_reseed_time = time(NULL);

    ctx->state = SECURE_RNG_STATE_OPERATIONAL;

    *ctx_out = ctx;
    return SECURE_RNG_SUCCESS;
}

secure_rng_error_t secure_rng_init_threadsafe(secure_rng_ctx_t **ctx) {
    secure_rng_config_t config;
    secure_rng_get_default_config(&config);
    config.enable_thread_safety = 1;
    return secure_rng_init_with_config(ctx, &config);
}

secure_rng_error_t secure_rng_init_threadsafe_with_config(
    secure_rng_ctx_t **ctx,
    const secure_rng_config_t *config
) {
    if (!config) return SECURE_RNG_ERROR_INVALID_PARAM;
    
    secure_rng_config_t ts_config = *config;
    ts_config.enable_thread_safety = 1;
    
    return secure_rng_init_with_config(ctx, &ts_config);
}

void secure_rng_free(secure_rng_ctx_t *ctx) {
    if (!ctx) return;

    ctx->state = SECURE_RNG_STATE_SHUTDOWN;

    if (ctx->rwlock_initialized) {
        pthread_rwlock_destroy(&ctx->rwlock);
        ctx->rwlock_initialized = 0;
    }

    if (ctx->qrng_ctx) {
        qrng_free(ctx->qrng_ctx);
        ctx->qrng_ctx = NULL;
    }

    if (ctx->health_ctx) {
        health_tests_free(ctx->health_ctx);
        free(ctx->health_ctx);
        ctx->health_ctx = NULL;
    }

    if (ctx->entropy_ctx) {
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        ctx->entropy_ctx = NULL;
    }

    if (ctx->entropy_cache) {
        secure_memzero(ctx->entropy_cache, ctx->cache_size);
        free(ctx->entropy_cache);
        ctx->entropy_cache = NULL;
    }

    secure_memzero(ctx, sizeof(*ctx));
    free(ctx);
}

secure_rng_error_t secure_rng_reset(secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    
    secure_rng_error_t lock_err = lock_write(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        unlock(ctx);
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    ctx->bytes_since_reseed = 0;
    ctx->bell_certified = 0;
    ctx->cache_used = 0;

    health_tests_reset(ctx->health_ctx);

    secure_rng_error_t result = secure_rng_reseed(ctx);
    unlock(ctx);
    return result;
}

// ============================================================================
// RESEEDING
// ============================================================================

secure_rng_error_t secure_rng_reseed(secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    uint8_t reseed_entropy[RESEED_ENTROPY_SIZE];
    secure_rng_error_t err = collect_tested_entropy(ctx, reseed_entropy, RESEED_ENTROPY_SIZE);

    if (err != SECURE_RNG_SUCCESS) {
        secure_memzero(reseed_entropy, RESEED_ENTROPY_SIZE);
        return err;
    }

    qrng_error qrng_err = qrng_reseed(ctx->qrng_ctx, reseed_entropy, RESEED_ENTROPY_SIZE);
    secure_memzero(reseed_entropy, RESEED_ENTROPY_SIZE);

    if (qrng_err != QRNG_SUCCESS) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    ctx->stats.reseed_count++;
    ctx->bytes_since_reseed = 0;
    ctx->bell_certified = 0;
    ctx->stats.last_reseed_time = time(NULL);

    return SECURE_RNG_SUCCESS;
}

secure_rng_error_t secure_rng_reseed_with_entropy(
    secure_rng_ctx_t *ctx,
    const uint8_t *external_entropy,
    size_t size
) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!external_entropy || size == 0) return SECURE_RNG_ERROR_INVALID_PARAM;
    
    secure_rng_error_t lock_err = lock_write(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        unlock(ctx);
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    uint8_t *tested_entropy = calloc(1, size);
    if (!tested_entropy) return SECURE_RNG_ERROR_INITIALIZATION;

    memcpy(tested_entropy, external_entropy, size);

    for (size_t i = 0; i < size; i++) {
        health_error_t health_err = health_tests_run(ctx->health_ctx, tested_entropy[i]);
        if (health_err != HEALTH_SUCCESS) {
            secure_memzero(tested_entropy, size);
            free(tested_entropy);
            return SECURE_RNG_ERROR_HEALTH_TEST_FAILED;
        }
    }

    uint8_t hw_entropy[256];
    secure_rng_error_t err = collect_tested_entropy(ctx, hw_entropy, sizeof(hw_entropy));
    if (err != SECURE_RNG_SUCCESS) {
        secure_memzero(tested_entropy, size);
        secure_memzero(hw_entropy, sizeof(hw_entropy));
        free(tested_entropy);
        return err;
    }

    size_t mix_size = (size < sizeof(hw_entropy)) ? size : sizeof(hw_entropy);
    for (size_t i = 0; i < mix_size; i++) {
        tested_entropy[i] ^= hw_entropy[i];
    }

    qrng_error qrng_err = qrng_reseed(ctx->qrng_ctx, tested_entropy, size);

    secure_memzero(tested_entropy, size);
    secure_memzero(hw_entropy, sizeof(hw_entropy));
    free(tested_entropy);

    if (qrng_err != QRNG_SUCCESS) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    ctx->stats.reseed_count++;
    ctx->bytes_since_reseed = 0;
    ctx->bell_certified = 0;
    ctx->stats.last_reseed_time = time(NULL);

    unlock(ctx);
    return SECURE_RNG_SUCCESS;
}

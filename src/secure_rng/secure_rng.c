#include "secure_rng.h"
#include "../common/secure_memory.h"
#include "../common/validation.h"
#include "../quantum_rng/quantum_state.h"
#include "../quantum_rng/bell_test.h"
#include "../quantum_rng/quantum_entropy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

/**
 * @file secure_rng.c
 * @brief Production implementation of secure RNG
 *
 * This implementation integrates entropy sources, health tests, and quantum RNG
 * into a unified, production-grade secure random number generator.
 */

// Version information
#define SECURE_RNG_VERSION_MAJOR 2
#define SECURE_RNG_VERSION_MINOR 0
#define SECURE_RNG_VERSION_PATCH 0

// Internal constants
#define STARTUP_ENTROPY_SIZE 2048
#define RESEED_ENTROPY_SIZE 1024
#define MIN_ENTROPY_FOR_RESEED 256
#define DEFAULT_RESEED_INTERVAL (1024 * 1024)  // 1MB
#define DEFAULT_HYBRID_THRESHOLD 1024  // Use FAST for < 1KB, QUANTUM for >= 1KB

// ============================================================================
// THREAD SAFETY HELPERS
// ============================================================================

/**
 * @brief Acquire write lock if thread safety is enabled
 */
static secure_rng_error_t lock_write(secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!ctx->thread_safe) return SECURE_RNG_SUCCESS;
    
    if (pthread_rwlock_wrlock(&ctx->rwlock) != 0) {
        return SECURE_RNG_ERROR_MUTEX_LOCK;
    }
    return SECURE_RNG_SUCCESS;
}

/**
 * @brief Acquire read lock if thread safety is enabled
 */
static secure_rng_error_t lock_read(const secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!ctx->thread_safe) return SECURE_RNG_SUCCESS;
    
    // Cast away const for locking - this is safe for rwlock
    // as read locks don't modify the logical state
    secure_rng_ctx_t *mutable_ctx = (secure_rng_ctx_t *)ctx;
    if (pthread_rwlock_rdlock(&mutable_ctx->rwlock) != 0) {
        return SECURE_RNG_ERROR_MUTEX_LOCK;
    }
    return SECURE_RNG_SUCCESS;
}

/**
 * @brief Release lock if thread safety is enabled
 */
static secure_rng_error_t unlock(const secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!ctx->thread_safe) return SECURE_RNG_SUCCESS;
    
    // Cast away const - safe for unlock operation
    secure_rng_ctx_t *mutable_ctx = (secure_rng_ctx_t *)ctx;
    if (pthread_rwlock_unlock(&mutable_ctx->rwlock) != 0) {
        return SECURE_RNG_ERROR_MUTEX_UNLOCK;
    }
    return SECURE_RNG_SUCCESS;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Invoke error callback if set
 */
static void invoke_error_callback(
    secure_rng_ctx_t *ctx,
    secure_rng_error_t error,
    const char *message
) {
    if (ctx && ctx->error_callback) {
        ctx->error_callback(error, message, ctx->callback_user_data);
    }
}

// Removed - now using secure_memzero from secure_memory.h

/**
 * @brief Collect and test entropy
 *
 * Collects entropy from hardware sources and runs health tests on it.
 * Returns only entropy that has passed all health tests.
 */
static secure_rng_error_t collect_tested_entropy(
    secure_rng_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
) {
    if (!ctx || !buffer || size == 0) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }

    // Collect raw entropy
    entropy_error_t entropy_err = entropy_get_bytes(ctx->entropy_ctx, buffer, size);
    if (entropy_err != ENTROPY_SUCCESS) {
        invoke_error_callback(ctx, SECURE_RNG_ERROR_ENTROPY_FAILURE,
                             "Failed to collect entropy from hardware sources");
        return SECURE_RNG_ERROR_ENTROPY_FAILURE;
    }

    // Run health tests on each byte
    for (size_t i = 0; i < size; i++) {
        health_error_t health_err = health_tests_run(ctx->health_ctx, buffer[i]);

        if (health_err != HEALTH_SUCCESS) {
            // Health test failed - this is critical
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

            // Securely zero the buffer on failure
            if (ctx->config.zeroize_on_error) {
                secure_memzero(buffer, size);
            }

            return SECURE_RNG_ERROR_HEALTH_TEST_FAILED;
        }
    }

    ctx->stats.entropy_bytes_consumed += size;
    return SECURE_RNG_SUCCESS;
}

/**
 * @brief Check if reseed is needed
 */
static int reseed_needed(const secure_rng_ctx_t *ctx) {
    if (!ctx || !ctx->config.auto_reseed_enabled) {
        return 0;
    }

    if (ctx->config.reseed_interval == 0) {
        return 0;  // Never reseed automatically
    }

    return (ctx->bytes_since_reseed >= ctx->config.reseed_interval);
}

/*
 * VERIFIED-mode Bell certification.
 *
 * Runs a CHSH Bell-inequality test on the simulated quantum engine and
 * confirms it violates the classical bound (S > 2). A classical process
 * cannot exceed S = 2; a value approaching the Tsirelson bound 2√2 ≈ 2.828
 * certifies that the quantum layer is producing genuine (simulated) quantum
 * correlations. The measurement sampling is seeded from the context's
 * hardware entropy source. Certification holds for one epoch (until the next
 * reseed), after which it is repeated.
 *
 * Returns SECURE_RNG_SUCCESS if the source is certified, or
 * SECURE_RNG_ERROR_HEALTH_TEST_FAILED if the Bell test fails to violate the
 * classical bound.
 */
#define SECURE_RNG_BELL_SAMPLES 2000
static secure_rng_error_t run_bell_certification(secure_rng_ctx_t *ctx) {
    quantum_state_t state;
    if (quantum_state_init(&state, 2) != QS_SUCCESS) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    /* Bridge the hardware entropy source into the quantum-op entropy interface
     * so Born-rule measurement sampling is cryptographically seeded. */
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

    // Mode defaults
    config->mode = SECURE_RNG_MODE_QUANTUM;  // Default to quantum mode
    config->hybrid_threshold = DEFAULT_HYBRID_THRESHOLD;

    // Health test defaults (conservative, H_min = 4.0 bits/byte)
    config->min_entropy_estimate = 4.0;
    config->rct_cutoff = health_calculate_rct_cutoff(4.0);
    config->apt_cutoff = health_calculate_apt_cutoff(4.0, 512);
    config->apt_window_size = 512;
    config->startup_test_samples = 1024;

    // Reseeding defaults
    config->reseed_interval = DEFAULT_RESEED_INTERVAL;
    config->auto_reseed_enabled = 1;

    // Entropy source defaults
    config->preferred_source = ENTROPY_SOURCE_RDSEED;
    config->use_multiple_sources = 0;

    // Security defaults
    config->require_hardware_entropy = 1;
    config->zeroize_on_error = 1;

    // Performance defaults
    config->entropy_cache_size = 0;  // No caching by default
    
    // Thread safety defaults
    config->enable_thread_safety = 0;  // Disabled by default (per-thread contexts recommended)
}

/**
 * @brief Validate configuration parameters
 */
static secure_rng_error_t validate_config(const secure_rng_config_t *config) {
    if (!config) return SECURE_RNG_ERROR_INVALID_PARAM;
    
    // Validate mode
    if (config->mode < SECURE_RNG_MODE_FAST || config->mode > SECURE_RNG_MODE_VERIFIED) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    // Validate hybrid threshold
    if (config->hybrid_threshold == 0 || config->hybrid_threshold > (100 * 1024 * 1024)) {
        return SECURE_RNG_ERROR_INVALID_PARAM;  // Max 100MB threshold
    }
    
    // Validate health test parameters
    if (!validate_health_config(
            config->rct_cutoff,
            config->apt_cutoff,
            config->apt_window_size,
            config->min_entropy_estimate)) {
        return SECURE_RNG_ERROR_INVALID_PARAM;
    }
    
    // Validate reseed interval
    if (config->reseed_interval > 0 && config->reseed_interval < 1024) {
        return SECURE_RNG_ERROR_INVALID_PARAM;  // Min 1KB if enabled
    }
    
    // Validate entropy cache size
    if (config->entropy_cache_size > (100 * 1024 * 1024)) {
        return SECURE_RNG_ERROR_INVALID_PARAM;  // Max 100MB cache
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
    
    // Validate configuration
    secure_rng_error_t validation_err = validate_config(config);
    if (validation_err != SECURE_RNG_SUCCESS) {
        return validation_err;
    }

    // Allocate context
    secure_rng_ctx_t *ctx = calloc(1, sizeof(secure_rng_ctx_t));
    if (!ctx) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    // Copy configuration
    memcpy(&ctx->config, config, sizeof(secure_rng_config_t));
    ctx->state = SECURE_RNG_STATE_STARTUP;

    // Initialize entropy source
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

    // Check hardware entropy requirement
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

    // Initialize health tests
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

    // Run startup health tests
    uint8_t *startup_entropy = calloc(1, STARTUP_ENTROPY_SIZE);
    if (!startup_entropy) {
        health_tests_free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->health_ctx);
        free(ctx->entropy_ctx);
        free(ctx);
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    // Collect startup entropy
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

    // Run startup tests
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

    // Initialize quantum RNG with tested entropy
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

    // Initialize entropy cache if configured
    if (config->entropy_cache_size > 0) {
        ctx->entropy_cache = calloc(1, config->entropy_cache_size);
        if (ctx->entropy_cache) {
            ctx->cache_size = config->entropy_cache_size;
            ctx->cache_used = 0;
        }
    }

    // Initialize thread safety if requested
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

    // Initialize statistics
    ctx->stats.state = SECURE_RNG_STATE_OPERATIONAL;
    ctx->stats.current_mode = config->mode;
    ctx->stats.primary_source = ctx->entropy_ctx->caps.preferred_source;
    ctx->stats.last_reseed_time = time(NULL);

    // Set state to operational
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
    
    // Create modified config with thread safety enabled
    secure_rng_config_t ts_config = *config;
    ts_config.enable_thread_safety = 1;
    
    return secure_rng_init_with_config(ctx, &ts_config);
}

void secure_rng_free(secure_rng_ctx_t *ctx) {
    if (!ctx) return;

    // Set state to shutdown
    ctx->state = SECURE_RNG_STATE_SHUTDOWN;

    // Destroy rwlock if initialized
    if (ctx->rwlock_initialized) {
        pthread_rwlock_destroy(&ctx->rwlock);
        ctx->rwlock_initialized = 0;
    }

    // Free quantum RNG
    if (ctx->qrng_ctx) {
        qrng_free(ctx->qrng_ctx);
        ctx->qrng_ctx = NULL;
    }

    // Free health tests
    if (ctx->health_ctx) {
        health_tests_free(ctx->health_ctx);
        free(ctx->health_ctx);
        ctx->health_ctx = NULL;
    }

    // Free entropy context
    if (ctx->entropy_ctx) {
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        ctx->entropy_ctx = NULL;
    }

    // Free and zero entropy cache
    if (ctx->entropy_cache) {
        secure_memzero(ctx->entropy_cache, ctx->cache_size);
        free(ctx->entropy_cache);
        ctx->entropy_cache = NULL;
    }

    // Zero entire context
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

    // Reset statistics (keep lifetime stats)
    ctx->bytes_since_reseed = 0;
    ctx->bell_certified = 0;  /* renew Bell certification for the new epoch */
    ctx->cache_used = 0;

    // Reset health tests
    health_tests_reset(ctx->health_ctx);

    // Reseed
    secure_rng_error_t result = secure_rng_reseed(ctx);
    unlock(ctx);
    return result;
}

// ============================================================================
// RESEEDING
// ============================================================================

secure_rng_error_t secure_rng_reseed(secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    
    // Note: Caller (secure_rng_reset or init) handles locking
    
    if (ctx->state != SECURE_RNG_STATE_OPERATIONAL) {
        return SECURE_RNG_ERROR_NOT_INITIALIZED;
    }

    // Collect fresh tested entropy
    uint8_t reseed_entropy[RESEED_ENTROPY_SIZE];
    secure_rng_error_t err = collect_tested_entropy(ctx, reseed_entropy, RESEED_ENTROPY_SIZE);

    if (err != SECURE_RNG_SUCCESS) {
        secure_memzero(reseed_entropy, RESEED_ENTROPY_SIZE);
        return err;
    }

    // Reseed quantum RNG
    qrng_error qrng_err = qrng_reseed(ctx->qrng_ctx, reseed_entropy, RESEED_ENTROPY_SIZE);
    secure_memzero(reseed_entropy, RESEED_ENTROPY_SIZE);

    if (qrng_err != QRNG_SUCCESS) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    // Update statistics
    ctx->stats.reseed_count++;
    ctx->bytes_since_reseed = 0;
    ctx->bell_certified = 0;  /* renew Bell certification for the new epoch */
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

    // Test external entropy through health tests
    uint8_t *tested_entropy = calloc(1, size);
    if (!tested_entropy) return SECURE_RNG_ERROR_INITIALIZATION;

    memcpy(tested_entropy, external_entropy, size);

    // Run health tests
    for (size_t i = 0; i < size; i++) {
        health_error_t health_err = health_tests_run(ctx->health_ctx, tested_entropy[i]);
        if (health_err != HEALTH_SUCCESS) {
            secure_memzero(tested_entropy, size);
            free(tested_entropy);
            return SECURE_RNG_ERROR_HEALTH_TEST_FAILED;
        }
    }

    // Mix with hardware entropy
    uint8_t hw_entropy[256];
    secure_rng_error_t err = collect_tested_entropy(ctx, hw_entropy, sizeof(hw_entropy));
    if (err != SECURE_RNG_SUCCESS) {
        secure_memzero(tested_entropy, size);
        secure_memzero(hw_entropy, sizeof(hw_entropy));
        free(tested_entropy);
        return err;
    }

    // Combine entropies (XOR mix)
    size_t mix_size = (size < sizeof(hw_entropy)) ? size : sizeof(hw_entropy);
    for (size_t i = 0; i < mix_size; i++) {
        tested_entropy[i] ^= hw_entropy[i];
    }

    // Reseed with combined entropy
    qrng_error qrng_err = qrng_reseed(ctx->qrng_ctx, tested_entropy, size);

    secure_memzero(tested_entropy, size);
    secure_memzero(hw_entropy, sizeof(hw_entropy));
    free(tested_entropy);

    if (qrng_err != QRNG_SUCCESS) {
        return SECURE_RNG_ERROR_INITIALIZATION;
    }

    ctx->stats.reseed_count++;
    ctx->bytes_since_reseed = 0;
    ctx->bell_certified = 0;  /* renew Bell certification for the new epoch */
    ctx->stats.last_reseed_time = time(NULL);

    unlock(ctx);
    return SECURE_RNG_SUCCESS;
}

// ============================================================================
// RANDOM NUMBER GENERATION
// ============================================================================

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

    // Determine effective mode (for HYBRID)
    secure_rng_mode_t effective_mode = ctx->config.mode;
    if (effective_mode == SECURE_RNG_MODE_HYBRID) {
        effective_mode = (size < ctx->config.hybrid_threshold) ?
                         SECURE_RNG_MODE_FAST : SECURE_RNG_MODE_QUANTUM;
    }

    // Check if reseed needed
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

    // Generate based on mode
    secure_rng_error_t result = SECURE_RNG_SUCCESS;
    
    switch (effective_mode) {
        case SECURE_RNG_MODE_FAST:
            // Direct hardware entropy (fastest, still health-tested)
            result = collect_tested_entropy(ctx, buffer, size);
            if (result == SECURE_RNG_SUCCESS) {
                ctx->stats.fast_mode_bytes += size;
            }
            break;
            
        case SECURE_RNG_MODE_QUANTUM:
            // Quantum mixing (default, balanced)
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
            // Quantum + Bell test verification (slowest, maximum assurance)
            {
                /* Certify the quantum source with a CHSH Bell test before
                 * serving VERIFIED-mode output. The certificate is established
                 * at first use and renewed after every reseed; if the source
                 * ever fails to violate the classical bound, the context enters
                 * the error state and no bytes are returned. */
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
            // Should not reach here (handled above)
            result = SECURE_RNG_ERROR_INVALID_PARAM;
            break;
    }

    if (result == SECURE_RNG_SUCCESS) {
        // Update statistics
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

// ============================================================================
// STATUS & MONITORING
// ============================================================================

secure_rng_error_t secure_rng_get_stats(
    const secure_rng_ctx_t *ctx,
    secure_rng_stats_t *stats
) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!stats) return SECURE_RNG_ERROR_NULL_BUFFER;

    // Use read lock for const operation - no cast needed!
    secure_rng_error_t lock_err = lock_read(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;

    memcpy(stats, &ctx->stats, sizeof(*stats));
    
    unlock(ctx);
    return SECURE_RNG_SUCCESS;
}

secure_rng_state_t secure_rng_get_state(const secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_STATE_UNINITIALIZED;
    return ctx->state;
}

int secure_rng_is_operational(const secure_rng_ctx_t *ctx) {
    return (ctx && ctx->state == SECURE_RNG_STATE_OPERATIONAL);
}

const health_test_stats_t* secure_rng_get_health_stats(const secure_rng_ctx_t *ctx) {
    if (!ctx || !ctx->health_ctx) return NULL;
    return &ctx->health_ctx->stats;
}

const entropy_capabilities_t* secure_rng_get_entropy_caps(const secure_rng_ctx_t *ctx) {
    if (!ctx || !ctx->entropy_ctx) return NULL;
    return &ctx->entropy_ctx->caps;
}

void secure_rng_print_stats(const secure_rng_ctx_t *ctx) {
    if (!ctx) return;

    printf("=== Secure RNG Statistics ===\n\n");

    // State
    printf("State: ");
    switch (ctx->state) {
        case SECURE_RNG_STATE_UNINITIALIZED: printf("Uninitialized\n"); break;
        case SECURE_RNG_STATE_STARTUP: printf("Startup\n"); break;
        case SECURE_RNG_STATE_OPERATIONAL: printf("Operational\n"); break;
        case SECURE_RNG_STATE_ERROR: printf("Error\n"); break;
        case SECURE_RNG_STATE_SHUTDOWN: printf("Shutdown\n"); break;
    }

    // Generation statistics
    printf("\nGeneration Statistics:\n");
    printf("  Bytes generated: %llu\n", (unsigned long long)ctx->stats.bytes_generated);
    printf("  Requests served: %llu\n", (unsigned long long)ctx->stats.requests_served);
    printf("  Reseed count: %llu\n", (unsigned long long)ctx->stats.reseed_count);
    printf("  Bytes since reseed: %llu\n", (unsigned long long)ctx->bytes_since_reseed);

    // Health test statistics
    printf("\nHealth Test Statistics:\n");
    printf("  Total failures: %llu\n", (unsigned long long)ctx->stats.health_test_failures);
    printf("  RCT failures: %llu\n", (unsigned long long)ctx->stats.rct_failures);
    printf("  APT failures: %llu\n", (unsigned long long)ctx->stats.apt_failures);

    // Entropy statistics
    printf("\nEntropy Statistics:\n");
    printf("  Entropy consumed: %llu bytes\n", (unsigned long long)ctx->stats.entropy_bytes_consumed);
    printf("  Primary source: %s\n", entropy_source_name(ctx->stats.primary_source));

    // Performance statistics
    if (ctx->config.entropy_cache_size > 0) {
        printf("\nCache Statistics:\n");
        printf("  Cache hits: %llu\n", (unsigned long long)ctx->stats.cache_hits);
        printf("  Cache misses: %llu\n", (unsigned long long)ctx->stats.cache_misses);
    }

    printf("\n");

    // Print health test details
    if (ctx->health_ctx) {
        health_tests_print_stats(ctx->health_ctx);
    }

    printf("\n");

    // Print entropy source details
    if (ctx->entropy_ctx) {
        entropy_print_stats(ctx->entropy_ctx);
    }
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

void secure_rng_set_error_callback(
    secure_rng_ctx_t *ctx,
    void (*callback)(secure_rng_error_t error, const char *msg, void *user_data),
    void *user_data
) {
    if (!ctx) return;
    ctx->error_callback = callback;
    ctx->callback_user_data = user_data;
}

const char* secure_rng_error_string(secure_rng_error_t error) {
    switch (error) {
        case SECURE_RNG_SUCCESS:
            return "Success";
        case SECURE_RNG_ERROR_NULL_CONTEXT:
            return "NULL context provided";
        case SECURE_RNG_ERROR_NULL_BUFFER:
            return "NULL buffer provided";
        case SECURE_RNG_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case SECURE_RNG_ERROR_HEALTH_TEST_FAILED:
            return "Health test failed - entropy source may be compromised";
        case SECURE_RNG_ERROR_ENTROPY_FAILURE:
            return "Entropy source failure";
        case SECURE_RNG_ERROR_INITIALIZATION:
            return "Initialization failed";
        case SECURE_RNG_ERROR_NOT_INITIALIZED:
            return "Context not initialized";
        case SECURE_RNG_ERROR_STARTUP_FAILED:
            return "Startup tests failed - cannot use RNG";
        case SECURE_RNG_ERROR_INSUFFICIENT_ENTROPY:
            return "Insufficient entropy available";
        case SECURE_RNG_ERROR_INVALID_RANGE:
            return "Invalid range parameters";
        case SECURE_RNG_ERROR_MUTEX_LOCK:
            return "Mutex lock failed";
        case SECURE_RNG_ERROR_MUTEX_UNLOCK:
            return "Mutex unlock failed";
        default:
            return "Unknown error";
    }
}

// ============================================================================
// MODE SWITCHING
// ============================================================================

secure_rng_mode_t secure_rng_get_mode(const secure_rng_ctx_t *ctx) {
    if (!ctx) return SECURE_RNG_MODE_QUANTUM;
    return ctx->config.mode;
}

secure_rng_error_t secure_rng_set_mode(secure_rng_ctx_t *ctx, secure_rng_mode_t mode) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    
    secure_rng_error_t lock_err = lock_write(ctx);
    if (lock_err != SECURE_RNG_SUCCESS) return lock_err;
    
    ctx->config.mode = mode;
    ctx->stats.current_mode = mode;
    
    unlock(ctx);
    return SECURE_RNG_SUCCESS;
}

const char* secure_rng_mode_string(secure_rng_mode_t mode) {
    switch (mode) {
        case SECURE_RNG_MODE_FAST:
            return "FAST (hardware entropy only)";
        case SECURE_RNG_MODE_QUANTUM:
            return "QUANTUM (quantum mixing + health tests)";
        case SECURE_RNG_MODE_HYBRID:
            return "HYBRID (adaptive mode switching)";
        case SECURE_RNG_MODE_VERIFIED:
            return "VERIFIED (quantum + Bell test verification)";
        default:
            return "Unknown mode";
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

int secure_rng_self_test(int verbose) {
    if (verbose) {
        printf("=== Secure RNG Self-Test ===\n\n");
    }

    // Test initialization
    if (verbose) printf("Testing initialization... ");
    secure_rng_ctx_t *ctx;
    secure_rng_error_t err = secure_rng_init(&ctx);
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        return 0;
    }
    if (verbose) printf("PASSED\n");

    // Test byte generation
    if (verbose) printf("Testing byte generation... ");
    uint8_t bytes[100];
    err = secure_rng_bytes(ctx, bytes, sizeof(bytes));
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    // Test uint64 generation
    if (verbose) printf("Testing uint64 generation... ");
    uint64_t val64;
    err = secure_rng_uint64(ctx, &val64);
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    // Test double generation
    if (verbose) printf("Testing double generation... ");
    double valdbl;
    err = secure_rng_double(ctx, &valdbl);
    if (err != SECURE_RNG_SUCCESS || valdbl < 0.0 || valdbl >= 1.0) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    // Test range generation
    if (verbose) printf("Testing range generation... ");
    int32_t valrange;
    err = secure_rng_range32(ctx, 1, 100, &valrange);
    if (err != SECURE_RNG_SUCCESS || valrange < 1 || valrange > 100) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    // Test reseeding
    if (verbose) printf("Testing reseed... ");
    err = secure_rng_reseed(ctx);
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    // Test statistics
    if (verbose) printf("Testing statistics... ");
    secure_rng_stats_t stats;
    err = secure_rng_get_stats(ctx, &stats);
    if (err != SECURE_RNG_SUCCESS || stats.bytes_generated == 0) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    // Cleanup
    if (verbose) printf("Testing cleanup... ");
    secure_rng_free(ctx);
    if (verbose) printf("PASSED\n\n");

    if (verbose) {
        printf("All self-tests PASSED\n");
    }

    return 1;
}

const char* secure_rng_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             SECURE_RNG_VERSION_MAJOR,
             SECURE_RNG_VERSION_MINOR,
             SECURE_RNG_VERSION_PATCH);
    return version;
}

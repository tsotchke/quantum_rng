/*
 * secure_stats.c - status, statistics, error handling, mode control, version
 */
#include "secure_internal.h"

secure_rng_error_t secure_rng_get_stats(
    const secure_rng_ctx_t *ctx,
    secure_rng_stats_t *stats
) {
    if (!ctx) return SECURE_RNG_ERROR_NULL_CONTEXT;
    if (!stats) return SECURE_RNG_ERROR_NULL_BUFFER;

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

    printf("State: ");
    switch (ctx->state) {
        case SECURE_RNG_STATE_UNINITIALIZED: printf("Uninitialized\n"); break;
        case SECURE_RNG_STATE_STARTUP: printf("Startup\n"); break;
        case SECURE_RNG_STATE_OPERATIONAL: printf("Operational\n"); break;
        case SECURE_RNG_STATE_ERROR: printf("Error\n"); break;
        case SECURE_RNG_STATE_SHUTDOWN: printf("Shutdown\n"); break;
    }

    printf("\nGeneration Statistics:\n");
    printf("  Bytes generated: %llu\n", (unsigned long long)ctx->stats.bytes_generated);
    printf("  Requests served: %llu\n", (unsigned long long)ctx->stats.requests_served);
    printf("  Reseed count: %llu\n", (unsigned long long)ctx->stats.reseed_count);
    printf("  Bytes since reseed: %llu\n", (unsigned long long)ctx->bytes_since_reseed);

    printf("\nHealth Test Statistics:\n");
    printf("  Total failures: %llu\n", (unsigned long long)ctx->stats.health_test_failures);
    printf("  RCT failures: %llu\n", (unsigned long long)ctx->stats.rct_failures);
    printf("  APT failures: %llu\n", (unsigned long long)ctx->stats.apt_failures);

    printf("\nEntropy Statistics:\n");
    printf("  Entropy consumed: %llu bytes\n", (unsigned long long)ctx->stats.entropy_bytes_consumed);
    printf("  Primary source: %s\n", entropy_source_name(ctx->stats.primary_source));

    if (ctx->config.entropy_cache_size > 0) {
        printf("\nCache Statistics:\n");
        printf("  Cache hits: %llu\n", (unsigned long long)ctx->stats.cache_hits);
        printf("  Cache misses: %llu\n", (unsigned long long)ctx->stats.cache_misses);
    }

    printf("\n");

    if (ctx->health_ctx) {
        health_tests_print_stats(ctx->health_ctx);
    }

    printf("\n");

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

const char* secure_rng_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             SECURE_RNG_VERSION_MAJOR,
             SECURE_RNG_VERSION_MINOR,
             SECURE_RNG_VERSION_PATCH);
    return version;
}

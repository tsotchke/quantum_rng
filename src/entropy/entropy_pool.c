#include "entropy_pool.h"
#include "../common/secure_memory.h"
#include "../common/validation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/**
 * @file entropy_pool.c
 * @brief High-performance entropy pool with background pre-generation
 * 
 * Implements a thread-safe entropy pool that continuously generates and
 * tests entropy in the background, providing near-zero latency for requests.
 */

// ============================================================================
// BACKGROUND WORKER THREAD
// ============================================================================

/**
 * @brief Background thread function for continuous entropy generation
 */
static void* entropy_worker_thread(void *arg) {
    entropy_pool_ctx_t *pool = (entropy_pool_ctx_t *)arg;
    
    uint8_t chunk[ENTROPY_POOL_CHUNK_SIZE];
    
    while (1) {
        // Check for shutdown signal
        pthread_mutex_lock(&pool->pool_mutex);
        if (pool->shutdown_requested) {
            pthread_mutex_unlock(&pool->pool_mutex);
            break;
        }
        
        // Wait if pool is full
        while (pool->pool_available >= pool->config.refill_threshold && 
               !pool->shutdown_requested) {
            pthread_cond_wait(&pool->refill_cond, &pool->pool_mutex);
        }
        
        if (pool->shutdown_requested) {
            pthread_mutex_unlock(&pool->pool_mutex);
            break;
        }
        
        pthread_mutex_unlock(&pool->pool_mutex);
        
        // Generate entropy outside lock for better concurrency
        entropy_error_t err = entropy_get_bytes(pool->entropy_ctx, chunk, sizeof(chunk));
        if (err != ENTROPY_SUCCESS) {
            usleep(1000);  // Back off on error
            continue;
        }
        
        // Run health tests on generated entropy. The health context is shared
        // with the on-demand generation path, so serialize access to it.
        pthread_mutex_lock(&pool->health_mutex);
        health_error_t health_err = health_tests_run_batch(
            pool->health_ctx, chunk, sizeof(chunk));
        pthread_mutex_unlock(&pool->health_mutex);

        if (health_err != HEALTH_SUCCESS) {
            // Health test failed - discard this chunk
            secure_memzero(chunk, sizeof(chunk));
            usleep(10000);  // Back off more on health failure
            continue;
        }
        
        // Add tested entropy to pool
        pthread_mutex_lock(&pool->pool_mutex);
        
        // Calculate space available
        size_t space_available = pool->pool_size - pool->pool_available;
        size_t bytes_to_add = (sizeof(chunk) < space_available) ? 
                               sizeof(chunk) : space_available;
        
        if (bytes_to_add > 0) {
            // Calculate write position with wraparound
            size_t write_pos = pool->pool_used + pool->pool_available;
            if (write_pos >= pool->pool_size) {
                write_pos -= pool->pool_size;
            }
            
            // Handle wraparound
            if (write_pos + bytes_to_add <= pool->pool_size) {
                memcpy(pool->pool_buffer + write_pos, chunk, bytes_to_add);
            } else {
                // Split copy
                size_t first_part = pool->pool_size - write_pos;
                memcpy(pool->pool_buffer + write_pos, chunk, first_part);
                memcpy(pool->pool_buffer, chunk + first_part, bytes_to_add - first_part);
            }
            
            pool->pool_available += bytes_to_add;
            pool->stats.background_chunks++;
        }
        
        pthread_mutex_unlock(&pool->pool_mutex);
        
        // Small delay to prevent spinning
        usleep(100);
    }
    
    secure_memzero(chunk, sizeof(chunk));
    return NULL;
}

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

int entropy_pool_init(entropy_pool_ctx_t **ctx) {
    entropy_pool_config_t config = {
        .pool_size = ENTROPY_POOL_DEFAULT_SIZE,
        .refill_threshold = ENTROPY_POOL_REFILL_THRESHOLD,
        .chunk_size = ENTROPY_POOL_CHUNK_SIZE,
        .enable_background_thread = 1,
        .min_entropy = 4.0
    };
    
    return entropy_pool_init_with_config(ctx, &config);
}

int entropy_pool_init_with_config(
    entropy_pool_ctx_t **ctx_out,
    const entropy_pool_config_t *config
) {
    VALIDATE_NOT_NULL(ctx_out, -1);
    VALIDATE_NOT_NULL(config, -1);
    
    // Validate configuration
    if (config->pool_size == 0 || config->pool_size > (100 * 1024 * 1024)) {
        return -1;  // Max 100MB pool
    }
    if (config->chunk_size == 0 || config->chunk_size > config->pool_size) {
        return -1;
    }
    
    // Allocate context
    entropy_pool_ctx_t *ctx = calloc(1, sizeof(entropy_pool_ctx_t));
    if (!ctx) return -1;
    
    // Copy configuration
    memcpy(&ctx->config, config, sizeof(entropy_pool_config_t));
    ctx->pool_size = config->pool_size;
    
    // Allocate pool buffer
    ctx->pool_buffer = calloc(1, ctx->pool_size);
    if (!ctx->pool_buffer) {
        free(ctx);
        return -1;
    }
    
    // Initialize hardware entropy
    ctx->entropy_ctx = calloc(1, sizeof(entropy_ctx_t));
    if (!ctx->entropy_ctx) {
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
        free(ctx);
        return -1;
    }
    
    entropy_error_t err = entropy_init(ctx->entropy_ctx);
    if (err != ENTROPY_SUCCESS) {
        free(ctx->entropy_ctx);
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
        free(ctx);
        return -1;
    }
    
    // Initialize health tests
    ctx->health_ctx = calloc(1, sizeof(health_test_ctx_t));
    if (!ctx->health_ctx) {
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
        free(ctx);
        return -1;
    }
    
    health_test_config_t health_config;
    health_get_recommended_config(config->min_entropy, &health_config);
    
    health_error_t health_err = health_tests_init_custom(ctx->health_ctx, &health_config);
    if (health_err != HEALTH_SUCCESS) {
        free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
        free(ctx);
        return -1;
    }
    
    // Initialize thread safety
    if (pthread_mutex_init(&ctx->pool_mutex, NULL) != 0) {
        health_tests_free(ctx->health_ctx);
        free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
        free(ctx);
        return -1;
    }
    
    if (pthread_mutex_init(&ctx->health_mutex, NULL) != 0) {
        pthread_mutex_destroy(&ctx->pool_mutex);
        health_tests_free(ctx->health_ctx);
        free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
        free(ctx);
        return -1;
    }

    if (pthread_cond_init(&ctx->refill_cond, NULL) != 0) {
        pthread_mutex_destroy(&ctx->health_mutex);
        pthread_mutex_destroy(&ctx->pool_mutex);
        health_tests_free(ctx->health_ctx);
        free(ctx->health_ctx);
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
        free(ctx);
        return -1;
    }
    
    // Pre-fill pool with tested entropy
    uint8_t startup_entropy[4096];
    err = entropy_get_bytes(ctx->entropy_ctx, startup_entropy, sizeof(startup_entropy));
    if (err == ENTROPY_SUCCESS) {
        pthread_mutex_lock(&ctx->health_mutex);
        health_err = health_tests_run_batch(ctx->health_ctx, startup_entropy, sizeof(startup_entropy));
        pthread_mutex_unlock(&ctx->health_mutex);
        if (health_err == HEALTH_SUCCESS) {
            memcpy(ctx->pool_buffer, startup_entropy, sizeof(startup_entropy));
            ctx->pool_available = sizeof(startup_entropy);
        }
    }
    secure_memzero(startup_entropy, sizeof(startup_entropy));
    
    // Start background thread if enabled
    if (config->enable_background_thread) {
        if (entropy_pool_start_background(ctx) != 0) {
            pthread_cond_destroy(&ctx->refill_cond);
            pthread_mutex_destroy(&ctx->health_mutex);
            pthread_mutex_destroy(&ctx->pool_mutex);
            health_tests_free(ctx->health_ctx);
            free(ctx->health_ctx);
            entropy_free(ctx->entropy_ctx);
            free(ctx->entropy_ctx);
            secure_memzero(ctx->pool_buffer, ctx->pool_size);
            free(ctx->pool_buffer);
            free(ctx);
            return -1;
        }
    }
    
    *ctx_out = ctx;
    return 0;
}

void entropy_pool_free(entropy_pool_ctx_t *ctx) {
    if (!ctx) return;
    
    // Stop background thread if running
    if (ctx->background_running) {
        entropy_pool_stop_background(ctx);
    }
    
    // Destroy synchronization primitives
    pthread_cond_destroy(&ctx->refill_cond);
    pthread_mutex_destroy(&ctx->health_mutex);
    pthread_mutex_destroy(&ctx->pool_mutex);

    // Free components
    if (ctx->health_ctx) {
        health_tests_free(ctx->health_ctx);
        free(ctx->health_ctx);
    }
    
    if (ctx->entropy_ctx) {
        entropy_free(ctx->entropy_ctx);
        free(ctx->entropy_ctx);
    }
    
    // Securely zero and free pool buffer
    if (ctx->pool_buffer) {
        secure_memzero(ctx->pool_buffer, ctx->pool_size);
        free(ctx->pool_buffer);
    }
    
    // Zero context
    secure_memzero(ctx, sizeof(*ctx));
    free(ctx);
}

int entropy_pool_start_background(entropy_pool_ctx_t *ctx) {
    VALIDATE_NOT_NULL(ctx, -1);
    
    if (ctx->background_running) {
        return 0;  // Already running
    }
    
    ctx->shutdown_requested = 0;
    
    if (pthread_create(&ctx->background_thread, NULL, entropy_worker_thread, ctx) != 0) {
        return -1;
    }
    
    ctx->background_running = 1;
    ctx->stats.background_active = 1;
    
    return 0;
}

void entropy_pool_stop_background(entropy_pool_ctx_t *ctx) {
    if (!ctx || !ctx->background_running) return;
    
    // Signal shutdown
    pthread_mutex_lock(&ctx->pool_mutex);
    ctx->shutdown_requested = 1;
    pthread_cond_broadcast(&ctx->refill_cond);
    pthread_mutex_unlock(&ctx->pool_mutex);
    
    // Wait for thread to exit
    pthread_join(ctx->background_thread, NULL);
    
    ctx->background_running = 0;
    ctx->stats.background_active = 0;
}

// ============================================================================
// ENTROPY RETRIEVAL
// ============================================================================

int entropy_pool_get_bytes(
    entropy_pool_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
) {
    VALIDATE_NOT_NULL(ctx, -1);
    VALIDATE_BUFFER(buffer, size, -1);
    
    pthread_mutex_lock(&ctx->pool_mutex);
    
    // Try to serve from pool first (cache hit)
    if (ctx->pool_available >= size) {
        // Copy from pool
        size_t read_pos = ctx->pool_used;
        
        if (read_pos + size <= ctx->pool_size) {
            // Simple copy
            memcpy(buffer, ctx->pool_buffer + read_pos, size);
        } else {
            // Wraparound copy
            size_t first_part = ctx->pool_size - read_pos;
            memcpy(buffer, ctx->pool_buffer + read_pos, first_part);
            memcpy(buffer + first_part, ctx->pool_buffer, size - first_part);
        }
        
        ctx->pool_used += size;
        if (ctx->pool_used >= ctx->pool_size) {
            ctx->pool_used -= ctx->pool_size;
        }
        
        ctx->pool_available -= size;
        ctx->stats.cache_hits++;
        ctx->stats.bytes_generated += size;
        
        // Signal refill if needed
        if (ctx->pool_available < ctx->config.refill_threshold) {
            pthread_cond_signal(&ctx->refill_cond);
        }
        
        pthread_mutex_unlock(&ctx->pool_mutex);
        return 0;
    }
    
    // Cache miss - need to generate fresh entropy
    ctx->stats.cache_misses++;
    pthread_mutex_unlock(&ctx->pool_mutex);
    
    // Generate directly (bypassing pool for large requests)
    entropy_error_t err = entropy_get_bytes(ctx->entropy_ctx, buffer, size);
    if (err != ENTROPY_SUCCESS) {
        return -1;
    }
    
    // Test generated entropy. Serialize health_ctx access — the background
    // worker thread runs the same tests on the shared context concurrently.
    pthread_mutex_lock(&ctx->health_mutex);
    health_error_t health_err = health_tests_run_batch(ctx->health_ctx, buffer, size);
    pthread_mutex_unlock(&ctx->health_mutex);
    if (health_err != HEALTH_SUCCESS) {
        secure_memzero(buffer, size);
        return -1;
    }
    
    pthread_mutex_lock(&ctx->pool_mutex);
    ctx->stats.bytes_generated += size;
    ctx->stats.cache_misses++;
    pthread_mutex_unlock(&ctx->pool_mutex);
    
    return 0;
}

int entropy_pool_refill(entropy_pool_ctx_t *ctx) {
    VALIDATE_NOT_NULL(ctx, -1);
    
    pthread_mutex_lock(&ctx->pool_mutex);
    pthread_cond_signal(&ctx->refill_cond);
    ctx->stats.refills_triggered++;
    pthread_mutex_unlock(&ctx->pool_mutex);
    
    return 0;
}

// ============================================================================
// MONITORING
// ============================================================================

int entropy_pool_get_stats(
    const entropy_pool_ctx_t *ctx,
    entropy_pool_stats_t *stats
) {
    VALIDATE_NOT_NULL(ctx, -1);
    VALIDATE_NOT_NULL(stats, -1);
    
    pthread_mutex_lock((pthread_mutex_t*)&ctx->pool_mutex);
    memcpy(stats, &ctx->stats, sizeof(*stats));
    stats->current_fill_level = ctx->pool_available;
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->pool_mutex);
    
    return 0;
}

size_t entropy_pool_get_fill_level(const entropy_pool_ctx_t *ctx) {
    if (!ctx) return 0;
    
    pthread_mutex_lock((pthread_mutex_t*)&ctx->pool_mutex);
    size_t level = ctx->pool_available;
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->pool_mutex);
    
    return level;
}

int entropy_pool_needs_refill(const entropy_pool_ctx_t *ctx) {
    if (!ctx) return 0;
    
    pthread_mutex_lock((pthread_mutex_t*)&ctx->pool_mutex);
    int needs = (ctx->pool_available < ctx->config.refill_threshold);
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->pool_mutex);
    
    return needs;
}

void entropy_pool_print_stats(const entropy_pool_ctx_t *ctx) {
    if (!ctx) return;
    
    entropy_pool_stats_t stats;
    if (entropy_pool_get_stats(ctx, &stats) != 0) {
        return;
    }
    
    printf("=== Entropy Pool Statistics ===\n");
    printf("Configuration:\n");
    printf("  Pool size:          %zu bytes\n", ctx->pool_size);
    printf("  Refill threshold:   %zu bytes\n", ctx->config.refill_threshold);
    printf("  Chunk size:         %zu bytes\n", ctx->config.chunk_size);
    printf("\n");
    printf("Status:\n");
    printf("  Current fill level: %zu bytes (%.1f%%)\n",
           stats.current_fill_level,
           100.0 * stats.current_fill_level / ctx->pool_size);
    printf("  Background active:  %s\n", stats.background_active ? "Yes" : "No");
    printf("\n");
    printf("Performance:\n");
    printf("  Bytes generated:    %llu\n", (unsigned long long)stats.bytes_generated);
    printf("  Cache hits:         %llu\n", (unsigned long long)stats.cache_hits);
    printf("  Cache misses:       %llu\n", (unsigned long long)stats.cache_misses);
    
    if (stats.cache_hits + stats.cache_misses > 0) {
        double hit_rate = 100.0 * stats.cache_hits / (stats.cache_hits + stats.cache_misses);
        printf("  Cache hit rate:     %.1f%%\n", hit_rate);
    }
    
    printf("  Refills triggered:  %llu\n", (unsigned long long)stats.refills_triggered);
    printf("  Background chunks:  %llu\n", (unsigned long long)stats.background_chunks);
    printf("\n");
}
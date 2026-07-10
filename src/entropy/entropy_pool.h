#ifndef ENTROPY_POOL_H
#define ENTROPY_POOL_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include "hardware_entropy.h"
#include "../health/health_tests.h"

/**
 * @file entropy_pool.h
 * @brief Advanced entropy pooling with background pre-generation
 *
 * Provides a high-performance entropy pool that:
 * - Pre-generates tested entropy in background thread
 * - Reduces latency for entropy requests
 * - Maintains continuous health testing
 * - Thread-safe access with lock-free reads when possible
 *
 * Performance benefits:
 * - Near-zero latency for cached entropy
 * - Continuous background collection
 * - Automatic refilling
 * - Burst request handling
 */

// ============================================================================
// CONFIGURATION
// ============================================================================

#define ENTROPY_POOL_DEFAULT_SIZE (64 * 1024)  // 64KB pool
#define ENTROPY_POOL_REFILL_THRESHOLD (16 * 1024)  // Refill at 25%
#define ENTROPY_POOL_CHUNK_SIZE 4096  // Generate 4KB chunks

/**
 * @brief Entropy pool configuration
 */
typedef struct {
    size_t pool_size;              /**< Total pool size in bytes */
    size_t refill_threshold;       /**< Trigger refill when below this */
    size_t chunk_size;             /**< Size of generation chunks */
    int enable_background_thread;  /**< Enable background generation */
    double min_entropy;            /**< Min-entropy for health tests */
} entropy_pool_config_t;

/**
 * @brief Entropy pool statistics
 */
typedef struct {
    uint64_t bytes_generated;      /**< Total bytes generated */
    uint64_t cache_hits;           /**< Requests served from cache */
    uint64_t cache_misses;         /**< Requests requiring generation */
    uint64_t refills_triggered;    /**< Number of refill operations */
    uint64_t background_chunks;    /**< Chunks generated in background */
    size_t current_fill_level;     /**< Current pool fill level */
    int background_active;         /**< Background thread status */
} entropy_pool_stats_t;

/**
 * @brief Entropy pool context
 */
typedef struct {
    // Configuration
    entropy_pool_config_t config;
    
    // Pool storage
    uint8_t *pool_buffer;          /**< Entropy pool buffer */
    size_t pool_size;              /**< Total pool size */
    size_t pool_used;              /**< Bytes consumed from pool */
    size_t pool_available;         /**< Bytes available in pool */
    
    // Thread safety
    pthread_mutex_t pool_mutex;    /**< Pool access mutex */
    pthread_cond_t refill_cond;    /**< Refill condition variable */
    pthread_t background_thread;   /**< Background generation thread */
    int background_running;        /**< Background thread running flag */
    int shutdown_requested;        /**< Shutdown flag */
    
    // Components
    entropy_ctx_t *entropy_ctx;    /**< Hardware entropy context */
    health_test_ctx_t *health_ctx; /**< Health test context */
    
    // Statistics
    entropy_pool_stats_t stats;
} entropy_pool_ctx_t;

// ============================================================================
// POOL MANAGEMENT
// ============================================================================

/**
 * @brief Initialize entropy pool with default configuration
 *
 * @param ctx Output context pointer
 * @return 0 on success, -1 on error
 */
int entropy_pool_init(entropy_pool_ctx_t **ctx);

/**
 * @brief Initialize entropy pool with custom configuration
 *
 * @param ctx Output context pointer
 * @param config Custom configuration
 * @return 0 on success, -1 on error
 */
int entropy_pool_init_with_config(
    entropy_pool_ctx_t **ctx,
    const entropy_pool_config_t *config
);

/**
 * @brief Free entropy pool context
 *
 * Stops background thread and securely erases pool.
 *
 * @param ctx Pool context
 */
void entropy_pool_free(entropy_pool_ctx_t *ctx);

/**
 * @brief Start background entropy generation
 *
 * Launches background thread that continuously refills the pool.
 *
 * @param ctx Pool context
 * @return 0 on success, -1 on error
 */
int entropy_pool_start_background(entropy_pool_ctx_t *ctx);

/**
 * @brief Stop background entropy generation
 *
 * @param ctx Pool context
 */
void entropy_pool_stop_background(entropy_pool_ctx_t *ctx);

// ============================================================================
// ENTROPY RETRIEVAL
// ============================================================================

/**
 * @brief Get entropy from pool
 *
 * Retrieves health-tested entropy from pool. If pool is empty,
 * generates fresh entropy immediately.
 *
 * @param ctx Pool context
 * @param buffer Output buffer
 * @param size Number of bytes requested
 * @return 0 on success, -1 on error
 */
int entropy_pool_get_bytes(
    entropy_pool_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
);

/**
 * @brief Refill entropy pool
 *
 * Manually triggers pool refill with fresh tested entropy.
 *
 * @param ctx Pool context
 * @return 0 on success, -1 on error
 */
int entropy_pool_refill(entropy_pool_ctx_t *ctx);

// ============================================================================
// MONITORING
// ============================================================================

/**
 * @brief Get pool statistics
 *
 * @param ctx Pool context
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int entropy_pool_get_stats(
    const entropy_pool_ctx_t *ctx,
    entropy_pool_stats_t *stats
);

/**
 * @brief Get current fill level
 *
 * @param ctx Pool context
 * @return Number of bytes available in pool
 */
size_t entropy_pool_get_fill_level(const entropy_pool_ctx_t *ctx);

/**
 * @brief Check if pool needs refill
 *
 * @param ctx Pool context
 * @return 1 if refill needed, 0 otherwise
 */
int entropy_pool_needs_refill(const entropy_pool_ctx_t *ctx);

/**
 * @brief Print pool statistics
 *
 * @param ctx Pool context
 */
void entropy_pool_print_stats(const entropy_pool_ctx_t *ctx);

#endif /* ENTROPY_POOL_H */
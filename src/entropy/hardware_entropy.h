#ifndef HARDWARE_ENTROPY_H
#define HARDWARE_ENTROPY_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * @file hardware_entropy.h
 * @brief Hardware entropy source abstraction layer
 * 
 * Provides unified interface to multiple hardware entropy sources:
 * - Intel RDRAND (CPU-based TRNG)
 * - Intel RDSEED (Enhanced TRNG with conditioning)
 * - /dev/random (kernel entropy pool)
 * - /dev/urandom (non-blocking kernel entropy)
 * - getrandom() syscall (modern Linux)
 * - CPU jitter entropy (timing uncertainty)
 * 
 * Implements fallback chain for maximum portability and security.
 */

/**
 * @brief Entropy source types
 */
typedef enum {
    ENTROPY_SOURCE_RDSEED,      // Intel RDSEED instruction
    ENTROPY_SOURCE_RDRAND,      // Intel RDRAND instruction
    ENTROPY_SOURCE_GETRANDOM,   // getrandom() syscall
    ENTROPY_SOURCE_DEV_RANDOM,  // /dev/random device
    ENTROPY_SOURCE_DEV_URANDOM, // /dev/urandom device
    ENTROPY_SOURCE_JITTER,      // CPU jitter timing
    ENTROPY_SOURCE_FALLBACK,    // System fallback
    ENTROPY_SOURCE_NONE         // No source available
} entropy_source_type_t;

/**
 * @brief Entropy source capabilities
 */
typedef struct {
    int has_rdrand;             // RDRAND available
    int has_rdseed;             // RDSEED available
    int has_getrandom;          // getrandom() available
    int has_dev_random;         // /dev/random available
    int has_dev_urandom;        // /dev/urandom available
    int has_jitter;             // Jitter source available
    entropy_source_type_t preferred_source;
} entropy_capabilities_t;

/**
 * @brief Entropy context for state management
 */
typedef struct {
    entropy_capabilities_t caps;
    int dev_random_fd;          // /dev/random file descriptor
    int dev_urandom_fd;         // /dev/urandom file descriptor
    uint64_t rdrand_failures;   // RDRAND failure count
    uint64_t rdseed_failures;   // RDSEED failure count
    uint64_t total_bytes;       // Total entropy collected
    entropy_source_type_t last_source; // Last successful source
} entropy_ctx_t;

/**
 * @brief Error codes
 */
typedef enum {
    ENTROPY_SUCCESS = 0,
    ENTROPY_ERROR_NO_SOURCE = -1,
    ENTROPY_ERROR_INSUFFICIENT = -2,
    ENTROPY_ERROR_SYSCALL = -3,
    ENTROPY_ERROR_TIMEOUT = -4,
    ENTROPY_ERROR_INVALID_PARAM = -5
} entropy_error_t;

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize entropy context
 * 
 * Detects available entropy sources and initializes state.
 * 
 * @param ctx Entropy context to initialize
 * @return ENTROPY_SUCCESS or error code
 */
entropy_error_t entropy_init(entropy_ctx_t *ctx);

/**
 * @brief Free entropy context resources
 * 
 * @param ctx Entropy context to free
 */
void entropy_free(entropy_ctx_t *ctx);

/**
 * @brief Get entropy capabilities
 * 
 * @param ctx Entropy context
 * @return Capabilities structure
 */
entropy_capabilities_t entropy_get_capabilities(const entropy_ctx_t *ctx);

// ============================================================================
// ENTROPY COLLECTION
// ============================================================================

/**
 * @brief Collect entropy from best available source
 * 
 * Tries sources in priority order:
 * 1. RDSEED (highest quality)
 * 2. RDRAND (high quality)
 * 3. getrandom() (kernel quality)
 * 4. /dev/random (blocking, highest quality)
 * 5. /dev/urandom (non-blocking)
 * 6. Jitter (timing-based)
 * 
 * @param ctx Entropy context
 * @param buffer Output buffer
 * @param size Number of bytes to collect
 * @return ENTROPY_SUCCESS or error code
 */
entropy_error_t entropy_get_bytes(entropy_ctx_t *ctx, uint8_t *buffer, size_t size);

/**
 * @brief Collect entropy from specific source
 * 
 * @param ctx Entropy context
 * @param buffer Output buffer
 * @param size Number of bytes to collect
 * @param source Specific entropy source to use
 * @return ENTROPY_SUCCESS or error code
 */
entropy_error_t entropy_get_bytes_from_source(
    entropy_ctx_t *ctx,
    uint8_t *buffer,
    size_t size,
    entropy_source_type_t source
);

/**
 * @brief Get 64-bit entropy value
 * 
 * @param ctx Entropy context
 * @param value Output value
 * @return ENTROPY_SUCCESS or error code
 */
entropy_error_t entropy_get_uint64(entropy_ctx_t *ctx, uint64_t *value);

// ============================================================================
// HARDWARE INSTRUCTIONS
// ============================================================================

/**
 * @brief Get entropy via RDRAND instruction
 * 
 * @param value Output value
 * @return 1 on success, 0 on failure
 */
int rdrand_get_uint64(uint64_t *value);

/**
 * @brief Get entropy via RDSEED instruction
 * 
 * @param value Output value
 * @return 1 on success, 0 on failure
 */
int rdseed_get_uint64(uint64_t *value);

/**
 * @brief Check if RDRAND is available
 * 
 * @return 1 if available, 0 otherwise
 */
int rdrand_available(void);

/**
 * @brief Check if RDSEED is available
 * 
 * @return 1 if available, 0 otherwise
 */
int rdseed_available(void);

// ============================================================================
// SYSTEM ENTROPY
// ============================================================================

/**
 * @brief Get entropy via getrandom() syscall
 * 
 * @param buffer Output buffer
 * @param size Number of bytes
 * @param flags getrandom flags
 * @return Number of bytes read, or -1 on error
 */
ssize_t entropy_getrandom(uint8_t *buffer, size_t size, unsigned int flags);

/**
 * @brief Read from /dev/random
 * 
 * @param ctx Entropy context
 * @param buffer Output buffer
 * @param size Number of bytes
 * @param blocking 1 for blocking, 0 for non-blocking
 * @return Number of bytes read, or -1 on error
 */
ssize_t entropy_dev_random(entropy_ctx_t *ctx, uint8_t *buffer, size_t size, int blocking);

/**
 * @brief Read from /dev/urandom
 * 
 * @param ctx Entropy context
 * @param buffer Output buffer
 * @param size Number of bytes
 * @return Number of bytes read, or -1 on error
 */
ssize_t entropy_dev_urandom(entropy_ctx_t *ctx, uint8_t *buffer, size_t size);

// ============================================================================
// JITTER ENTROPY
// ============================================================================

/**
 * @brief Get entropy from CPU timing jitter
 * 
 * Measures timing variations in CPU execution to extract entropy.
 * Based on CPU timing uncertainty principle.
 * 
 * @param buffer Output buffer
 * @param size Number of bytes to generate
 * @return ENTROPY_SUCCESS or error code
 */
entropy_error_t entropy_jitter(uint8_t *buffer, size_t size);

// ============================================================================
// QUALITY ASSESSMENT
// ============================================================================

/**
 * @brief Estimate entropy quality from source
 * 
 * Returns estimated bits of entropy per byte:
 * - RDSEED: 8.0 bits/byte (full entropy)
 * - RDRAND: 8.0 bits/byte (full entropy)
 * - /dev/random: 8.0 bits/byte (kernel certified)
 * - /dev/urandom: 7.5-8.0 bits/byte (high quality)
 * - Jitter: 4.0-6.0 bits/byte (variable quality)
 * 
 * @param source Entropy source type
 * @return Estimated bits per byte
 */
double entropy_quality_estimate(entropy_source_type_t source);

/**
 * @brief Get source name string
 * 
 * @param source Entropy source type
 * @return Human-readable source name
 */
const char* entropy_source_name(entropy_source_type_t source);

/**
 * @brief Print entropy statistics
 * 
 * @param ctx Entropy context
 */
void entropy_print_stats(const entropy_ctx_t *ctx);

#endif /* HARDWARE_ENTROPY_H */
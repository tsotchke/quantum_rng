#ifndef QUANTUM_ENTROPY_H
#define QUANTUM_ENTROPY_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file quantum_entropy.h
 * @brief Cryptographically secure entropy interface for quantum operations
 * 
 * Provides secure random number generation for quantum measurement simulation
 * and sampling operations. This interface allows quantum modules to access
 * cryptographic-quality randomness without creating circular dependencies.
 * 
 * SECURITY CRITICAL: Quantum measurements must use cryptographically secure
 * randomness. Using predictable sources like stdlib rand() completely
 * undermines the security of the quantum RNG system.
 */

/**
 * @brief Entropy source function pointer type
 * 
 * Function that provides cryptographically secure random bytes.
 * Must be thread-safe if used in concurrent contexts.
 * 
 * @param user_data Opaque pointer to entropy context
 * @param buffer Output buffer for random bytes
 * @param size Number of bytes to generate
 * @return 0 on success, non-zero on error
 */
typedef int (*quantum_entropy_fn)(void *user_data, uint8_t *buffer, size_t size);

/**
 * @brief Quantum entropy context
 * 
 * Encapsulates entropy source for quantum operations.
 * Can be backed by secure_rng, hardware entropy, or other sources.
 */
typedef struct {
    quantum_entropy_fn get_bytes;  /**< Entropy generation function */
    void *user_data;               /**< Opaque context for entropy source */
} quantum_entropy_ctx_t;

/**
 * @brief Initialize quantum entropy context
 * 
 * @param ctx Entropy context to initialize
 * @param get_bytes Entropy function
 * @param user_data User data for entropy function
 */
static inline void quantum_entropy_init(
    quantum_entropy_ctx_t *ctx,
    quantum_entropy_fn get_bytes,
    void *user_data
) {
    if (!ctx) return;
    ctx->get_bytes = get_bytes;
    ctx->user_data = user_data;
}

/**
 * @brief Get cryptographically secure random bytes
 * 
 * @param ctx Entropy context
 * @param buffer Output buffer
 * @param size Number of bytes to generate
 * @return 0 on success, non-zero on error
 */
static inline int quantum_entropy_get_bytes(
    quantum_entropy_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
) {
    if (!ctx || !ctx->get_bytes || !buffer || size == 0) {
        return -1;
    }
    return ctx->get_bytes(ctx->user_data, buffer, size);
}

/**
 * @brief Get cryptographically secure random double in [0, 1)
 * 
 * @param ctx Entropy context
 * @param value Output pointer for random double
 * @return 0 on success, non-zero on error
 */
static inline int quantum_entropy_get_double(
    quantum_entropy_ctx_t *ctx,
    double *value
) {
    if (!ctx || !value) return -1;
    
    uint64_t random_bits;
    int err = quantum_entropy_get_bytes(ctx, (uint8_t*)&random_bits, sizeof(random_bits));
    if (err != 0) return err;
    
    // Convert to double in [0, 1) using 53 bits of precision
    *value = (double)(random_bits >> 11) * 0x1.0p-53;
    return 0;
}

/**
 * @brief Get cryptographically secure random uint64
 * 
 * @param ctx Entropy context
 * @param value Output pointer for random uint64
 * @return 0 on success, non-zero on error
 */
static inline int quantum_entropy_get_uint64(
    quantum_entropy_ctx_t *ctx,
    uint64_t *value
) {
    if (!ctx || !value) return -1;
    return quantum_entropy_get_bytes(ctx, (uint8_t*)value, sizeof(*value));
}

#endif /* QUANTUM_ENTROPY_H */
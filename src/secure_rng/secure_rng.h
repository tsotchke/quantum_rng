#ifndef SECURE_RNG_H
#define SECURE_RNG_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include "../quantum_rng/quantum_rng.h"
#include "../entropy/hardware_entropy.h"
#include "../health/health_tests.h"

/**
 * @file secure_rng.h
 * @brief Production-grade secure random number generator
 *
 * This module integrates three critical components:
 * 1. Hardware entropy sources (RDSE ED, RDRAND, /dev/random, etc.)
 * 2. NIST SP 800-90B health tests (continuous monitoring)
 * 3. Quantum-inspired random number generation (high-quality output)
 *
 * The result is a FIPS 140-3 compliant, cryptographically secure RNG
 * suitable for production use in security-critical applications.
 *
 * Features:
 * - Continuous health testing of entropy sources
 * - Multiple entropy source fallbacks
 * - Quantum mixing for enhanced randomness
 * - Thread-safe operation (with proper usage)
 * - Automatic reseeding
 * - Comprehensive error handling
 * - Performance optimized
 */

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief RNG operation modes
 *
 * Controls the balance between performance and quantum security:
 * - FAST: Maximum performance, hardware entropy only (no quantum mixing)
 * - QUANTUM: Full quantum mixing, health-tested entropy (default)
 * - HYBRID: Adaptive mode switching based on request size
 * - VERIFIED: Quantum + Bell test verification (slowest, maximum assurance)
 */
typedef enum {
    SECURE_RNG_MODE_FAST = 0,      /**< Hardware entropy only, max performance */
    SECURE_RNG_MODE_QUANTUM,       /**< Quantum mixing + health tests (default) */
    SECURE_RNG_MODE_HYBRID,        /**< Adaptive: FAST for small, QUANTUM for large */
    SECURE_RNG_MODE_VERIFIED       /**< Quantum + Bell test verification */
} secure_rng_mode_t;

/**
 * @brief Secure RNG configuration parameters
 */
typedef struct {
    // Mode configuration
    secure_rng_mode_t mode;           /**< Operation mode */
    size_t hybrid_threshold;          /**< Bytes threshold for HYBRID mode (default: 1024) */

    // Health test configuration
    double min_entropy_estimate;      /**< Min-entropy estimate (bits/byte) */
    uint32_t rct_cutoff;              /**< RCT failure threshold */
    uint32_t apt_cutoff;              /**< APT failure threshold */
    uint32_t apt_window_size;         /**< APT window size */
    uint32_t startup_test_samples;    /**< Startup test sample count */

    // Reseeding configuration
    uint64_t reseed_interval;         /**< Bytes before forced reseed (0=never) */
    int auto_reseed_enabled;          /**< Enable automatic reseeding */

    // Entropy source configuration
    entropy_source_type_t preferred_source;  /**< Preferred entropy source */
    int use_multiple_sources;         /**< Mix multiple sources if available */

    // Security configuration
    int require_hardware_entropy;     /**< Fail if no hardware entropy available */
    int zeroize_on_error;            /**< Zero buffers on error */

    // Performance configuration
    size_t entropy_cache_size;        /**< Size of entropy cache (0=no cache) */
    
    // Thread safety configuration
    int enable_thread_safety;         /**< Enable pthread mutex locking */
} secure_rng_config_t;

/**
 * @brief Secure RNG error codes
 */
typedef enum {
    SECURE_RNG_SUCCESS = 0,                    /**< Operation successful */
    SECURE_RNG_ERROR_NULL_CONTEXT = -1,        /**< NULL context provided */
    SECURE_RNG_ERROR_NULL_BUFFER = -2,         /**< NULL buffer provided */
    SECURE_RNG_ERROR_INVALID_PARAM = -3,       /**< Invalid parameter */
    SECURE_RNG_ERROR_HEALTH_TEST_FAILED = -4,  /**< Health test failure */
    SECURE_RNG_ERROR_ENTROPY_FAILURE = -5,     /**< Entropy source failure */
    SECURE_RNG_ERROR_INITIALIZATION = -6,      /**< Initialization failed */
    SECURE_RNG_ERROR_NOT_INITIALIZED = -7,     /**< Context not initialized */
    SECURE_RNG_ERROR_STARTUP_FAILED = -8,      /**< Startup tests failed */
    SECURE_RNG_ERROR_INSUFFICIENT_ENTROPY = -9, /**< Not enough entropy */
    SECURE_RNG_ERROR_INVALID_RANGE = -10,      /**< Invalid range parameters */
    SECURE_RNG_ERROR_MUTEX_LOCK = -11,         /**< Mutex lock failed */
    SECURE_RNG_ERROR_MUTEX_UNLOCK = -12        /**< Mutex unlock failed */
} secure_rng_error_t;

/**
 * @brief Secure RNG context state
 */
typedef enum {
    SECURE_RNG_STATE_UNINITIALIZED = 0,  /**< Not initialized */
    SECURE_RNG_STATE_STARTUP,            /**< Running startup tests */
    SECURE_RNG_STATE_OPERATIONAL,        /**< Normal operation */
    SECURE_RNG_STATE_ERROR,              /**< Error state */
    SECURE_RNG_STATE_SHUTDOWN            /**< Shut down */
} secure_rng_state_t;

/**
 * @brief Secure RNG statistics
 */
typedef struct {
    // Generation statistics
    uint64_t bytes_generated;          /**< Total bytes generated */
    uint64_t requests_served;          /**< Total requests served */
    uint64_t reseed_count;             /**< Number of reseeds */

    // Health test statistics
    uint64_t health_test_failures;     /**< Total health test failures */
    uint64_t rct_failures;             /**< RCT failures */
    uint64_t apt_failures;             /**< APT failures */

    // Entropy statistics
    uint64_t entropy_bytes_consumed;   /**< Raw entropy consumed */
    entropy_source_type_t primary_source;  /**< Primary entropy source */

    // Performance statistics
    uint64_t cache_hits;               /**< Entropy cache hits */
    uint64_t cache_misses;             /**< Entropy cache misses */
    
    // Mode statistics
    uint64_t fast_mode_bytes;          /**< Bytes generated in FAST mode */
    uint64_t quantum_mode_bytes;       /**< Bytes generated in QUANTUM mode */
    uint64_t verified_mode_bytes;      /**< Bytes generated in VERIFIED mode */

    // State
    secure_rng_state_t state;          /**< Current state */
    secure_rng_mode_t current_mode;    /**< Current operation mode */
    time_t last_reseed_time;           /**< Last reseed timestamp */
} secure_rng_stats_t;

/**
 * @brief Secure RNG context
 */
typedef struct {
    // Component contexts
    qrng_ctx *qrng_ctx;                /**< Quantum RNG context */
    entropy_ctx_t *entropy_ctx;        /**< Entropy source context */
    health_test_ctx_t *health_ctx;     /**< Health test context */

    // Configuration
    secure_rng_config_t config;        /**< Configuration */

    // State
    secure_rng_state_t state;          /**< Current state */
    secure_rng_stats_t stats;          /**< Statistics */

    // Entropy cache (optional)
    uint8_t *entropy_cache;            /**< Cached entropy buffer */
    size_t cache_size;                 /**< Cache size */
    size_t cache_used;                 /**< Bytes used from cache */

    // Reseeding tracking
    uint64_t bytes_since_reseed;       /**< Bytes since last reseed */

    // VERIFIED-mode Bell certification
    int bell_certified;                /**< 1 once the quantum source has passed a CHSH Bell test this epoch */
    double last_chsh_value;            /**< Most recent measured CHSH S value (0 until first certification) */

    // Thread safety
    int thread_safe;                   /**< Thread-safety enabled flag */
    pthread_rwlock_t rwlock;           /**< Read-write lock for thread safety */
    int rwlock_initialized;            /**< RW-lock initialization flag */

    // Error callback
    void (*error_callback)(secure_rng_error_t error, const char *msg, void *user_data);
    void *callback_user_data;
} secure_rng_ctx_t;

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

/**
 * @brief Get default configuration
 *
 * Returns a configuration structure with secure default values suitable
 * for most applications.
 *
 * @param config Output configuration structure
 */
void secure_rng_get_default_config(secure_rng_config_t *config);

/**
 * @brief Initialize secure RNG with default configuration
 *
 * Creates and initializes a new secure RNG context with default settings.
 * The context must be freed with secure_rng_free() when no longer needed.
 *
 * Initialization process:
 * 1. Initialize entropy sources
 * 2. Initialize health tests
 * 3. Run startup health tests (NIST SP 800-90B requirement)
 * 4. Initialize quantum RNG with tested entropy
 * 5. Verify all components operational
 *
 * @param ctx Output context pointer
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_init(secure_rng_ctx_t **ctx);

/**
 * @brief Initialize secure RNG with custom configuration
 *
 * @param ctx Output context pointer
 * @param config Custom configuration
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_init_with_config(
    secure_rng_ctx_t **ctx,
    const secure_rng_config_t *config
);

/**
 * @brief Free secure RNG context
 *
 * Securely erases all sensitive data and frees resources.
 *
 * @param ctx Context to free
 */
void secure_rng_free(secure_rng_ctx_t *ctx);

/**
 * @brief Reset secure RNG context
 *
 * Resets statistics and reseeds the RNG. Does not change configuration.
 *
 * @param ctx Context to reset
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_reset(secure_rng_ctx_t *ctx);

/**
 * @brief Initialize thread-safe RNG context
 *
 * Same as secure_rng_init() but enables pthread mutex locking for
 * safe concurrent access from multiple threads. Slightly slower due
 * to mutex overhead, but allows sharing a single context across threads.
 *
 * @param ctx Output context pointer
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_init_threadsafe(secure_rng_ctx_t **ctx);

/**
 * @brief Initialize thread-safe RNG with custom configuration
 *
 * @param ctx Output context pointer
 * @param config Custom configuration
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_init_threadsafe_with_config(
    secure_rng_ctx_t **ctx,
    const secure_rng_config_t *config
);

// ============================================================================
// RANDOM NUMBER GENERATION
// ============================================================================

/**
 * @brief Generate random bytes
 *
 * Fills the output buffer with cryptographically secure random bytes.
 * All entropy is tested through health tests before use.
 *
 * Process:
 * 1. Check if reseed needed
 * 2. Collect raw entropy
 * 3. Run health tests on entropy
 * 4. Mix entropy through quantum RNG
 * 5. Output final random bytes
 *
 * @param ctx Secure RNG context
 * @param buffer Output buffer
 * @param size Number of bytes to generate
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_bytes(
    secure_rng_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
);

/**
 * @brief Generate random 64-bit unsigned integer
 *
 * @param ctx Secure RNG context
 * @param value Output value pointer
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_uint64(
    secure_rng_ctx_t *ctx,
    uint64_t *value
);

/**
 * @brief Generate random 32-bit unsigned integer
 *
 * @param ctx Secure RNG context
 * @param value Output value pointer
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_uint32(
    secure_rng_ctx_t *ctx,
    uint32_t *value
);

/**
 * @brief Generate random double in [0, 1)
 *
 * @param ctx Secure RNG context
 * @param value Output value pointer
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_double(
    secure_rng_ctx_t *ctx,
    double *value
);

/**
 * @brief Generate random integer in range [min, max]
 *
 * Uses unbiased sampling to ensure uniform distribution.
 *
 * @param ctx Secure RNG context
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @param value Output value pointer
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_range32(
    secure_rng_ctx_t *ctx,
    int32_t min,
    int32_t max,
    int32_t *value
);

/**
 * @brief Generate random 64-bit integer in range [min, max]
 *
 * @param ctx Secure RNG context
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @param value Output value pointer
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_range64(
    secure_rng_ctx_t *ctx,
    uint64_t min,
    uint64_t max,
    uint64_t *value
);

// ============================================================================
// RESEEDING
// ============================================================================

/**
 * @brief Manually reseed the RNG
 *
 * Forces an immediate reseed with fresh entropy from hardware sources.
 * All entropy is tested through health tests.
 *
 * @param ctx Secure RNG context
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_reseed(secure_rng_ctx_t *ctx);

/**
 * @brief Reseed with additional external entropy
 *
 * Mixes external entropy into the RNG state. External entropy is still
 * tested through health tests before use.
 *
 * @param ctx Secure RNG context
 * @param external_entropy External entropy bytes
 * @param size Size of external entropy
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_reseed_with_entropy(
    secure_rng_ctx_t *ctx,
    const uint8_t *external_entropy,
    size_t size
);

// ============================================================================
// STATUS & MONITORING
// ============================================================================

/**
 * @brief Get RNG statistics
 *
 * @param ctx Secure RNG context
 * @param stats Output statistics structure
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_get_stats(
    const secure_rng_ctx_t *ctx,
    secure_rng_stats_t *stats
);

/**
 * @brief Get current RNG state
 *
 * @param ctx Secure RNG context
 * @return Current state
 */
secure_rng_state_t secure_rng_get_state(const secure_rng_ctx_t *ctx);

/**
 * @brief Check if RNG is operational
 *
 * @param ctx Secure RNG context
 * @return 1 if operational, 0 otherwise
 */
int secure_rng_is_operational(const secure_rng_ctx_t *ctx);

/**
 * @brief Get health test statistics
 *
 * @param ctx Secure RNG context
 * @return Health test statistics (read-only)
 */
const health_test_stats_t* secure_rng_get_health_stats(const secure_rng_ctx_t *ctx);

/**
 * @brief Get entropy source capabilities
 *
 * @param ctx Secure RNG context
 * @return Entropy capabilities (read-only)
 */
const entropy_capabilities_t* secure_rng_get_entropy_caps(const secure_rng_ctx_t *ctx);

/**
 * @brief Print detailed statistics
 *
 * Prints comprehensive statistics including health tests, entropy sources,
 * and generation statistics.
 *
 * @param ctx Secure RNG context
 */
void secure_rng_print_stats(const secure_rng_ctx_t *ctx);

/**
 * @brief Get current operation mode
 *
 * @param ctx Secure RNG context
 * @return Current mode
 */
secure_rng_mode_t secure_rng_get_mode(const secure_rng_ctx_t *ctx);

/**
 * @brief Set operation mode
 *
 * Changes the RNG operation mode. Safe to call at any time.
 * Mode change takes effect on next generation request.
 *
 * @param ctx Secure RNG context
 * @param mode New operation mode
 * @return SECURE_RNG_SUCCESS or error code
 */
secure_rng_error_t secure_rng_set_mode(secure_rng_ctx_t *ctx, secure_rng_mode_t mode);

/**
 * @brief Get mode name string
 *
 * @param mode Operation mode
 * @return Mode name
 */
const char* secure_rng_mode_string(secure_rng_mode_t mode);

// ============================================================================
// ERROR HANDLING
// ============================================================================

/**
 * @brief Set error callback
 *
 * Callback is invoked when errors occur during RNG operation.
 * Useful for logging, alerting, or custom error handling.
 *
 * @param ctx Secure RNG context
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void secure_rng_set_error_callback(
    secure_rng_ctx_t *ctx,
    void (*callback)(secure_rng_error_t error, const char *msg, void *user_data),
    void *user_data
);

/**
 * @brief Get error string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* secure_rng_error_string(secure_rng_error_t error);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Run self-test
 *
 * Performs comprehensive self-test of all components:
 * - Entropy source functionality
 * - Health test detection capabilities
 * - Quantum RNG operation
 * - Integration correctness
 *
 * @param verbose Print detailed test output
 * @return 1 if all tests pass, 0 if any fail
 */
int secure_rng_self_test(int verbose);

/**
 * @brief Get library version
 *
 * @return Version string
 */
const char* secure_rng_version(void);

#endif /* SECURE_RNG_H */

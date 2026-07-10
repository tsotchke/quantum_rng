#ifndef QUANTUM_RNG_V3_H
#define QUANTUM_RNG_V3_H

#include <stdint.h>
#include <stddef.h>
#include "quantum_state.h"
#include "quantum_gates.h"
#include "quantum_entropy.h"
#include "bell_test.h"
#include "grover.h"
#include "../entropy/entropy_pool.h"
#include "../profiling/performance_monitor.h"

/**
 * @file quantum_rng_v3.h
 * @brief Unified Quantum RNG v3.0 - Production quantum engine integration
 * 
 * This is the NEW unified quantum RNG that integrates:
 * - Proven quantum simulation engine (Bell verified CHSH=2.828)
 * - Hardware entropy pool with background generation
 * - Continuous Bell test monitoring
 * - Advanced Grover-based sampling
 * - SIMD-optimized operations
 * 
 * REPLACES: quantum_rng.c (legacy quantum-inspired implementation)
 * INTEGRATES: quantum_state.c + quantum_gates.c + bell_test.c + grover.c
 * 
 * Key improvements over v1.0:
 * - Actually uses the Bell-verified quantum engine
 * - Resolves entropy circular dependency (layered architecture)
 * - 10-20x faster Bell tests
 * - Advanced Grover sampling modes
 * - Runtime quantum verification
 */

// ============================================================================
// VERSION INFORMATION
// ============================================================================

#define QUANTUM_RNG_V3_VERSION_MAJOR 3
#define QUANTUM_RNG_V3_VERSION_MINOR 0
#define QUANTUM_RNG_V3_VERSION_PATCH 0

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief Quantum RNG operation modes
 */
typedef enum {
    QRNG_V3_MODE_DIRECT,       /**< Direct quantum measurement (fastest) */
    QRNG_V3_MODE_GROVER,       /**< Grover-enhanced sampling */
    QRNG_V3_MODE_BELL_VERIFIED /**< With continuous Bell verification */
} qrng_v3_mode_t;

/**
 * @brief Configuration for quantum RNG v3
 */
typedef struct {
    // Quantum simulation
    size_t num_qubits;              /**< Qubits in quantum state (4-16) */
    qrng_v3_mode_t mode;            /**< Operation mode */
    
    // Bell test verification
    int enable_bell_monitoring;     /**< Enable continuous Bell tests */
    uint64_t bell_test_interval;    /**< Test every N bytes (0=disabled) */
    double min_acceptable_chsh;     /**< Minimum CHSH value (2.4 recommended) */
    
    // Grover optimization
    int enable_grover_cache;        /**< Cache Grover iterations */
    size_t grover_cache_size;       /**< Grover result cache size */
    
    // Entropy pool
    size_t entropy_pool_size;       /**< Hardware entropy pool size */
    int enable_background_entropy;  /**< Background entropy generation */
    
    // Output buffering
    size_t output_buffer_size;      /**< Output buffer size (larger = fewer refills) */
    
    // Performance
    int enable_simd;                /**< Use SIMD optimizations */
    int enable_performance_monitoring; /**< Track performance metrics */
} qrng_v3_config_t;

/**
 * @brief Error codes
 */
typedef enum {
    QRNG_V3_SUCCESS = 0,
    QRNG_V3_ERROR_NULL_CONTEXT = -1,
    QRNG_V3_ERROR_NULL_BUFFER = -2,
    QRNG_V3_ERROR_INVALID_PARAM = -3,
    QRNG_V3_ERROR_ENTROPY_FAILURE = -4,
    QRNG_V3_ERROR_QUANTUM_INIT = -5,
    QRNG_V3_ERROR_BELL_TEST_FAILED = -6,
    QRNG_V3_ERROR_OUT_OF_MEMORY = -7,
    QRNG_V3_ERROR_NOT_INITIALIZED = -8
} qrng_v3_error_t;

/**
 * @brief Statistics structure
 */
typedef struct {
    // Generation stats
    uint64_t bytes_generated;
    uint64_t quantum_measurements;
    uint64_t grover_searches;
    
    // Bell test stats
    uint64_t bell_tests_performed;
    uint64_t bell_tests_passed;
    double average_chsh;
    double min_chsh;
    double max_chsh;
    
    // Entropy stats
    uint64_t hardware_entropy_consumed;
    double current_entanglement_entropy;
    
    // Performance
    double avg_measurement_latency_ns;
    double throughput_mbps;
} qrng_v3_stats_t;

/**
 * @brief Quantum RNG v3 context
 */
typedef struct {
    // Configuration
    qrng_v3_config_t config;
    
    // Quantum simulation engine
    quantum_state_t *quantum_state;
    quantum_entropy_ctx_t entropy_ctx;  // For quantum measurements
    
    // Hardware entropy (base layer - no circular dependency!)
    entropy_pool_ctx_t *entropy_pool;
    
    // Bell test monitoring
    bell_test_monitor_t *bell_monitor;
    uint64_t bytes_since_bell_test;
    
    // Output buffering
    uint8_t *output_buffer;
    size_t output_buffer_size;
    size_t buffer_pos;
    
    // Grover caching
    uint64_t *grover_cache;
    size_t grover_cache_pos;
    
    // Performance monitoring
    perf_monitor_ctx_t *perf_monitor;
    
    // Statistics
    qrng_v3_stats_t stats;
    
    // State
    int initialized;
} qrng_v3_ctx_t;

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

/**
 * @brief Get default configuration
 * 
 * Returns recommended settings:
 * - 8 qubits (256-dimensional state space)
 * - DIRECT mode (fastest)
 * - Bell monitoring every 1MB
 * - Background entropy enabled
 * 
 * @param config Output configuration
 */
void qrng_v3_get_default_config(qrng_v3_config_t *config);

/**
 * @brief Initialize quantum RNG v3 with default configuration
 * 
 * Creates unified quantum RNG using proven Bell-verified engine.
 * Resolves entropy circular dependency through layered architecture:
 * 
 * Layer 1: Hardware entropy pool (RDSEED, /dev/random, etc.)
 *          ↓
 * Layer 2: Quantum simulation (measurements use Layer 1)
 *          ↓
 * Layer 3: RNG output (quantum-evolved, conditioned)
 * 
 * @param ctx Output context pointer
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_init(qrng_v3_ctx_t **ctx);

/**
 * @brief Initialize with custom configuration
 * 
 * @param ctx Output context pointer
 * @param config Custom configuration
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_init_with_config(
    qrng_v3_ctx_t **ctx,
    const qrng_v3_config_t *config
);

/**
 * @brief Free quantum RNG context
 * 
 * Securely erases all quantum states and entropy buffers.
 * 
 * @param ctx Context to free
 */
void qrng_v3_free(qrng_v3_ctx_t *ctx);

// ============================================================================
// RANDOM NUMBER GENERATION
// ============================================================================

/**
 * @brief Generate random bytes using quantum simulation
 * 
 * Process:
 * 1. Apply quantum gates to evolve state
 * 2. Measure qubits (using hardware entropy)
 * 3. Extract randomness from measurements
 * 4. Condition output through mixing
 * 5. Optional: Verify with Bell test
 * 
 * @param ctx Quantum RNG context
 * @param buffer Output buffer
 * @param size Number of bytes to generate
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_bytes(
    qrng_v3_ctx_t *ctx,
    uint8_t *buffer,
    size_t size
);

/**
 * @brief Generate uint64 using quantum simulation
 * 
 * @param ctx Quantum RNG context
 * @param value Output value pointer
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_uint64(qrng_v3_ctx_t *ctx, uint64_t *value);

/**
 * @brief Generate double in [0, 1) using quantum simulation
 * 
 * @param ctx Quantum RNG context
 * @param value Output value pointer
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_double(qrng_v3_ctx_t *ctx, double *value);

/**
 * @brief Generate integer in range using quantum simulation
 * 
 * @param ctx Quantum RNG context
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @param value Output value pointer
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_range(
    qrng_v3_ctx_t *ctx,
    uint64_t min,
    uint64_t max,
    uint64_t *value
);

// ============================================================================
// GROVER-ENHANCED SAMPLING (NEW!)
// ============================================================================

/**
 * @brief Generate sample using Grover's algorithm
 * 
 * Uses Grover search for quantum-enhanced sampling.
 * Provides √N speedup for certain distributions.
 * 
 * @param ctx Quantum RNG context
 * @param value Output value (0 to 2^num_qubits - 1)
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_grover_sample(
    qrng_v3_ctx_t *ctx,
    uint64_t *value
);

/**
 * @brief Generate samples from target distribution using Grover
 * 
 * Uses amplitude amplification to sample from arbitrary distributions.
 * 
 * @param ctx Quantum RNG context
 * @param target_distribution Probability distribution function
 * @param value Output sample
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_grover_sample_distribution(
    qrng_v3_ctx_t *ctx,
    double (*target_distribution)(uint64_t),
    uint64_t *value
);

/**
 * @brief Multi-target Grover search
 * 
 * Searches for multiple targets simultaneously.
 * 
 * @param ctx Quantum RNG context
 * @param targets Array of target states
 * @param num_targets Number of targets
 * @param found_index Output: which target was found
 * @param value Output: found value
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_grover_multi_target(
    qrng_v3_ctx_t *ctx,
    const uint64_t *targets,
    size_t num_targets,
    size_t *found_index,
    uint64_t *value
);

// ============================================================================
// QUANTUM VERIFICATION
// ============================================================================

/**
 * @brief Run Bell test to verify quantum behavior
 * 
 * Performs CHSH inequality test to prove genuine quantum properties.
 * Should be called periodically or on-demand.
 * 
 * @param ctx Quantum RNG context
 * @param num_measurements Number of measurements (recommend 1000+)
 * @return Bell test result
 */
bell_test_result_t qrng_v3_verify_quantum(
    qrng_v3_ctx_t *ctx,
    size_t num_measurements
);

/**
 * @brief Get entanglement entropy of current quantum state
 * 
 * @param ctx Quantum RNG context
 * @return Entanglement entropy in bits
 */
double qrng_v3_get_entanglement_entropy(const qrng_v3_ctx_t *ctx);

/**
 * @brief Check if quantum behavior is maintained
 * 
 * Returns 1 if recent Bell tests confirm quantum behavior,
 * 0 if quantum properties have degraded.
 * 
 * @param ctx Quantum RNG context
 * @return 1 if quantum, 0 otherwise
 */
int qrng_v3_is_quantum_verified(const qrng_v3_ctx_t *ctx);

// ============================================================================
// MODE CONTROL
// ============================================================================

/**
 * @brief Set operation mode
 * 
 * Modes:
 * - DIRECT: Direct quantum measurements (fastest)
 * - GROVER: Grover-enhanced sampling (quantum advantage)
 * - BELL_VERIFIED: Continuous Bell test monitoring (slowest, max assurance)
 * 
 * @param ctx Quantum RNG context
 * @param mode New operation mode
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_set_mode(qrng_v3_ctx_t *ctx, qrng_v3_mode_t mode);

/**
 * @brief Get current operation mode
 * 
 * @param ctx Quantum RNG context
 * @return Current mode
 */
qrng_v3_mode_t qrng_v3_get_mode(const qrng_v3_ctx_t *ctx);

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

/**
 * @brief Get statistics
 * 
 * @param ctx Quantum RNG context
 * @param stats Output statistics structure
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_get_stats(
    const qrng_v3_ctx_t *ctx,
    qrng_v3_stats_t *stats
);

/**
 * @brief Print detailed statistics
 * 
 * @param ctx Quantum RNG context
 */
void qrng_v3_print_stats(const qrng_v3_ctx_t *ctx);

/**
 * @brief Get Bell test history
 * 
 * @param ctx Quantum RNG context
 * @return Bell test monitor (read-only)
 */
const bell_test_monitor_t* qrng_v3_get_bell_history(const qrng_v3_ctx_t *ctx);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Get version string
 * 
 * @return Version string "major.minor.patch"
 */
const char* qrng_v3_version(void);

/**
 * @brief Get error string
 * 
 * @param error Error code
 * @return Human-readable error description
 */
const char* qrng_v3_error_string(qrng_v3_error_t error);

/**
 * @brief Get mode string
 * 
 * @param mode Operation mode
 * @return Mode name
 */
const char* qrng_v3_mode_string(qrng_v3_mode_t mode);

// ============================================================================
// BACKWARD COMPATIBILITY (with legacy quantum_rng.h)
// ============================================================================

/**
 * @brief Create v3 context from legacy seed (backward compatible)
 * 
 * Maintains compatibility with old qrng_init() API.
 * 
 * @param ctx Output context
 * @param seed Seed bytes (can be NULL)
 * @param seed_len Seed length
 * @return QRNG_V3_SUCCESS or error code
 */
qrng_v3_error_t qrng_v3_init_from_seed(
    qrng_v3_ctx_t **ctx,
    const uint8_t *seed,
    size_t seed_len
);

#endif /* QUANTUM_RNG_V3_H */
#ifndef VALIDATION_H
#define VALIDATION_H

#include <stddef.h>
#include <stdint.h>

/**
 * @file validation.h
 * @brief Input validation macros and utilities
 * 
 * Provides comprehensive, consistent input validation across all modules.
 * Helps prevent security vulnerabilities, crashes, and undefined behavior
 * caused by invalid inputs.
 * 
 * Key principles:
 * - Validate all inputs at API boundaries
 * - Provide clear error messages
 * - Fail safely with proper error codes
 * - Zero performance overhead in release builds (optional)
 */

// ============================================================================
// VALIDATION MACROS
// ============================================================================

/**
 * @brief Validate pointer is not NULL
 */
#define VALIDATE_NOT_NULL(ptr, error_code) \
    do { \
        if (!(ptr)) { \
            return (error_code); \
        } \
    } while (0)

/**
 * @brief Validate pointer with custom error message (for debugging builds)
 */
#ifdef ENABLE_VALIDATION_MESSAGES
    #define VALIDATE_NOT_NULL_MSG(ptr, error_code, msg) \
        do { \
            if (!(ptr)) { \
                fprintf(stderr, "Validation error: %s (NULL pointer)\n", msg); \
                return (error_code); \
            } \
        } while (0)
#else
    #define VALIDATE_NOT_NULL_MSG(ptr, error_code, msg) \
        VALIDATE_NOT_NULL(ptr, error_code)
#endif

/**
 * @brief Validate value is within range [min, max]
 */
#define VALIDATE_RANGE(val, min, max, error_code) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            return (error_code); \
        } \
    } while (0)

/**
 * @brief Validate unsigned value is within range [min, max]
 */
#define VALIDATE_RANGE_U(val, min, max, error_code) \
    do { \
        if ((uint64_t)(val) < (uint64_t)(min) || (uint64_t)(val) > (uint64_t)(max)) { \
            return (error_code); \
        } \
    } while (0)

/**
 * @brief Validate condition is true
 */
#define VALIDATE_CONDITION(condition, error_code) \
    do { \
        if (!(condition)) { \
            return (error_code); \
        } \
    } while (0)

/**
 * @brief Validate buffer and size are valid
 */
#define VALIDATE_BUFFER(buf, size, error_code) \
    do { \
        if (!(buf) || (size) == 0) { \
            return (error_code); \
        } \
    } while (0)

// ============================================================================
// QUANTUM-SPECIFIC VALIDATION
// ============================================================================

/**
 * @brief Validate qubit index is valid for quantum state
 */
#define VALIDATE_QUBIT(state, qubit, error_code) \
    do { \
        if (!state || (qubit) < 0 || (qubit) >= (int)((state)->num_qubits)) { \
            return (error_code); \
        } \
    } while (0)

/**
 * @brief Validate multiple qubits are valid and unique
 */
#define VALIDATE_QUBITS_UNIQUE(state, qubits, num_qubits, error_code) \
    do { \
        if (!(state) || !(qubits) || (num_qubits) == 0) { \
            return (error_code); \
        } \
        for (size_t _i = 0; _i < (num_qubits); _i++) { \
            if ((qubits)[_i] < 0 || (qubits)[_i] >= (int)((state)->num_qubits)) { \
                return (error_code); \
            } \
            for (size_t _j = _i + 1; _j < (num_qubits); _j++) { \
                if ((qubits)[_i] == (qubits)[_j]) { \
                    return (error_code); \
                } \
            } \
        } \
    } while (0)

/**
 * @brief Validate quantum state is initialized
 */
#define VALIDATE_QUANTUM_STATE(state, error_code) \
    do { \
        if (!(state) || !(state)->amplitudes || (state)->state_dim == 0) { \
            return (error_code); \
        } \
    } while (0)

/**
 * @brief Validate basis state index is within state dimension
 */
#define VALIDATE_BASIS_INDEX(state, index, error_code) \
    do { \
        if (!(state) || (index) >= (state)->state_dim) { \
            return (error_code); \
        } \
    } while (0)

// ============================================================================
// ENTROPY VALIDATION
// ============================================================================

/**
 * @brief Validate entropy context is initialized
 */
#define VALIDATE_ENTROPY_CTX(ctx, error_code) \
    do { \
        if (!(ctx) || !(ctx)->get_bytes) { \
            return (error_code); \
        } \
    } while (0)

/**
 * @brief Validate entropy quality estimate
 */
#define VALIDATE_ENTROPY_QUALITY(min_entropy, error_code) \
    do { \
        if ((min_entropy) <= 0.0 || (min_entropy) > 8.0) { \
            return (error_code); \
        } \
    } while (0)

// ============================================================================
// CONFIGURATION VALIDATION
// ============================================================================

/**
 * @brief Validate health test configuration
 */
static inline int validate_health_config(
    uint32_t rct_cutoff,
    uint32_t apt_cutoff,
    uint32_t apt_window_size,
    double min_entropy
) {
    if (rct_cutoff < 2 || rct_cutoff > 10000) return 0;
    if (apt_cutoff < 10 || apt_cutoff > 100000) return 0;
    if (apt_window_size < 16 || apt_window_size > 65536) return 0;
    if (min_entropy <= 0.0 || min_entropy > 8.0) return 0;
    return 1;
}

/**
 * @brief Validate matrix dimensions
 */
static inline int validate_matrix_dims(size_t m, size_t n, size_t k) {
    if (m == 0 || n == 0 || k == 0) return 0;
    if (m > 65536 || n > 65536 || k > 65536) return 0;  // Reasonable limits
    return 1;
}

/**
 * @brief Validate number of qubits
 */
static inline int validate_num_qubits(size_t num_qubits, size_t max_qubits) {
    return (num_qubits > 0 && num_qubits <= max_qubits);
}

// ============================================================================
// UTILITY VALIDATION
// ============================================================================

/**
 * @brief Check if value is power of 2
 */
static inline int is_power_of_2(uint64_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

/**
 * @brief Validate array indices
 */
static inline int validate_array_indices(
    const int *indices,
    size_t num_indices,
    size_t max_index
) {
    if (!indices || num_indices == 0) return 0;
    
    for (size_t i = 0; i < num_indices; i++) {
        if (indices[i] < 0 || (size_t)indices[i] >= max_index) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Validate no duplicate indices in array
 */
static inline int validate_unique_indices(const int *indices, size_t num_indices) {
    if (!indices || num_indices == 0) return 0;
    
    for (size_t i = 0; i < num_indices; i++) {
        for (size_t j = i + 1; j < num_indices; j++) {
            if (indices[i] == indices[j]) {
                return 0;
            }
        }
    }
    return 1;
}

#endif /* VALIDATION_H */
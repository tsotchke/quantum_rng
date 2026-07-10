#ifndef SIMD_OPS_H
#define SIMD_OPS_H

#include <stdint.h>
#include <stddef.h>
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file simd_ops.h
 * @brief SIMD-optimized operations for quantum RNG
 *
 * Provides SSE2/AVX2 vectorized implementations of performance-critical
 * operations with automatic fallback to scalar implementations.
 *
 * Key optimizations:
 * - Complex number arithmetic (SSE2)
 * - Matrix operations (AVX2 if available)
 * - Normalization calculations
 * - Probability computations
 *
 * Runtime CPU feature detection ensures optimal code path selection.
 */

// Complex type (compatible with quantum_state.h)
typedef double _Complex complex_t;

// ============================================================================
// CPU FEATURE DETECTION
// ============================================================================

/**
 * @brief CPU SIMD capabilities
 */
typedef struct {
    int has_sse2;      /**< SSE2 support (2001+) */
    int has_sse3;      /**< SSE3 support (2004+) */
    int has_ssse3;     /**< SSSE3 support (2006+) */
    int has_sse4_1;    /**< SSE4.1 support (2007+) */
    int has_avx;       /**< AVX support (2011+) */
    int has_avx2;      /**< AVX2 support (2013+) */
    int has_fma;       /**< FMA3 support */
} simd_capabilities_t;

/**
 * @brief Detect CPU SIMD capabilities
 *
 * @return SIMD capabilities structure
 */
simd_capabilities_t simd_detect_capabilities(void);

/**
 * @brief Get CPU capabilities as string
 *
 * @param caps Capabilities structure
 * @return Human-readable string
 */
const char* simd_capabilities_string(const simd_capabilities_t *caps);

// ============================================================================
// VECTORIZED OPERATIONS
// ============================================================================

/**
 * @brief Compute sum of squared magnitudes (vectorized)
 *
 * Calculates Σ|αᵢ|² for normalization checks.
 * Uses SSE2/AVX2 when available.
 *
 * @param amplitudes Complex amplitude array
 * @param n Number of amplitudes
 * @return Sum of squared magnitudes
 */
double simd_sum_squared_magnitudes(const complex_t *amplitudes, size_t n);

/**
 * @brief Normalize complex amplitude array (vectorized)
 *
 * Divides all amplitudes by sqrt(Σ|αᵢ|²).
 * Uses SSE2/AVX2 when available.
 *
 * @param amplitudes Complex amplitude array (modified in-place)
 * @param n Number of amplitudes
 * @param norm Normalization factor (sqrt of sum squared magnitudes)
 */
void simd_normalize_amplitudes(complex_t *amplitudes, size_t n, double norm);

/**
 * @brief Matrix-vector multiply for 2x2 complex matrices (vectorized)
 *
 * Optimized for quantum gate operations.
 * Uses SSE2 for complex arithmetic.
 *
 * @param matrix 2x2 complex matrix (row-major)
 * @param input 2-element complex vector
 * @param output 2-element complex vector (output)
 */
void simd_matrix2x2_vec_multiply(
    const complex_t matrix[4],
    const complex_t input[2],
    complex_t output[2]
);

/**
 * @brief Complex multiplication (vectorized)
 *
 * Computes (a + bi) * (c + di) using SSE2.
 *
 * @param z1 First complex number
 * @param z2 Second complex number
 * @return Product z1 * z2
 */
complex_t simd_complex_multiply(complex_t z1, complex_t z2);

/**
 * @brief Batch compute probabilities from amplitudes (vectorized)
 *
 * Computes |α|² for array of amplitudes.
 * Uses SSE2/AVX2 when available.
 *
 * @param amplitudes Input amplitudes
 * @param probabilities Output probabilities
 * @param n Number of elements
 */
void simd_compute_probabilities(
    const complex_t *amplitudes,
    double *probabilities,
    size_t n
);

// ============================================================================
// QUANTUM GATE PRIMITIVES (SIMD-OPTIMIZED)
// ============================================================================

/**
 * @brief Vectorized complex swap for Pauli X gate
 *
 * Efficiently swaps pairs of complex amplitudes using SIMD.
 * Used by Pauli X and CNOT gates.
 *
 * @param amp0 First amplitude array
 * @param amp1 Second amplitude array
 * @param n Number of pairs to swap
 */
void simd_complex_swap(complex_t *amp0, complex_t *amp1, size_t n);

/**
 * @brief Vectorized multiply by i for Pauli Y components
 *
 * Efficiently multiplies complex numbers by ±i using SIMD.
 *
 * @param amplitudes Amplitude array to modify
 * @param n Number of amplitudes
 * @param negate If true, multiply by -i; otherwise by +i
 */
void simd_multiply_by_i(complex_t *amplitudes, size_t n, int negate);

/**
 * @brief Vectorized negate for Pauli Z gate
 *
 * Efficiently negates complex amplitudes using SIMD.
 *
 * @param amplitudes Amplitude array to negate
 * @param n Number of amplitudes
 */
void simd_negate(complex_t *amplitudes, size_t n);

/**
 * @brief Vectorized phase multiplication
 *
 * Efficiently multiplies amplitudes by a phase factor e^(iθ).
 * Used by S, T, and Phase gates.
 *
 * @param amplitudes Amplitude array to modify
 * @param phase Complex phase factor
 * @param n Number of amplitudes
 */
void simd_apply_phase(complex_t *amplitudes, complex_t phase, size_t n);

// ============================================================================
// ENTROPY MIXING OPERATIONS
// ============================================================================

/**
 * @brief XOR mixing of byte arrays (vectorized)
 *
 * Performs dest[i] ^= src[i] using SSE2/AVX2.
 *
 * @param dest Destination array (modified in-place)
 * @param src Source array
 * @param n Number of bytes
 */
void simd_xor_bytes(uint8_t *dest, const uint8_t *src, size_t n);

/**
 * @brief Fast entropy mixing using SIMD
 *
 * Mixes entropy buffers with cryptographic quality using
 * vectorized operations.
 *
 * @param state Current state buffer
 * @param entropy New entropy to mix
 * @param output Mixed output
 * @param size Buffer size
 */
void simd_mix_entropy(
    const uint8_t *state,
    const uint8_t *entropy,
    uint8_t *output,
    size_t size
);

/**
 * @brief SIMD-optimized cumulative probability search
 *
 * Finds the index where cumulative probability exceeds threshold.
 * Critical for fast quantum measurement sampling.
 *
 * @param amplitudes Complex amplitude array
 * @param n Number of amplitudes
 * @param random_threshold Random value in [0,1) for sampling
 * @return Index where cumulative probability exceeds threshold
 */
uint64_t simd_cumulative_probability_search(
    const complex_t *amplitudes,
    size_t n,
    double random_threshold
);

/**
 * @brief Batch compute probabilities and find cumulative threshold
 *
 * Optimized version that pre-computes all probabilities with SIMD,
 * then does fast cumulative sum.
 *
 * @param amplitudes Complex amplitude array
 * @param n Number of amplitudes
 * @param random_threshold Random value for sampling
 * @return Sampled index
 */
uint64_t simd_fast_measurement_sample(
    const complex_t *amplitudes,
    size_t n,
    double random_threshold
);

#ifdef __cplusplus
}
#endif

#endif /* SIMD_OPS_H */
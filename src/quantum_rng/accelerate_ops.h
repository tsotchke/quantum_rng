#ifndef ACCELERATE_OPS_H
#define ACCELERATE_OPS_H

#include <stdint.h>
#include <stddef.h>
#include <complex.h>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#define HAS_ACCELERATE 1
#else
#define HAS_ACCELERATE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file accelerate_ops.h
 * @brief Apple Accelerate framework integration for M2 Ultra quantum RNG
 * 
 * Phase 3 Optimization: AMX-Accelerated Matrix Operations
 * 
 * Leverages Apple's Accelerate framework to achieve 5-10x additional speedup
 * through AMX (Apple Matrix coprocessor) hardware acceleration.
 * 
 * Key Features:
 * - vDSP vectorized complex operations (AMX-accelerated)
 * - BLAS Level 2/3 operations for matrix math
 * - 64-byte memory alignment for optimal AMX performance
 * - Automatic fallback to SIMD when Accelerate unavailable
 * 
 * M2 Ultra AMX Capabilities:
 * - 2× 512-bit matrix units
 * - 8×8 complex matrix operations in ~4 cycles
 * - Integrated with vDSP for automatic engagement
 * - Expected 5-10x speedup for matrix-heavy quantum gates
 */

// Complex type compatible with quantum_state.h and Accelerate framework
typedef double _Complex complex_t;

// ============================================================================
// ACCELERATE CAPABILITY DETECTION
// ============================================================================

/**
 * @brief Check if Accelerate framework is available
 * @return 1 if available, 0 otherwise
 */
int accelerate_is_available(void);

/**
 * @brief Get hardware capabilities string
 * @return Human-readable description of acceleration capabilities
 */
const char* accelerate_get_capabilities(void);

// ============================================================================
// MEMORY MANAGEMENT (AMX-OPTIMIZED)
// ============================================================================

/**
 * @brief Allocate AMX-aligned memory (64-byte boundary)
 * 
 * AMX requires 64-byte alignment for optimal performance.
 * This ensures data structures are properly aligned.
 * 
 * @param size Number of bytes to allocate
 * @return Pointer to aligned memory, or NULL on failure
 */
void* accelerate_aligned_alloc(size_t size);

/**
 * @brief Free AMX-aligned memory
 * @param ptr Pointer to free (must be from accelerate_aligned_alloc)
 */
void accelerate_aligned_free(void* ptr);

/**
 * @brief Allocate complex amplitude array with AMX alignment
 * @param num_elements Number of complex elements
 * @return Pointer to aligned complex array
 */
complex_t* accelerate_alloc_complex_array(size_t num_elements);

/**
 * @brief Free complex amplitude array
 * @param ptr Pointer to complex array
 */
void accelerate_free_complex_array(complex_t* ptr);

// ============================================================================
// VECTORIZED COMPLEX OPERATIONS (vDSP)
// ============================================================================

/**
 * @brief Complex vector multiplication (element-wise)
 * 
 * Computes: result[i] = a[i] * b[i]
 * Uses vDSP_zvmul for AMX acceleration
 * 
 * @param a First complex vector
 * @param b Second complex vector  
 * @param result Output vector
 * @param n Number of elements
 */
void accelerate_complex_multiply(
    const complex_t* a,
    const complex_t* b,
    complex_t* result,
    size_t n
);

/**
 * @brief Complex vector addition
 * 
 * Computes: result[i] = a[i] + b[i]
 * Uses vDSP_zvadd for AMX acceleration
 * 
 * @param a First complex vector
 * @param b Second complex vector
 * @param result Output vector
 * @param n Number of elements
 */
void accelerate_complex_add(
    const complex_t* a,
    const complex_t* b,
    complex_t* result,
    size_t n
);

/**
 * @brief Complex vector subtraction
 * 
 * Computes: result[i] = a[i] - b[i]
 * Uses vDSP_zvsub for AMX acceleration
 * 
 * @param a First complex vector
 * @param b Second complex vector
 * @param result Output vector
 * @param n Number of elements
 */
void accelerate_complex_subtract(
    const complex_t* a,
    const complex_t* b,
    complex_t* result,
    size_t n
);

/**
 * @brief Complex vector magnitude (absolute value)
 * 
 * Computes: result[i] = |a[i]| = sqrt(real² + imag²)
 * Uses vDSP_zvabs for AMX acceleration
 * 
 * @param a Complex input vector
 * @param result Real output vector (magnitudes)
 * @param n Number of elements
 */
void accelerate_complex_magnitude(
    const complex_t* a,
    double* result,
    size_t n
);

/**
 * @brief Complex vector magnitude squared
 * 
 * Computes: result[i] = |a[i]|² = real² + imag²
 * Uses vDSP_zvmags for AMX acceleration (faster than zvabs + square)
 * 
 * @param a Complex input vector
 * @param result Real output vector (squared magnitudes)
 * @param n Number of elements
 */
void accelerate_complex_magnitude_squared(
    const complex_t* a,
    double* result,
    size_t n
);

/**
 * @brief Complex vector scale (multiply by scalar)
 * 
 * Computes: result[i] = a[i] * scalar
 * Uses vDSP_zvsmul for AMX acceleration
 * 
 * @param a Complex input vector
 * @param scalar Complex scalar value
 * @param result Complex output vector
 * @param n Number of elements
 */
void accelerate_complex_scale(
    const complex_t* a,
    complex_t scalar,
    complex_t* result,
    size_t n
);

// ============================================================================
// BLAS OPERATIONS (AMX-ACCELERATED)
// ============================================================================

/**
 * @brief Complex matrix-vector multiply (BLAS Level 2)
 * 
 * Computes: y = alpha * A * x + beta * y
 * Uses cblas_zgemv with AMX acceleration for M2 Ultra
 * 
 * This is CRITICAL for quantum gate operations - up to 10x faster
 * than scalar implementations due to AMX matrix units.
 * 
 * @param m Number of rows in matrix A
 * @param n Number of columns in matrix A
 * @param alpha Scalar multiplier for A*x
 * @param A Complex matrix (m×n, column-major for BLAS)
 * @param x Complex input vector (length n)
 * @param beta Scalar multiplier for y
 * @param y Complex output vector (length m, modified in-place)
 */
void accelerate_matrix_vector_multiply(
    size_t m,
    size_t n,
    complex_t alpha,
    const complex_t* A,
    const complex_t* x,
    complex_t beta,
    complex_t* y
);

/**
 * @brief Complex matrix-matrix multiply (BLAS Level 3)
 * 
 * Computes: C = alpha * A * B + beta * C
 * Uses cblas_zgemm with AMX acceleration
 * 
 * @param m Rows in A and C
 * @param n Columns in B and C
 * @param k Columns in A, rows in B
 * @param alpha Scalar multiplier for A*B
 * @param A Complex matrix (m×k, column-major)
 * @param B Complex matrix (k×n, column-major)
 * @param beta Scalar multiplier for C
 * @param C Complex output matrix (m×n, column-major, modified in-place)
 */
void accelerate_matrix_multiply(
    size_t m,
    size_t n,
    size_t k,
    complex_t alpha,
    const complex_t* A,
    const complex_t* B,
    complex_t beta,
    complex_t* C
);

// ============================================================================
// QUANTUM-SPECIFIC OPTIMIZATIONS
// ============================================================================

/**
 * @brief Sum of squared magnitudes (for normalization)
 * 
 * Computes: Σ|a[i]|² 
 * Uses vDSP_zvmags + vDSP_sve for AMX-accelerated reduction
 * 
 * Critical for quantum state normalization checks.
 * 
 * @param amplitudes Complex amplitude array
 * @param n Number of amplitudes
 * @return Sum of squared magnitudes
 */
double accelerate_sum_squared_magnitudes(
    const complex_t* amplitudes,
    size_t n
);

/**
 * @brief Normalize complex amplitude array
 * 
 * Divides all amplitudes by normalization factor.
 * Uses vDSP_zvsmul for vectorized scaling.
 * 
 * @param amplitudes Complex amplitude array (modified in-place)
 * @param n Number of amplitudes
 * @param norm_factor Normalization factor (typically sqrt(Σ|a|²))
 */
void accelerate_normalize_amplitudes(
    complex_t* amplitudes,
    size_t n,
    double norm_factor
);

/**
 * @brief Apply 2×2 quantum gate to amplitude pairs
 * 
 * For each pair (amp0, amp1), computes:
 *   new_amp0 = matrix[0][0] * amp0 + matrix[0][1] * amp1
 *   new_amp1 = matrix[1][0] * amp0 + matrix[1][1] * amp1
 * 
 * Uses vDSP operations with AMX acceleration.
 * Optimized for single-qubit quantum gates (H, X, Y, Z, RY, etc.)
 * 
 * @param matrix 2×2 complex gate matrix
 * @param amplitudes Amplitude array (modified in-place)
 * @param indices Array of amplitude pair indices
 * @param num_pairs Number of pairs to process
 */
void accelerate_apply_2x2_gate(
    const complex_t matrix[2][2],
    complex_t* amplitudes,
    const uint64_t* indices,
    size_t num_pairs
);

/**
 * @brief Cumulative probability search (for quantum measurement)
 * 
 * Finds index where cumulative probability exceeds threshold.
 * Uses vDSP_zvmags for fast magnitude computation.
 * 
 * @param amplitudes Complex amplitude array
 * @param n Number of amplitudes
 * @param threshold Random threshold value [0, 1)
 * @return Index where cumulative probability exceeds threshold
 */
uint64_t accelerate_cumulative_probability_search(
    const complex_t* amplitudes,
    size_t n,
    double threshold
);

// ============================================================================
// CONVERSION UTILITIES
// ============================================================================

/**
 * @brief Convert row-major matrix to column-major (for BLAS)
 * 
 * BLAS expects column-major matrices, but our code uses row-major.
 * This performs efficient transpose conversion.
 * 
 * @param row_major Input matrix (row-major)
 * @param col_major Output matrix (column-major)
 * @param rows Number of rows
 * @param cols Number of columns
 */
void accelerate_convert_to_column_major(
    const complex_t* row_major,
    complex_t* col_major,
    size_t rows,
    size_t cols
);

/**
 * @brief Convert column-major matrix to row-major (from BLAS)
 * 
 * @param col_major Input matrix (column-major)
 * @param row_major Output matrix (row-major)
 * @param rows Number of rows
 * @param cols Number of columns
 */
void accelerate_convert_to_row_major(
    const complex_t* col_major,
    complex_t* row_major,
    size_t rows,
    size_t cols
);

// ============================================================================
// PERFORMANCE UTILITIES
// ============================================================================

/**
 * @brief Benchmark Accelerate vs SIMD performance
 * 
 * Compares performance of Accelerate framework operations
 * against current SIMD implementations.
 * 
 * @param num_qubits Number of qubits to test
 * @return Speedup factor (>1.0 means Accelerate is faster)
 */
double accelerate_benchmark_vs_simd(size_t num_qubits);

/**
 * @brief Print AMX performance statistics
 * 
 * Displays information about AMX utilization and performance.
 */
void accelerate_print_performance_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* ACCELERATE_OPS_H */
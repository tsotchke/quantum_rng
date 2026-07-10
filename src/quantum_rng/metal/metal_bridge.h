#ifndef METAL_BRIDGE_H
#define METAL_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file metal_bridge.h
 * @brief C API for Metal GPU acceleration on M2 Ultra
 * 
 * Provides C interface to Metal compute pipeline for quantum operations.
 * Zero-copy unified memory architecture for maximum performance.
 * 
 * PERFORMANCE TARGET: 100-200x speedup over CPU
 * - 76 GPU cores on M2 Ultra (top configuration)
 * - 192GB unified memory with 800 GB/s bandwidth
 * - MTLResourceStorageModeShared for zero-copy
 */

// Complex type (compatible with C)
typedef double _Complex complex_t;

// Opaque handle to Metal compute context
typedef struct metal_compute_ctx metal_compute_ctx_t;

// Metal buffer handle
typedef struct metal_buffer metal_buffer_t;

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

/**
 * @brief Initialize Metal compute context
 * 
 * Creates Metal device, command queue, and compiles compute pipeline.
 * 
 * @return Metal compute context or NULL on failure
 */
metal_compute_ctx_t* metal_compute_init(void);

/**
 * @brief Free Metal compute context
 * 
 * @param ctx Metal compute context
 */
void metal_compute_free(metal_compute_ctx_t* ctx);

/**
 * @brief Check if Metal is available on this system
 * 
 * @return 1 if Metal is available, 0 otherwise
 */
int metal_is_available(void);

/**
 * @brief Get GPU device information
 * 
 * @param ctx Metal compute context
 * @param name Output buffer for device name (min 256 bytes)
 * @param max_threads Output: max threads per threadgroup
 * @param num_cores Output: number of GPU cores
 */
void metal_get_device_info(
    metal_compute_ctx_t* ctx,
    char* name,
    uint32_t* max_threads,
    uint32_t* num_cores
);

// ============================================================================
// MEMORY MANAGEMENT (ZERO-COPY UNIFIED MEMORY)
// ============================================================================

/**
 * @brief Allocate Metal buffer with zero-copy shared storage
 * 
 * Uses MTLResourceStorageModeShared for unified memory access.
 * CPU and GPU can access the same memory without copying.
 * 
 * @param ctx Metal compute context
 * @param size Buffer size in bytes
 * @return Metal buffer handle or NULL on failure
 */
metal_buffer_t* metal_buffer_create(metal_compute_ctx_t* ctx, size_t size);

/**
 * @brief Create Metal buffer from existing CPU memory
 * 
 * Wraps existing memory as shared Metal buffer (zero-copy).
 * 
 * @param ctx Metal compute context
 * @param data Existing CPU memory pointer
 * @param size Buffer size in bytes
 * @return Metal buffer handle or NULL on failure
 */
metal_buffer_t* metal_buffer_create_from_data(
    metal_compute_ctx_t* ctx,
    void* data,
    size_t size
);

/**
 * @brief Get CPU-accessible pointer to Metal buffer
 * 
 * @param buffer Metal buffer
 * @return Pointer to buffer data
 */
void* metal_buffer_contents(metal_buffer_t* buffer);

/**
 * @brief Free Metal buffer
 * 
 * @param buffer Metal buffer
 */
void metal_buffer_free(metal_buffer_t* buffer);

// ============================================================================
// QUANTUM GATE OPERATIONS (GPU-ACCELERATED)
// ============================================================================

/**
 * @brief GPU-accelerated Hadamard gate
 * 
 * PERFORMANCE: 20-40x faster than CPU
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer (MTLResourceStorageModeShared)
 * @param qubit_index Index of qubit to apply gate to
 * @param state_dim Number of amplitudes (2^num_qubits)
 * @return 0 on success, -1 on error
 */
int metal_hadamard(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
);

/**
 * @brief GPU-accelerated Hadamard on all qubits
 * 
 * Applies Hadamard to all qubits in single GPU dispatch.
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param num_qubits Number of qubits
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_hadamard_all(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t num_qubits,
    uint32_t state_dim
);

/**
 * @brief GPU-accelerated oracle (phase flip)
 * 
 * PERFORMANCE: 50-100x faster than CPU
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param target_state State to flip phase of
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_oracle(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t state_dim
);

/**
 * @brief GPU-accelerated oracle with multiple marked states
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param marked_states Array of marked states
 * @param num_marked Number of marked states
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_oracle_multi(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    const uint32_t* marked_states,
    uint32_t num_marked,
    uint32_t state_dim
);

/**
 * @brief GPU-accelerated Grover diffusion operator
 * 
 * PERFORMANCE: 15-30x faster than CPU
 * Fused implementation: Hadamard → Inversion → Hadamard
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param num_qubits Number of qubits
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_grover_diffusion(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t num_qubits,
    uint32_t state_dim
);

/**
 * @brief GPU-accelerated Pauli X gate
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param qubit_index Index of qubit
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_pauli_x(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
);

/**
 * @brief GPU-accelerated Pauli Z gate
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param qubit_index Index of qubit
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_pauli_z(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
);

// ============================================================================
// PROBABILITY & MEASUREMENT
// ============================================================================

/**
 * @brief Compute probabilities from amplitudes (GPU)
 * 
 * PERFORMANCE: 30-50x faster than CPU
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param probabilities Output probability buffer
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_compute_probabilities(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    metal_buffer_t* probabilities,
    uint32_t state_dim
);

/**
 * @brief Normalize quantum state (GPU)
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param norm Normalization factor
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_normalize(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    float norm,
    uint32_t state_dim
);
/**
 * @brief BREAKTHROUGH: Execute MULTIPLE complete Grover searches in parallel!
 * 
 * THIS IS THE RIGHT WAY TO USE GPU!
 * - Processes N searches simultaneously (one per threadgroup)
 * - 76 searches optimal for M2 Ultra (76 GPU cores)
 * - Single kernel launch for ALL searches
 * - Amortizes overhead across batch
 * 
 * EXPECTED PERFORMANCE:
 * - 76 searches in ~150ms (vs 14,972ms on CPU)
 * - Speedup: 100x+ for batch workloads!
 * 
 * @param ctx Metal compute context
 * @param batch_states Buffer for all quantum states (num_searches × state_dim)
 * @param targets Array of target states (one per search)
 * @param results Output array of found states (one per search)
 * @param num_searches Number of parallel searches (≤76 optimal)
 * @param num_qubits Qubits per search
 * @param num_iterations Grover iterations per search
 * @return 0 on success, -1 on error
 */
int metal_grover_batch_search(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* batch_states,
    const uint32_t* targets,
    uint32_t* results,
    uint32_t num_searches,
    uint32_t num_qubits,
    uint32_t num_iterations
);


// ============================================================================
// BATCH OPERATIONS
// ============================================================================

/**
 * @brief Execute complete Grover iteration on GPU
 * 
 * Fuses: Oracle → Diffusion into single GPU dispatch
 * Minimizes CPU↔GPU synchronization
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param target_state Marked state
 * @param num_qubits Number of qubits
 * @param state_dim Number of amplitudes
 * @return 0 on success, -1 on error
 */
int metal_grover_iteration(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t num_qubits,
    uint32_t state_dim
);

/**
 * @brief Execute multiple Grover iterations on GPU
 * 
 * Keeps all computation on GPU with minimal CPU interaction.
 * 
 * @param ctx Metal compute context
 * @param amplitudes Amplitude buffer
 * @param target_state Marked state
 * @param num_qubits Number of qubits
 * @param state_dim Number of amplitudes
 * @param num_iterations Number of Grover iterations
 * @return 0 on success, -1 on error
 */
int metal_grover_search(
    metal_compute_ctx_t* ctx,
    metal_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t num_qubits,
    uint32_t state_dim,
    uint32_t num_iterations
);

// ============================================================================
// SYNCHRONIZATION & UTILITIES
// ============================================================================

/**
 * @brief Wait for GPU operations to complete
 * 
 * @param ctx Metal compute context
 */
void metal_wait_completion(metal_compute_ctx_t* ctx);

/**
 * @brief Get GPU execution time for last operation
 * 
 * @param ctx Metal compute context
 * @return Execution time in seconds
 */
double metal_get_last_execution_time(metal_compute_ctx_t* ctx);

/**
 * @brief Enable/disable performance monitoring
 * 
 * @param ctx Metal compute context
 * @param enable 1 to enable, 0 to disable
 */
void metal_set_performance_monitoring(metal_compute_ctx_t* ctx, int enable);

// ============================================================================
// DIAGNOSTICS
// ============================================================================

/**
 * @brief Print Metal device capabilities
 * 
 * @param ctx Metal compute context
 */
void metal_print_device_info(metal_compute_ctx_t* ctx);

/**
 * @brief Get error message for last error
 * 
 * @param ctx Metal compute context
 * @return Error message string
 */
const char* metal_get_error(metal_compute_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* METAL_BRIDGE_H */
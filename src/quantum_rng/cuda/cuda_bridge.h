#ifndef CUDA_BRIDGE_H
#define CUDA_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file cuda_bridge.h
 * @brief C API for NVIDIA CUDA GPU acceleration of quantum operations
 *
 * Provides a C interface to a CUDA compute pipeline for quantum operations,
 * mirroring the Apple Metal backend in src/quantum_rng/metal/. The data layout
 * (interleaved single-precision complex amplitudes) is identical to Metal's so
 * that the two backends are drop-in interchangeable.
 *
 * Memory model: buffers are allocated with cudaMallocManaged (CUDA unified
 * memory), so cuda_buffer_contents() returns a host-usable pointer, mirroring
 * Metal's zero-copy shared storage. Call cuda_wait_completion() before reading
 * results back on the host.
 */

// Interleaved single-precision complex amplitude (8 bytes, matches Metal Complex)
typedef struct {
    float real;
    float imag;
} cuda_complex_t;

// Opaque handle to CUDA compute context
typedef struct cuda_compute_ctx cuda_compute_ctx_t;

// CUDA buffer handle (unified memory)
typedef struct cuda_buffer cuda_buffer_t;

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

/**
 * @brief Initialize CUDA compute context.
 * @return CUDA compute context or NULL on failure.
 */
cuda_compute_ctx_t* cuda_compute_init(void);

/**
 * @brief Free CUDA compute context.
 */
void cuda_compute_free(cuda_compute_ctx_t* ctx);

/**
 * @brief Check if a CUDA device is available on this system.
 * @return 1 if available, 0 otherwise.
 */
int cuda_is_available(void);

/**
 * @brief Get GPU device information.
 * @param ctx CUDA compute context
 * @param name Output buffer for device name (min 256 bytes)
 * @param max_threads Output: max threads per block
 * @param num_cores Output: number of streaming multiprocessors (SMs)
 */
void cuda_get_device_info(
    cuda_compute_ctx_t* ctx,
    char* name,
    uint32_t* max_threads,
    uint32_t* num_cores
);

// ============================================================================
// MEMORY MANAGEMENT (UNIFIED MEMORY)
// ============================================================================

/**
 * @brief Allocate a CUDA unified-memory buffer.
 */
cuda_buffer_t* cuda_buffer_create(cuda_compute_ctx_t* ctx, size_t size);

/**
 * @brief Allocate a CUDA unified-memory buffer and copy existing host data in.
 */
cuda_buffer_t* cuda_buffer_create_from_data(
    cuda_compute_ctx_t* ctx,
    void* data,
    size_t size
);

/**
 * @brief Get a host-usable pointer to a CUDA buffer.
 */
void* cuda_buffer_contents(cuda_buffer_t* buffer);

/**
 * @brief Free a CUDA buffer.
 */
void cuda_buffer_free(cuda_buffer_t* buffer);

// ============================================================================
// QUANTUM GATE OPERATIONS (GPU-ACCELERATED)
// ============================================================================

/**
 * @brief GPU Hadamard gate on a single qubit.
 * @param qubit_index Index of qubit to apply gate to
 * @param state_dim Number of amplitudes (2^num_qubits)
 * @return 0 on success, -1 on error
 */
int cuda_hadamard(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
);

/**
 * @brief GPU Hadamard applied to all qubits (uniform superposition from |0..0>).
 */
int cuda_hadamard_all(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t num_qubits,
    uint32_t state_dim
);

/**
 * @brief GPU oracle (phase flip of a single marked state).
 */
int cuda_oracle(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t state_dim
);

/**
 * @brief GPU oracle with multiple marked states.
 */
int cuda_oracle_multi(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    const uint32_t* marked_states,
    uint32_t num_marked,
    uint32_t state_dim
);

/**
 * @brief GPU Grover diffusion (inversion about the mean amplitude).
 * mean = sum(amp)/N ; amp = 2*mean - amp.
 */
int cuda_grover_diffusion(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t num_qubits,
    uint32_t state_dim
);

/**
 * @brief GPU Pauli X (bit flip on qubit).
 */
int cuda_pauli_x(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
);

/**
 * @brief GPU Pauli Z (phase flip where qubit bit is set).
 */
int cuda_pauli_z(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t qubit_index,
    uint32_t state_dim
);

// ============================================================================
// PROBABILITY & MEASUREMENT
// ============================================================================

/**
 * @brief Compute probabilities from amplitudes: prob[i] = real^2 + imag^2.
 * @param probabilities Output buffer of state_dim floats
 */
int cuda_compute_probabilities(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    cuda_buffer_t* probabilities,
    uint32_t state_dim
);

/**
 * @brief Normalize a quantum state: divide each amplitude by norm.
 */
int cuda_normalize(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    float norm,
    uint32_t state_dim
);

// ============================================================================
// GROVER SEARCH (SINGLE + BATCH)
// ============================================================================

/**
 * @brief One Grover iteration on GPU: oracle(target) then diffusion.
 */
int cuda_grover_iteration(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t num_qubits,
    uint32_t state_dim
);

/**
 * @brief Complete Grover search on GPU.
 *
 * Initializes to a uniform superposition then runs num_iterations of
 * (oracle -> diffusion). The amplitude of target_state should dominate.
 */
int cuda_grover_search(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* amplitudes,
    uint32_t target_state,
    uint32_t num_qubits,
    uint32_t state_dim,
    uint32_t num_iterations
);

/**
 * @brief Execute MULTIPLE complete Grover searches in parallel.
 *
 * One CUDA block per independent search. Each block runs a full Grover search
 * on its own state (num_searches states of state_dim each) and writes the
 * argmax (most probable state index) to results[search].
 *
 * @param batch_states Buffer for all quantum states (num_searches * state_dim)
 * @param targets Array of target states (one per search)
 * @param results Output array of found states (one per search)
 * @param num_searches Number of parallel searches
 * @param num_qubits Qubits per search
 * @param num_iterations Grover iterations per search
 */
int cuda_grover_batch_search(
    cuda_compute_ctx_t* ctx,
    cuda_buffer_t* batch_states,
    const uint32_t* targets,
    uint32_t* results,
    uint32_t num_searches,
    uint32_t num_qubits,
    uint32_t num_iterations
);

// ============================================================================
// SYNCHRONIZATION & UTILITIES
// ============================================================================

/**
 * @brief Wait for all queued GPU operations to complete.
 */
void cuda_wait_completion(cuda_compute_ctx_t* ctx);

/**
 * @brief Get GPU execution time (seconds) of the last measured operation.
 */
double cuda_get_last_execution_time(cuda_compute_ctx_t* ctx);

/**
 * @brief Enable/disable performance monitoring (event-based timing).
 */
void cuda_set_performance_monitoring(cuda_compute_ctx_t* ctx, int enable);

// ============================================================================
// DIAGNOSTICS
// ============================================================================

/**
 * @brief Print CUDA device capabilities.
 */
void cuda_print_device_info(cuda_compute_ctx_t* ctx);

/**
 * @brief Get error message for the last error.
 */
const char* cuda_get_error(cuda_compute_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* CUDA_BRIDGE_H */

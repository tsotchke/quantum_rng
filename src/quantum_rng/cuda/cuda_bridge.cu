/**
 * @file cuda_bridge.cu
 * @brief NVIDIA CUDA implementation of the quantum compute backend.
 *
 * Mirrors the Apple Metal backend (src/quantum_rng/metal/). Amplitudes are
 * stored as interleaved single-precision complex (cuda_complex_t, 8 bytes),
 * identical to Metal's Complex layout. Buffers use CUDA unified memory
 * (cudaMallocManaged) so the host can read results after cuda_wait_completion.
 *
 * Build (RTX 3090 = sm_86):
 *   nvcc -O3 -arch=sm_86 -Isrc -c cuda_bridge.cu
 */

#include "cuda_bridge.h"

#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 1/sqrt(2)
#define SQRT2_INV 0.7071067811865476f

// ============================================================================
// CONTEXT & BUFFER STRUCTS
// ============================================================================

struct cuda_compute_ctx {
    int device_id;
    cudaDeviceProp props;
    cudaEvent_t ev_start;
    cudaEvent_t ev_stop;
    double last_execution_time; // seconds
    int perf_monitoring;
    char error[256];
};

struct cuda_buffer {
    void* ptr;      // unified-memory pointer (host + device usable)
    size_t size;    // bytes
};

// Default launch geometry
#define THREADS_PER_BLOCK 256

static inline int div_up(uint32_t a, uint32_t b) {
    return (int)((a + b - 1u) / b);
}

static void set_error(cuda_compute_ctx_t* ctx, const char* msg) {
    if (!ctx) return;
    snprintf(ctx->error, sizeof(ctx->error), "%s", msg);
}

static int check_cuda(cuda_compute_ctx_t* ctx, cudaError_t err, const char* where) {
    if (err != cudaSuccess) {
        if (ctx) {
            snprintf(ctx->error, sizeof(ctx->error), "%s: %s",
                     where, cudaGetErrorString(err));
        }
        return -1;
    }
    return 0;
}

// ============================================================================
// TIMING HELPERS
// ============================================================================

static inline void timing_begin(cuda_compute_ctx_t* ctx) {
    if (ctx && ctx->perf_monitoring) {
        cudaEventRecord(ctx->ev_start, 0);
    }
}

static inline void timing_end(cuda_compute_ctx_t* ctx) {
    if (ctx && ctx->perf_monitoring) {
        cudaEventRecord(ctx->ev_stop, 0);
    }
}

// ============================================================================
// KERNELS
// ============================================================================

// Hadamard on a single qubit. Launched with state_dim/2 threads; each thread
// owns one amplitude pair (idx0, idx1) with bit `qubit_index` cleared/set.
__global__ void k_hadamard(cuda_complex_t* amp, uint32_t qubit_index, uint32_t state_dim) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t half = state_dim >> 1;
    if (gid >= half) return;

    uint32_t stride = 1u << qubit_index;
    uint32_t block = gid / stride;
    uint32_t pos = gid % stride;
    uint32_t idx0 = block * (stride << 1) + pos;
    uint32_t idx1 = idx0 + stride;

    cuda_complex_t a0 = amp[idx0];
    cuda_complex_t a1 = amp[idx1];
    amp[idx0].real = (a0.real + a1.real) * SQRT2_INV;
    amp[idx0].imag = (a0.imag + a1.imag) * SQRT2_INV;
    amp[idx1].real = (a0.real - a1.real) * SQRT2_INV;
    amp[idx1].imag = (a0.imag - a1.imag) * SQRT2_INV;
}

// Oracle: phase-flip a single target state.
__global__ void k_oracle_single(cuda_complex_t* amp, uint32_t target) {
    // Launched with a single thread; trivial but keeps everything on GPU.
    amp[target].real = -amp[target].real;
    amp[target].imag = -amp[target].imag;
}

// Oracle: phase-flip any of a set of marked states.
__global__ void k_oracle_multi(cuda_complex_t* amp, const uint32_t* marked,
                               uint32_t num_marked, uint32_t state_dim) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= state_dim) return;
    for (uint32_t i = 0; i < num_marked; i++) {
        if (gid == marked[i]) {
            amp[gid].real = -amp[gid].real;
            amp[gid].imag = -amp[gid].imag;
            return;
        }
    }
}

// Reduction pass: accumulate real/imag sums into a managed float[2] via atomics.
__global__ void k_sum_reduce(const cuda_complex_t* amp, uint32_t state_dim,
                             float* sum_out /* [0]=real [1]=imag */) {
    __shared__ float s_real[THREADS_PER_BLOCK];
    __shared__ float s_imag[THREADS_PER_BLOCK];

    uint32_t tid = threadIdx.x;
    uint32_t gid = blockIdx.x * blockDim.x + tid;
    uint32_t grid_stride = blockDim.x * gridDim.x;

    float sr = 0.0f, si = 0.0f;
    for (uint32_t i = gid; i < state_dim; i += grid_stride) {
        sr += amp[i].real;
        si += amp[i].imag;
    }
    s_real[tid] = sr;
    s_imag[tid] = si;
    __syncthreads();

    for (uint32_t s = blockDim.x >> 1; s > 0; s >>= 1) {
        if (tid < s) {
            s_real[tid] += s_real[tid + s];
            s_imag[tid] += s_imag[tid + s];
        }
        __syncthreads();
    }
    if (tid == 0) {
        atomicAdd(&sum_out[0], s_real[0]);
        atomicAdd(&sum_out[1], s_imag[0]);
    }
}

// Inversion about the mean: amp = 2*mean - amp.
__global__ void k_inversion(cuda_complex_t* amp, uint32_t state_dim,
                           const float* sum_in /* real,imag */) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= state_dim) return;
    float mean_r = sum_in[0] / (float)state_dim;
    float mean_i = sum_in[1] / (float)state_dim;
    amp[gid].real = 2.0f * mean_r - amp[gid].real;
    amp[gid].imag = 2.0f * mean_i - amp[gid].imag;
}

// Pauli X: swap amplitude pairs across qubit bit.
__global__ void k_pauli_x(cuda_complex_t* amp, uint32_t qubit_index, uint32_t state_dim) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t half = state_dim >> 1;
    if (gid >= half) return;

    uint32_t stride = 1u << qubit_index;
    uint32_t block = gid / stride;
    uint32_t pos = gid % stride;
    uint32_t idx0 = block * (stride << 1) + pos;
    uint32_t idx1 = idx0 + stride;

    cuda_complex_t tmp = amp[idx0];
    amp[idx0] = amp[idx1];
    amp[idx1] = tmp;
}

// Pauli Z: negate amplitudes where qubit bit is set.
__global__ void k_pauli_z(cuda_complex_t* amp, uint32_t qubit_index, uint32_t state_dim) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= state_dim) return;
    if (gid & (1u << qubit_index)) {
        amp[gid].real = -amp[gid].real;
        amp[gid].imag = -amp[gid].imag;
    }
}

// Probabilities: prob[i] = real^2 + imag^2.
__global__ void k_probabilities(const cuda_complex_t* amp, float* prob, uint32_t state_dim) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= state_dim) return;
    float re = amp[gid].real;
    float im = amp[gid].imag;
    prob[gid] = re * re + im * im;
}

// Normalize: divide each amplitude by norm.
__global__ void k_normalize(cuda_complex_t* amp, float norm, uint32_t state_dim) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= state_dim) return;
    float inv = 1.0f / norm;
    amp[gid].real *= inv;
    amp[gid].imag *= inv;
}

// Set state to computational basis |0...0>.
__global__ void k_init_zero(cuda_complex_t* amp, uint32_t state_dim) {
    uint32_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= state_dim) return;
    amp[gid].real = (gid == 0) ? 1.0f : 0.0f;
    amp[gid].imag = 0.0f;
}

// ----------------------------------------------------------------------------
// BATCH GROVER: one block per independent search.
// Each block owns one state of `state_dim` amplitudes and runs a full search:
//   uniform superposition (Hadamard on all qubits) -> N * (oracle, diffusion)
//   -> argmax of |amp|^2 written to results[block].
// Diffusion is inversion about the mean (matches single-search path).
// ----------------------------------------------------------------------------
__global__ void k_grover_batch(cuda_complex_t* batch_states,
                              const uint32_t* targets,
                              uint32_t* results,
                              uint32_t num_searches,
                              uint32_t num_qubits,
                              uint32_t num_iterations) {
    uint32_t search = blockIdx.x;
    if (search >= num_searches) return;

    const uint32_t state_dim = 1u << num_qubits;
    cuda_complex_t* amp = &batch_states[(size_t)search * state_dim];
    const uint32_t target = targets[search];

    uint32_t tid = threadIdx.x;
    uint32_t nthreads = blockDim.x;

    __shared__ float s_real[THREADS_PER_BLOCK];
    __shared__ float s_imag[THREADS_PER_BLOCK];

    // ---- Initialize to |0...0> ----
    for (uint32_t i = tid; i < state_dim; i += nthreads) {
        amp[i].real = (i == 0) ? 1.0f : 0.0f;
        amp[i].imag = 0.0f;
    }
    __syncthreads();

    // ---- Hadamard on all qubits (uniform superposition) ----
    for (uint32_t q = 0; q < num_qubits; q++) {
        uint32_t stride = 1u << q;
        for (uint32_t idx = tid; idx < state_dim; idx += nthreads) {
            if ((idx & stride) == 0) {
                uint32_t j = idx | stride;
                cuda_complex_t a0 = amp[idx];
                cuda_complex_t a1 = amp[j];
                amp[idx].real = (a0.real + a1.real) * SQRT2_INV;
                amp[idx].imag = (a0.imag + a1.imag) * SQRT2_INV;
                amp[j].real = (a0.real - a1.real) * SQRT2_INV;
                amp[j].imag = (a0.imag - a1.imag) * SQRT2_INV;
            }
        }
        __syncthreads();
    }

    // ---- Grover iterations ----
    for (uint32_t iter = 0; iter < num_iterations; iter++) {
        // Oracle: phase flip target
        if (tid == 0) {
            amp[target].real = -amp[target].real;
            amp[target].imag = -amp[target].imag;
        }
        __syncthreads();

        // Diffusion: inversion about the mean
        float sr = 0.0f, si = 0.0f;
        for (uint32_t i = tid; i < state_dim; i += nthreads) {
            sr += amp[i].real;
            si += amp[i].imag;
        }
        s_real[tid] = sr;
        s_imag[tid] = si;
        __syncthreads();
        for (uint32_t s = nthreads >> 1; s > 0; s >>= 1) {
            if (tid < s) {
                s_real[tid] += s_real[tid + s];
                s_imag[tid] += s_imag[tid + s];
            }
            __syncthreads();
        }
        float mean_r = s_real[0] / (float)state_dim;
        float mean_i = s_imag[0] / (float)state_dim;
        __syncthreads();

        for (uint32_t i = tid; i < state_dim; i += nthreads) {
            amp[i].real = 2.0f * mean_r - amp[i].real;
            amp[i].imag = 2.0f * mean_i - amp[i].imag;
        }
        __syncthreads();
    }

    // ---- Measurement: argmax of |amp|^2 ----
    float max_prob = -1.0f;
    uint32_t max_idx = 0;
    for (uint32_t i = tid; i < state_dim; i += nthreads) {
        float p = amp[i].real * amp[i].real + amp[i].imag * amp[i].imag;
        if (p > max_prob) {
            max_prob = p;
            max_idx = i;
        }
    }
    s_real[tid] = max_prob;
    s_imag[tid] = (float)max_idx;
    __syncthreads();
    for (uint32_t s = nthreads >> 1; s > 0; s >>= 1) {
        if (tid < s) {
            if (s_real[tid + s] > s_real[tid]) {
                s_real[tid] = s_real[tid + s];
                s_imag[tid] = s_imag[tid + s];
            }
        }
        __syncthreads();
    }
    if (tid == 0) {
        results[search] = (uint32_t)s_imag[0];
    }
}

// ============================================================================
// CONTEXT LIFECYCLE
// ============================================================================

extern "C" cuda_compute_ctx_t* cuda_compute_init(void) {
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess || count <= 0) {
        return NULL;
    }
    cuda_compute_ctx_t* ctx = (cuda_compute_ctx_t*)calloc(1, sizeof(cuda_compute_ctx_t));
    if (!ctx) return NULL;

    ctx->device_id = 0;
    if (check_cuda(ctx, cudaSetDevice(ctx->device_id), "cudaSetDevice") != 0) {
        free(ctx);
        return NULL;
    }
    if (check_cuda(ctx, cudaGetDeviceProperties(&ctx->props, ctx->device_id),
                   "cudaGetDeviceProperties") != 0) {
        free(ctx);
        return NULL;
    }
    cudaEventCreate(&ctx->ev_start);
    cudaEventCreate(&ctx->ev_stop);
    ctx->last_execution_time = 0.0;
    ctx->perf_monitoring = 1;
    set_error(ctx, "no error");
    return ctx;
}

extern "C" void cuda_compute_free(cuda_compute_ctx_t* ctx) {
    if (!ctx) return;
    cudaEventDestroy(ctx->ev_start);
    cudaEventDestroy(ctx->ev_stop);
    free(ctx);
}

extern "C" int cuda_is_available(void) {
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess) return 0;
    return count > 0 ? 1 : 0;
}

extern "C" void cuda_get_device_info(cuda_compute_ctx_t* ctx, char* name,
                                     uint32_t* max_threads, uint32_t* num_cores) {
    if (!ctx) return;
    if (name) snprintf(name, 256, "%s", ctx->props.name);
    if (max_threads) *max_threads = (uint32_t)ctx->props.maxThreadsPerBlock;
    if (num_cores) *num_cores = (uint32_t)ctx->props.multiProcessorCount;
}

// ============================================================================
// MEMORY
// ============================================================================

extern "C" cuda_buffer_t* cuda_buffer_create(cuda_compute_ctx_t* ctx, size_t size) {
    cuda_buffer_t* buf = (cuda_buffer_t*)calloc(1, sizeof(cuda_buffer_t));
    if (!buf) return NULL;
    if (check_cuda(ctx, cudaMallocManaged(&buf->ptr, size), "cudaMallocManaged") != 0) {
        free(buf);
        return NULL;
    }
    buf->size = size;
    return buf;
}

extern "C" cuda_buffer_t* cuda_buffer_create_from_data(cuda_compute_ctx_t* ctx,
                                                       void* data, size_t size) {
    cuda_buffer_t* buf = cuda_buffer_create(ctx, size);
    if (!buf) return NULL;
    if (data) memcpy(buf->ptr, data, size);
    return buf;
}

extern "C" void* cuda_buffer_contents(cuda_buffer_t* buffer) {
    return buffer ? buffer->ptr : NULL;
}

extern "C" void cuda_buffer_free(cuda_buffer_t* buffer) {
    if (!buffer) return;
    if (buffer->ptr) cudaFree(buffer->ptr);
    free(buffer);
}

// ============================================================================
// GATE OPERATIONS
// ============================================================================

extern "C" int cuda_hadamard(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                             uint32_t qubit_index, uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;
    uint32_t half = state_dim >> 1;
    timing_begin(ctx);
    k_hadamard<<<div_up(half, THREADS_PER_BLOCK), THREADS_PER_BLOCK>>>(amp, qubit_index, state_dim);
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_hadamard");
}

extern "C" int cuda_hadamard_all(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                                 uint32_t num_qubits, uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;
    uint32_t half = state_dim >> 1;
    int blocks = div_up(half, THREADS_PER_BLOCK);
    timing_begin(ctx);
    // Kernel boundaries provide the global synchronization needed between qubits.
    for (uint32_t q = 0; q < num_qubits; q++) {
        k_hadamard<<<blocks, THREADS_PER_BLOCK>>>(amp, q, state_dim);
    }
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_hadamard_all");
}

extern "C" int cuda_oracle(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                           uint32_t target_state, uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    (void)state_dim;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;
    timing_begin(ctx);
    k_oracle_single<<<1, 1>>>(amp, target_state);
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_oracle");
}

extern "C" int cuda_oracle_multi(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                                 const uint32_t* marked_states, uint32_t num_marked,
                                 uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;

    // Copy marked list into unified memory the kernel can read.
    uint32_t* d_marked = NULL;
    if (check_cuda(ctx, cudaMallocManaged(&d_marked, num_marked * sizeof(uint32_t)),
                   "cuda_oracle_multi alloc") != 0) {
        return -1;
    }
    memcpy(d_marked, marked_states, num_marked * sizeof(uint32_t));

    timing_begin(ctx);
    k_oracle_multi<<<div_up(state_dim, THREADS_PER_BLOCK), THREADS_PER_BLOCK>>>(
        amp, d_marked, num_marked, state_dim);
    timing_end(ctx);
    int rc = check_cuda(ctx, cudaGetLastError(), "cuda_oracle_multi");
    cudaDeviceSynchronize();
    cudaFree(d_marked);
    return rc;
}

extern "C" int cuda_grover_diffusion(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                                     uint32_t num_qubits, uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    (void)num_qubits;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;

    float* sum = NULL;
    if (check_cuda(ctx, cudaMallocManaged(&sum, 2 * sizeof(float)),
                   "cuda_grover_diffusion alloc") != 0) {
        return -1;
    }
    sum[0] = 0.0f;
    sum[1] = 0.0f;

    int blocks = div_up(state_dim, THREADS_PER_BLOCK);
    int reduce_blocks = blocks < 256 ? blocks : 256; // cap grid for reduction

    timing_begin(ctx);
    k_sum_reduce<<<reduce_blocks, THREADS_PER_BLOCK>>>(amp, state_dim, sum);
    k_inversion<<<blocks, THREADS_PER_BLOCK>>>(amp, state_dim, sum);
    timing_end(ctx);
    int rc = check_cuda(ctx, cudaGetLastError(), "cuda_grover_diffusion");
    cudaDeviceSynchronize();
    cudaFree(sum);
    return rc;
}

extern "C" int cuda_pauli_x(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                            uint32_t qubit_index, uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;
    uint32_t half = state_dim >> 1;
    timing_begin(ctx);
    k_pauli_x<<<div_up(half, THREADS_PER_BLOCK), THREADS_PER_BLOCK>>>(amp, qubit_index, state_dim);
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_pauli_x");
}

extern "C" int cuda_pauli_z(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                            uint32_t qubit_index, uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;
    timing_begin(ctx);
    k_pauli_z<<<div_up(state_dim, THREADS_PER_BLOCK), THREADS_PER_BLOCK>>>(amp, qubit_index, state_dim);
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_pauli_z");
}

// ============================================================================
// PROBABILITY & MEASUREMENT
// ============================================================================

extern "C" int cuda_compute_probabilities(cuda_compute_ctx_t* ctx,
                                          cuda_buffer_t* amplitudes,
                                          cuda_buffer_t* probabilities,
                                          uint32_t state_dim) {
    if (!ctx || !amplitudes || !probabilities) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;
    float* prob = (float*)probabilities->ptr;
    timing_begin(ctx);
    k_probabilities<<<div_up(state_dim, THREADS_PER_BLOCK), THREADS_PER_BLOCK>>>(amp, prob, state_dim);
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_compute_probabilities");
}

extern "C" int cuda_normalize(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                              float norm, uint32_t state_dim) {
    if (!ctx || !amplitudes) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;
    timing_begin(ctx);
    k_normalize<<<div_up(state_dim, THREADS_PER_BLOCK), THREADS_PER_BLOCK>>>(amp, norm, state_dim);
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_normalize");
}

// ============================================================================
// GROVER (SINGLE)
// ============================================================================

extern "C" int cuda_grover_iteration(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                                     uint32_t target_state, uint32_t num_qubits,
                                     uint32_t state_dim) {
    if (cuda_oracle(ctx, amplitudes, target_state, state_dim) != 0) return -1;
    if (cuda_grover_diffusion(ctx, amplitudes, num_qubits, state_dim) != 0) return -1;
    return 0;
}

extern "C" int cuda_grover_search(cuda_compute_ctx_t* ctx, cuda_buffer_t* amplitudes,
                                  uint32_t target_state, uint32_t num_qubits,
                                  uint32_t state_dim, uint32_t num_iterations) {
    if (!ctx || !amplitudes) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)amplitudes->ptr;

    timing_begin(ctx);
    // Initialize to |0...0>, then build uniform superposition.
    k_init_zero<<<div_up(state_dim, THREADS_PER_BLOCK), THREADS_PER_BLOCK>>>(amp, state_dim);
    if (cuda_hadamard_all(ctx, amplitudes, num_qubits, state_dim) != 0) return -1;

    for (uint32_t iter = 0; iter < num_iterations; iter++) {
        if (cuda_grover_iteration(ctx, amplitudes, target_state, num_qubits, state_dim) != 0) {
            return -1;
        }
    }
    timing_end(ctx);
    return check_cuda(ctx, cudaGetLastError(), "cuda_grover_search");
}

extern "C" int cuda_grover_batch_search(cuda_compute_ctx_t* ctx,
                                        cuda_buffer_t* batch_states,
                                        const uint32_t* targets,
                                        uint32_t* results,
                                        uint32_t num_searches,
                                        uint32_t num_qubits,
                                        uint32_t num_iterations) {
    if (!ctx || !batch_states) return -1;
    cuda_complex_t* amp = (cuda_complex_t*)batch_states->ptr;

    uint32_t* d_targets = NULL;
    uint32_t* d_results = NULL;
    if (check_cuda(ctx, cudaMallocManaged(&d_targets, num_searches * sizeof(uint32_t)),
                   "batch targets alloc") != 0) return -1;
    if (check_cuda(ctx, cudaMallocManaged(&d_results, num_searches * sizeof(uint32_t)),
                   "batch results alloc") != 0) {
        cudaFree(d_targets);
        return -1;
    }
    memcpy(d_targets, targets, num_searches * sizeof(uint32_t));

    // One block per independent search.
    timing_begin(ctx);
    k_grover_batch<<<num_searches, THREADS_PER_BLOCK>>>(
        amp, d_targets, d_results, num_searches, num_qubits, num_iterations);
    timing_end(ctx);
    int rc = check_cuda(ctx, cudaGetLastError(), "cuda_grover_batch_search");
    cudaDeviceSynchronize();

    if (rc == 0 && results) {
        memcpy(results, d_results, num_searches * sizeof(uint32_t));
    }
    cudaFree(d_targets);
    cudaFree(d_results);
    return rc;
}

// ============================================================================
// SYNC & UTILITIES
// ============================================================================

extern "C" void cuda_wait_completion(cuda_compute_ctx_t* ctx) {
    if (!ctx) return;
    cudaDeviceSynchronize();
    if (ctx->perf_monitoring) {
        float ms = 0.0f;
        if (cudaEventElapsedTime(&ms, ctx->ev_start, ctx->ev_stop) == cudaSuccess) {
            ctx->last_execution_time = (double)ms / 1000.0;
        }
    }
}

extern "C" double cuda_get_last_execution_time(cuda_compute_ctx_t* ctx) {
    return ctx ? ctx->last_execution_time : 0.0;
}

extern "C" void cuda_set_performance_monitoring(cuda_compute_ctx_t* ctx, int enable) {
    if (ctx) ctx->perf_monitoring = enable ? 1 : 0;
}

extern "C" void cuda_print_device_info(cuda_compute_ctx_t* ctx) {
    if (!ctx) return;
    const cudaDeviceProp* p = &ctx->props;
    printf("CUDA Device: %s\n", p->name);
    printf("  Compute capability: %d.%d\n", p->major, p->minor);
    printf("  SMs (multiprocessors): %d\n", p->multiProcessorCount);
    printf("  Max threads/block: %d\n", p->maxThreadsPerBlock);
    printf("  Global memory: %.1f GB\n", (double)p->totalGlobalMem / (1024.0 * 1024.0 * 1024.0));
    printf("  Memory clock: %.0f MHz, bus width: %d-bit\n",
           p->memoryClockRate / 1000.0, p->memoryBusWidth);
    printf("  Managed memory (unified): %s\n", p->managedMemory ? "yes" : "no");
}

extern "C" const char* cuda_get_error(cuda_compute_ctx_t* ctx) {
    return ctx ? ctx->error : "no context";
}

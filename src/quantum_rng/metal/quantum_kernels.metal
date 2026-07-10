#include <metal_stdlib>
using namespace metal;

/**
 * @file quantum_kernels.metal
 * @brief Metal GPU kernels for quantum operations on M2 Ultra
 * 
 * PERFORMANCE TARGET: 100-200x speedup over CPU
 * - 76 GPU cores × 1024 threads = 77,824 parallel threads
 * - Threadgroup size: 256-1024 threads
 * - Zero-copy unified memory (MTLResourceStorageModeShared)
 */

// ============================================================================
// COMPLEX NUMBER OPERATIONS
// ============================================================================

struct Complex {
    float real;
    float imag;
    
    Complex(float r = 0.0f, float i = 0.0f) : real(r), imag(i) {}
    
    // Operators for thread address space (default)
    Complex operator+(thread const Complex& other) const {
        return Complex(real + other.real, imag + other.imag);
    }
    
    Complex operator-(thread const Complex& other) const {
        return Complex(real - other.real, imag - other.imag);
    }
    
    Complex operator*(thread const Complex& other) const {
        return Complex(
            real * other.real - imag * other.imag,
            real * other.imag + imag * other.real
        );
    }
    
    Complex operator*(float scalar) const {
        return Complex(real * scalar, imag * scalar);
    }
    
    float magnitude_squared() const {
        return real * real + imag * imag;
    }
};

// Helper functions for device memory operations
static Complex complex_add(Complex a, device Complex& b) {
    return Complex(a.real + b.real, a.imag + b.imag);
}

static Complex complex_sub(Complex a, device Complex& b) {
    return Complex(a.real - b.real, a.imag - b.imag);
}

static Complex complex_mul_scalar(device Complex& a, float s) {
    return Complex(a.real * s, a.imag * s);
}

// Constants
constant float SQRT2_INV = 0.7071067811865476f;  // 1/√2

// ============================================================================
// HADAMARD TRANSFORM KERNEL (HIGHEST PRIORITY - 40% OF RUNTIME)
// ============================================================================

/**
 * @brief GPU-accelerated Hadamard gate
 * 
 * H = 1/√2 * [1  1]
 *             [1 -1]
 * 
 * OPTIMIZATION STRATEGY:
 * - Stride-based access pattern for coalesced memory
 * - Each thread processes one amplitude pair
 * - Full GPU utilization: 76 threadgroups × 1024 threads
 * 
 * Expected speedup: 20-40x vs CPU (memory bandwidth limited)
 */
kernel void hadamard_transform(
    device Complex* amplitudes [[buffer(0)]],
    constant uint& qubit_index [[buffer(1)]],
    constant uint& state_dim [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    const uint stride = 1u << qubit_index;
    const uint block_size = stride << 1;
    
    // Calculate which block and position within block
    const uint block = gid / stride;
    const uint pos = gid % stride;
    const uint base = block * block_size;
    
    if (base + pos + stride >= state_dim) return;
    
    const uint idx0 = base + pos;
    const uint idx1 = idx0 + stride;
    
    // Load amplitudes
    Complex amp0 = amplitudes[idx0];
    Complex amp1 = amplitudes[idx1];
    
    // Hadamard transformation
    // |0⟩ → (|0⟩ + |1⟩)/√2
    // |1⟩ → (|0⟩ - |1⟩)/√2
    amplitudes[idx0] = (amp0 + amp1) * SQRT2_INV;
    amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
}

// ============================================================================
// MULTI-HADAMARD KERNEL (BATCH PROCESSING)
// ============================================================================

/**
 * @brief Apply Hadamard to all qubits simultaneously
 * 
 * OPTIMIZATION: Processes all Hadamard operations in single GPU dispatch
 * Eliminates CPU↔GPU round-trips between gate applications
 */
kernel void hadamard_all_qubits(
    device Complex* amplitudes [[buffer(0)]],
    constant uint& num_qubits [[buffer(1)]],
    constant uint& state_dim [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= state_dim) return;
    
    // Apply Hadamard to each qubit sequentially
    // (In-place transformation)
    for (uint qubit = 0; qubit < num_qubits; qubit++) {
        const uint stride = 1u << qubit;
        const uint block_size = stride << 1;
        const uint block = gid / block_size;
        const uint pos = gid % stride;
        
        if ((gid & stride) == 0) {  // Only process first half of each block
            const uint idx0 = gid;
            const uint idx1 = gid | stride;
            
            Complex amp0 = amplitudes[idx0];
            Complex amp1 = amplitudes[idx1];
            
            amplitudes[idx0] = (amp0 + amp1) * SQRT2_INV;
            amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
            
            threadgroup_barrier(mem_flags::mem_device);
        }
    }
}

// ============================================================================
// SPARSE ORACLE KERNEL (GROVER PHASE FLIP)
// ============================================================================

/**
 * @brief GPU-accelerated oracle for Grover's algorithm
 * 
 * Flips phase of target state: |target⟩ → -|target⟩
 * 
 * OPTIMIZATION:
 * - Embarrassingly parallel (no dependencies)
 * - Expected speedup: 50-100x vs CPU
 * - Each thread checks one amplitude independently
 */
kernel void sparse_oracle(
    device Complex* amplitudes [[buffer(0)]],
    constant uint* marked_states [[buffer(1)]],
    constant uint& num_marked [[buffer(2)]],
    constant uint& state_dim [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= state_dim) return;
    
    // Check if this state is marked
    for (uint i = 0; i < num_marked; i++) {
        if (gid == marked_states[i]) {
            // Phase flip: multiply by -1
            amplitudes[gid].real = -amplitudes[gid].real;
            amplitudes[gid].imag = -amplitudes[gid].imag;
            return;
        }
    }
}

// ============================================================================
// OPTIMIZED ORACLE (SINGLE TARGET)
// ============================================================================

/**
 * @brief Optimized oracle for single marked state
 * 
 * Most common case in Grover's algorithm
 */
kernel void oracle_single_target(
    device Complex* amplitudes [[buffer(0)]],
    constant uint& target_state [[buffer(1)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid == target_state) {
        amplitudes[gid].real = -amplitudes[gid].real;
        amplitudes[gid].imag = -amplitudes[gid].imag;
    }
}

// ============================================================================
// INVERSION ABOUT AVERAGE (GROVER DIFFUSION)
// ============================================================================

/**
 * @brief GPU-accelerated diffusion operator
 * 
 * D = 2|ψ⟩⟨ψ| - I = H^⊗n (2|0⟩⟨0| - I) H^⊗n
 * 
 * OPTIMIZATION STRATEGY:
 * - Two-pass algorithm:
 *   Pass 1: Compute average amplitude using parallel reduction
 *   Pass 2: Apply inversion about average
 * - Uses threadgroup shared memory for reduction
 * 
 * Expected speedup: 15-30x vs CPU
 */

// Pass 1: Compute average using parallel reduction
kernel void compute_average_amplitude(
    device const Complex* amplitudes [[buffer(0)]],
    device float* partial_sums_real [[buffer(1)]],
    device float* partial_sums_imag [[buffer(2)]],
    constant uint& state_dim [[buffer(3)]],
    uint gid [[thread_position_in_grid]],
    uint lid [[thread_position_in_threadgroup]],
    uint threadgroup_size [[threads_per_threadgroup]]
) {
    // Shared memory for reduction within threadgroup
    threadgroup float shared_real[1024];
    threadgroup float shared_imag[1024];
    
    // Load and accumulate
    float sum_real = 0.0f;
    float sum_imag = 0.0f;
    
    for (uint i = gid; i < state_dim; i += threadgroup_size * 76) {  // 76 threadgroups
        sum_real += amplitudes[i].real;
        sum_imag += amplitudes[i].imag;
    }
    
    shared_real[lid] = sum_real;
    shared_imag[lid] = sum_imag;
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    // Parallel reduction within threadgroup
    for (uint stride = threadgroup_size / 2; stride > 0; stride >>= 1) {
        if (lid < stride) {
            shared_real[lid] += shared_real[lid + stride];
            shared_imag[lid] += shared_imag[lid + stride];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    // Write threadgroup result
    if (lid == 0) {
        uint threadgroup_id = gid / threadgroup_size;
        partial_sums_real[threadgroup_id] = shared_real[0];
        partial_sums_imag[threadgroup_id] = shared_imag[0];
    }
}

// Pass 2: Apply inversion about average
kernel void inversion_about_average(
    device Complex* amplitudes [[buffer(0)]],
    constant float& avg_real [[buffer(1)]],
    constant float& avg_imag [[buffer(2)]],
    constant uint& state_dim [[buffer(3)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= state_dim) return;
    
    // Inversion: amp' = 2*avg - amp
    float two_avg_real = avg_real * 2.0f;
    float two_avg_imag = avg_imag * 2.0f;
    amplitudes[gid].real = two_avg_real - amplitudes[gid].real;
    amplitudes[gid].imag = two_avg_imag - amplitudes[gid].imag;
}

// ============================================================================
// FUSED GROVER DIFFUSION (SINGLE KERNEL)
// ============================================================================

/**
 * @brief Fused Hadamard + Inversion + Hadamard for diffusion
 * 
 * OPTIMIZATION: Combines three operations into one kernel
 * - Eliminates 2 CPU↔GPU round-trips
 * - Keeps intermediate results in GPU memory
 */
kernel void grover_diffusion_fused(
    device Complex* amplitudes [[buffer(0)]],
    constant uint& num_qubits [[buffer(1)]],
    constant uint& state_dim [[buffer(2)]],
    device float* scratch_avg [[buffer(3)]],  // Scratch space for average
    uint gid [[thread_position_in_grid]],
    uint lid [[thread_position_in_threadgroup]],
    uint threadgroup_size [[threads_per_threadgroup]]
) {
    // Step 1: Apply Hadamard to all qubits
    if (gid < state_dim) {
        for (uint qubit = 0; qubit < num_qubits; qubit++) {
            const uint stride = 1u << qubit;
            if ((gid & stride) == 0) {
                const uint idx1 = gid | stride;
                
                Complex amp0 = amplitudes[gid];
                Complex amp1 = amplitudes[idx1];
                
                amplitudes[gid] = (amp0 + amp1) * SQRT2_INV;
                amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
                
                threadgroup_barrier(mem_flags::mem_device);
            }
        }
    }
    
    threadgroup_barrier(mem_flags::mem_device);
    
    // Step 2: Compute average (local reduction)
    threadgroup float shared_real[1024];
    threadgroup float shared_imag[1024];
    
    float sum_real = 0.0f;
    float sum_imag = 0.0f;
    
    if (gid < state_dim) {
        sum_real = amplitudes[gid].real;
        sum_imag = amplitudes[gid].imag;
    }
    
    shared_real[lid] = sum_real;
    shared_imag[lid] = sum_imag;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    for (uint stride = threadgroup_size / 2; stride > 0; stride >>= 1) {
        if (lid < stride && lid + stride < threadgroup_size) {
            shared_real[lid] += shared_real[lid + stride];
            shared_imag[lid] += shared_imag[lid + stride];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    // Broadcast average to all threads
    float avg_real = shared_real[0] / float(state_dim);
    float avg_imag = shared_imag[0] / float(state_dim);
    
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    // Step 3: Inversion about average
    if (gid < state_dim) {
        float two_avg_real = avg_real * 2.0f;
        float two_avg_imag = avg_imag * 2.0f;
        amplitudes[gid].real = two_avg_real - amplitudes[gid].real;
        amplitudes[gid].imag = two_avg_imag - amplitudes[gid].imag;
    }
    
    threadgroup_barrier(mem_flags::mem_device);
    
    // Step 4: Apply Hadamard again
    if (gid < state_dim) {
        for (uint qubit = 0; qubit < num_qubits; qubit++) {
            const uint stride = 1u << qubit;
            if ((gid & stride) == 0) {
                const uint idx1 = gid | stride;
                
                Complex amp0 = amplitudes[gid];
                Complex amp1 = amplitudes[idx1];
                
                amplitudes[gid] = (amp0 + amp1) * SQRT2_INV;
                amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
                
                threadgroup_barrier(mem_flags::mem_device);
            }
        }
    }
}

// ============================================================================
// PROBABILITY CALCULATION KERNEL
// ============================================================================

/**
 * @brief Compute probabilities from amplitudes
 * 
 * P(i) = |α_i|² = real² + imag²
 * 
 * OPTIMIZATION: Embarrassingly parallel
 * Expected speedup: 30-50x vs CPU
 */
kernel void compute_probabilities(
    device const Complex* amplitudes [[buffer(0)]],
    device float* probabilities [[buffer(1)]],
    constant uint& state_dim [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= state_dim) return;
    
    float re = amplitudes[gid].real;
    float im = amplitudes[gid].imag;
    probabilities[gid] = re * re + im * im;
}

// ============================================================================
// CUMULATIVE PROBABILITY (FOR MEASUREMENT)
// ============================================================================

/**
 * @brief Parallel prefix sum for cumulative probabilities
 * 
 * Used for quantum measurement sampling
 * Implements Blelloch scan algorithm
 */
kernel void cumulative_sum_upsweep(
    device float* data [[buffer(0)]],
    constant uint& n [[buffer(1)]],
    constant uint& stride [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    uint i = (gid + 1) * stride * 2 - 1;
    if (i < n) {
        data[i] += data[i - stride];
    }
}

kernel void cumulative_sum_downsweep(
    device float* data [[buffer(0)]],
    constant uint& n [[buffer(1)]],
    constant uint& stride [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    uint i = (gid + 1) * stride * 2 - 1;
    if (i < n) {
        float temp = data[i - stride];
        data[i - stride] = data[i];
        data[i] += temp;
    }
}

// ============================================================================
// NORMALIZATION KERNEL
// ============================================================================

/**
 * @brief Normalize quantum state
 * 
 * Divides all amplitudes by sqrt(Σ|α_i|²)
 */
kernel void normalize_state(
    device Complex* amplitudes [[buffer(0)]],
    constant float& norm [[buffer(1)]],
    constant uint& state_dim [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= state_dim) return;
    
    float inv_norm = 1.0f / norm;
    amplitudes[gid].real *= inv_norm;
    amplitudes[gid].imag *= inv_norm;
}

// ============================================================================
// PAULI GATES (BONUS OPTIMIZATIONS)
// ============================================================================

// Pauli X (bit flip)
kernel void pauli_x(
    device Complex* amplitudes [[buffer(0)]],
    constant uint& qubit_index [[buffer(1)]],
    constant uint& state_dim [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    const uint stride = 1u << qubit_index;
    const uint block_size = stride << 1;
    const uint block = gid / stride;
    const uint pos = gid % stride;
    const uint base = block * block_size;
    
    if (base + pos + stride >= state_dim) return;
    
    const uint idx0 = base + pos;
    const uint idx1 = idx0 + stride;
    
    Complex temp = amplitudes[idx0];
    amplitudes[idx0] = amplitudes[idx1];
    amplitudes[idx1] = temp;
}

// Pauli Z (phase flip)
kernel void pauli_z(
    device Complex* amplitudes [[buffer(0)]],
    constant uint& qubit_index [[buffer(1)]],
    constant uint& state_dim [[buffer(2)]],
    uint gid [[thread_position_in_grid]]
) {
    if (gid >= state_dim) return;
    
    const uint stride = 1u << qubit_index;
    if (gid & stride) {  // If qubit is |1⟩
        amplitudes[gid].real = -amplitudes[gid].real;
        amplitudes[gid].imag = -amplitudes[gid].imag;
    }
}
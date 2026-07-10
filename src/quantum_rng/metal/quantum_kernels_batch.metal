#include <metal_stdlib>
using namespace metal;

/**
 * @file quantum_kernels_batch.metal
 * @brief BATCH GPU kernels - The RIGHT way to use GPU!
 * 
 * BREAKTHROUGH: Process MANY Grover searches in parallel!
 * - 76 threadgroups = 76 simultaneous Grover searches
 * - Each threadgroup (1024 threads) handles ONE complete search
 * - Amortizes kernel launch overhead across all searches
 * 
 * EXPECTED: 100-1000x speedup over CPU for batch workloads!
 */

// ============================================================================
// COMPLEX NUMBER (same as before)
// ============================================================================

struct Complex {
    float real;
    float imag;
    
    Complex(float r = 0.0f, float i = 0.0f) : real(r), imag(i) {}
    
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

constant float SQRT2_INV = 0.7071067811865476f;

// ============================================================================
// BATCH GROVER SEARCH KERNEL (THE GAME CHANGER!)
// ============================================================================

/**
 * @brief Execute COMPLETE Grover search for multiple states in parallel
 * 
 * ARCHITECTURE:
 * - Each threadgroup = ONE complete Grover search
 * - 76 threadgroups = 76 parallel searches
 * - Each search: Initialize → N iterations → Result
 * - ALL in ONE kernel launch!
 * 
 * EXPECTED PERFORMANCE:
 * - 76 searches in ~150ms (vs 197ms × 76 = 14,972ms on CPU)
 * - Speedup: ~100x!
 * 
 * @param batch_states All quantum states (concatenated)
 * @param targets Target state for each search
 * @param results Output: found states
 * @param num_searches Number of parallel searches (≤ 76 optimal)
 * @param num_qubits Qubits per search
 * @param num_iterations Grover iterations
 */
kernel void grover_batch_search(
    device Complex* batch_states [[buffer(0)]],
    constant uint* targets [[buffer(1)]],
    device uint* results [[buffer(2)]],
    constant uint& num_searches [[buffer(3)]],
    constant uint& num_qubits [[buffer(4)]],
    constant uint& num_iterations [[buffer(5)]],
    uint threadgroup_id [[threadgroup_position_in_grid]],
    uint thread_id [[thread_position_in_threadgroup]],
    uint threadgroup_size [[threads_per_threadgroup]]
) {
    // Bounds check
    if (threadgroup_id >= num_searches) return;
    
    const uint state_dim = 1u << num_qubits;
    const uint search_offset = threadgroup_id * state_dim;
    
    // This threadgroup's quantum state
    device Complex* amplitudes = &batch_states[search_offset];
    const uint target = targets[threadgroup_id];
    
    // Shared memory for reductions
    threadgroup float shared_real[1024];
    threadgroup float shared_imag[1024];
    
    // ========================================
    // INITIALIZATION: Apply Hadamard to all qubits
    // ========================================
    
    for (uint qubit = 0; qubit < num_qubits; qubit++) {
        const uint stride = 1u << qubit;
        
        // Each thread processes amplitude pairs
        for (uint i = thread_id; i < stride; i += threadgroup_size) {
            const uint idx0 = i;
            const uint idx1 = i + stride;
            
            Complex amp0 = amplitudes[idx0];
            Complex amp1 = amplitudes[idx1];
            
            amplitudes[idx0] = (amp0 + amp1) * SQRT2_INV;
            amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
        }
        
        threadgroup_barrier(mem_flags::mem_device);
    }
    
    // ========================================
    // GROVER ITERATIONS
    // ========================================
    
    for (uint iter = 0; iter < num_iterations; iter++) {
        // ORACLE: Flip phase of target state
        if (thread_id == 0) {
            amplitudes[target].real = -amplitudes[target].real;
            amplitudes[target].imag = -amplitudes[target].imag;
        }
        threadgroup_barrier(mem_flags::mem_device);
        
        // DIFFUSION OPERATOR
        
        // Step 1: Apply Hadamard to all qubits
        for (uint qubit = 0; qubit < num_qubits; qubit++) {
            const uint stride = 1u << qubit;
            
            for (uint i = thread_id; i < stride; i += threadgroup_size) {
                const uint idx0 = i;
                const uint idx1 = i + stride;
                
                Complex amp0 = amplitudes[idx0];
                Complex amp1 = amplitudes[idx1];
                
                amplitudes[idx0] = (amp0 + amp1) * SQRT2_INV;
                amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
            }
            
            threadgroup_barrier(mem_flags::mem_device);
        }
        
        // Step 2: Compute average amplitude (parallel reduction)
        float sum_real = 0.0f;
        float sum_imag = 0.0f;
        
        for (uint i = thread_id; i < state_dim; i += threadgroup_size) {
            sum_real += amplitudes[i].real;
            sum_imag += amplitudes[i].imag;
        }
        
        shared_real[thread_id] = sum_real;
        shared_imag[thread_id] = sum_imag;
        threadgroup_barrier(mem_flags::mem_threadgroup);
        
        // Reduction tree
        for (uint s = threadgroup_size / 2; s > 0; s >>= 1) {
            if (thread_id < s) {
                shared_real[thread_id] += shared_real[thread_id + s];
                shared_imag[thread_id] += shared_imag[thread_id + s];
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
        }
        
        float avg_real = shared_real[0] / float(state_dim);
        float avg_imag = shared_imag[0] / float(state_dim);
        
        threadgroup_barrier(mem_flags::mem_threadgroup);
        
        // Step 3: Inversion about average
        for (uint i = thread_id; i < state_dim; i += threadgroup_size) {
            float two_avg_real = avg_real * 2.0f;
            float two_avg_imag = avg_imag * 2.0f;
            amplitudes[i].real = two_avg_real - amplitudes[i].real;
            amplitudes[i].imag = two_avg_imag - amplitudes[i].imag;
        }
        
        threadgroup_barrier(mem_flags::mem_device);
        
        // Step 4: Apply Hadamard again
        for (uint qubit = 0; qubit < num_qubits; qubit++) {
            const uint stride = 1u << qubit;
            
            for (uint i = thread_id; i < stride; i += threadgroup_size) {
                const uint idx0 = i;
                const uint idx1 = i + stride;
                
                Complex amp0 = amplitudes[idx0];
                Complex amp1 = amplitudes[idx1];
                
                amplitudes[idx0] = (amp0 + amp1) * SQRT2_INV;
                amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
            }
            
            threadgroup_barrier(mem_flags::mem_device);
        }
    }
    
    // ========================================
    // MEASUREMENT: Find state with highest probability
    // ========================================
    
    // Each thread finds max in its partition
    uint max_idx = thread_id;
    float max_prob = 0.0f;
    
    for (uint i = thread_id; i < state_dim; i += threadgroup_size) {
        // Compute magnitude_squared inline (avoids device->thread copy)
        float prob = amplitudes[i].real * amplitudes[i].real +
                    amplitudes[i].imag * amplitudes[i].imag;
        if (prob > max_prob) {
            max_prob = prob;
            max_idx = i;
        }
    }
    
    shared_real[thread_id] = max_prob;
    shared_imag[thread_id] = (float)max_idx;  // Store index in imag
    threadgroup_barrier(mem_flags::mem_threadgroup);
    
    // Reduction to find global max
    for (uint s = threadgroup_size / 2; s > 0; s >>= 1) {
        if (thread_id < s) {
            if (shared_real[thread_id + s] > shared_real[thread_id]) {
                shared_real[thread_id] = shared_real[thread_id + s];
                shared_imag[thread_id] = shared_imag[thread_id + s];
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    // Thread 0 writes result
    if (thread_id == 0) {
        results[threadgroup_id] = (uint)shared_imag[0];
    }
}

// ============================================================================
// OPTIMIZED BATCH HADAMARD (ALL SEARCHES, ALL QUBITS)
// ============================================================================

/**
 * @brief Initialize ALL searches with Hadamard in ONE kernel
 * 
 * Each threadgroup handles initialization for ONE search.
 * Processes all qubits for that search.
 */
kernel void batch_hadamard_init(
    device Complex* batch_states [[buffer(0)]],
    constant uint& num_searches [[buffer(1)]],
    constant uint& num_qubits [[buffer(2)]],
    uint threadgroup_id [[threadgroup_position_in_grid]],
    uint thread_id [[thread_position_in_threadgroup]],
    uint threadgroup_size [[threads_per_threadgroup]]
) {
    if (threadgroup_id >= num_searches) return;
    
    const uint state_dim = 1u << num_qubits;
    const uint search_offset = threadgroup_id * state_dim;
    device Complex* amplitudes = &batch_states[search_offset];
    
    // Apply Hadamard to all qubits
    for (uint qubit = 0; qubit < num_qubits; qubit++) {
        const uint stride = 1u << qubit;
        
        for (uint i = thread_id; i < stride; i += threadgroup_size) {
            const uint idx0 = i;
            const uint idx1 = i + stride;
            
            Complex amp0 = amplitudes[idx0];
            Complex amp1 = amplitudes[idx1];
            
            amplitudes[idx0] = (amp0 + amp1) * SQRT2_INV;
            amplitudes[idx1] = (amp0 - amp1) * SQRT2_INV;
        }
        
        threadgroup_barrier(mem_flags::mem_device);
    }
}
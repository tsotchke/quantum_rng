#include "quantum_state.h"
#include "matrix_math.h"
#include "simd_ops.h"
#include "accelerate_ops.h"  // Phase 3: Apple Accelerate + AMX
#include "../common/secure_memory.h"
#include "../common/validation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define NORMALIZATION_TOLERANCE 1e-10
#define SMALL_PROBABILITY 1e-15

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

qs_error_t quantum_state_init(quantum_state_t *state, size_t num_qubits) {
    if (!state) return QS_ERROR_INVALID_STATE;
    if (num_qubits == 0 || num_qubits > MAX_QUBITS) return QS_ERROR_INVALID_DIMENSION;
    
    memset(state, 0, sizeof(quantum_state_t));
    
    state->num_qubits = num_qubits;
    state->state_dim = 1ULL << num_qubits;
    
    // Phase 4: Allocate AMX-aligned memory for optimal M2 Ultra performance
    // Uses 64-byte alignment for Apple Silicon AMX hardware acceleration
    #if HAS_ACCELERATE
    state->amplitudes = accelerate_alloc_complex_array(state->state_dim);
    #else
    state->amplitudes = (complex_t *)calloc(state->state_dim, sizeof(complex_t));
    #endif
    
    if (!state->amplitudes) {
        return QS_ERROR_OUT_OF_MEMORY;
    }
    
    // Zero-initialize (AMX-aligned alloc doesn't guarantee zeroing)
    memset(state->amplitudes, 0, state->state_dim * sizeof(complex_t));
    
    // Initialize to |0...0⟩ state
    state->amplitudes[0] = 1.0 + 0.0*I;
    
    // Allocate measurement history
    state->max_measurements = 1024;
    state->measurement_outcomes = (uint64_t *)calloc(state->max_measurements, sizeof(uint64_t));
    if (!state->measurement_outcomes) {
        free(state->amplitudes);
        return QS_ERROR_OUT_OF_MEMORY;
    }
    
    state->owns_memory = 1;
    state->global_phase = 0.0;
    state->purity = 1.0;  // Pure state
    state->fidelity = 1.0;
    state->num_measurements = 0;
    
    return QS_SUCCESS;
}

void quantum_state_free(quantum_state_t *state) {
    if (!state) return;
    
    if (state->owns_memory) {
        if (state->amplitudes) {
            // Secure zero before freeing (prevent memory dumps)
            secure_memzero(state->amplitudes, state->state_dim * sizeof(complex_t));
            
            // Phase 3: Use Accelerate-aware deallocation
            #if HAS_ACCELERATE
            accelerate_free_complex_array(state->amplitudes);
            #else
            free(state->amplitudes);
            #endif
        }
        
        if (state->measurement_outcomes) {
            // Secure zero measurement history
            secure_memzero(state->measurement_outcomes, state->max_measurements * sizeof(uint64_t));
            free(state->measurement_outcomes);
        }
    }
    
    secure_memzero(state, sizeof(quantum_state_t));
}

qs_error_t quantum_state_from_amplitudes(
    quantum_state_t *state,
    const complex_t *amplitudes,
    size_t dim
) {
    if (!state || !amplitudes) return QS_ERROR_INVALID_STATE;
    
    // Determine number of qubits from dimension
    size_t num_qubits = 0;
    size_t check_dim = dim;
    while (check_dim > 1) {
        if (check_dim & 1) return QS_ERROR_INVALID_DIMENSION;  // Not power of 2
        check_dim >>= 1;
        num_qubits++;
    }
    
    if (num_qubits > MAX_QUBITS) return QS_ERROR_INVALID_DIMENSION;
    
    // Initialize state
    qs_error_t err = quantum_state_init(state, num_qubits);
    if (err != QS_SUCCESS) return err;
    
    // Copy amplitudes
    memcpy(state->amplitudes, amplitudes, dim * sizeof(complex_t));
    
    // Verify and normalize
    if (!quantum_state_is_normalized(state, NORMALIZATION_TOLERANCE)) {
        quantum_state_normalize(state);
    }
    
    return QS_SUCCESS;
}

qs_error_t quantum_state_clone(quantum_state_t *dest, const quantum_state_t *src) {
    if (!dest || !src) return QS_ERROR_INVALID_STATE;
    
    qs_error_t err = quantum_state_init(dest, src->num_qubits);
    if (err != QS_SUCCESS) return err;
    
    // Copy amplitudes
    memcpy(dest->amplitudes, src->amplitudes, src->state_dim * sizeof(complex_t));
    
    // Copy properties
    dest->global_phase = src->global_phase;
    dest->entanglement_entropy = src->entanglement_entropy;
    dest->purity = src->purity;
    dest->fidelity = src->fidelity;
    
    // Copy measurement history
    size_t measurements_to_copy = src->num_measurements;
    if (measurements_to_copy > dest->max_measurements) {
        measurements_to_copy = dest->max_measurements;
    }
    memcpy(dest->measurement_outcomes, src->measurement_outcomes, 
           measurements_to_copy * sizeof(uint64_t));
    dest->num_measurements = measurements_to_copy;
    
    return QS_SUCCESS;
}

void quantum_state_reset(quantum_state_t *state) {
    if (!state || !state->amplitudes) return;
    
    // Reset to |0...0⟩
    memset(state->amplitudes, 0, state->state_dim * sizeof(complex_t));
    state->amplitudes[0] = 1.0 + 0.0*I;
    
    state->global_phase = 0.0;
    state->purity = 1.0;
    state->fidelity = 1.0;
    state->num_measurements = 0;
}

// ============================================================================
// STATE PROPERTIES
// ============================================================================

int quantum_state_is_normalized(const quantum_state_t *state, double tolerance) {
    if (!state || !state->amplitudes) return 0;
    
    // Phase 3: Use Accelerate framework for AMX acceleration (5-10x additional speedup)
    #if HAS_ACCELERATE
    double sum = accelerate_sum_squared_magnitudes(state->amplitudes, state->state_dim);
    #else
    // Fallback: Use SIMD for fast normalization check (8-16x faster with AVX2)
    double sum = simd_sum_squared_magnitudes(state->amplitudes, state->state_dim);
    #endif
    
    return fabs(sum - 1.0) < tolerance;
}

qs_error_t quantum_state_normalize(quantum_state_t *state) {
    VALIDATE_QUANTUM_STATE(state, QS_ERROR_INVALID_STATE);
    
    // Phase 3: Calculate norm using Accelerate framework (AMX-accelerated)
    #if HAS_ACCELERATE
    double norm_squared = accelerate_sum_squared_magnitudes(state->amplitudes, state->state_dim);
    #else
    // Fallback: Calculate norm using SIMD (8-16x faster with AVX2)
    double norm_squared = simd_sum_squared_magnitudes(state->amplitudes, state->state_dim);
    #endif
    
    if (norm_squared < SMALL_PROBABILITY) {
        return QS_ERROR_NOT_NORMALIZED;
    }
    
    double norm = sqrt(norm_squared);
    
    // Phase 3: Normalize using Accelerate framework (AMX-accelerated)
    #if HAS_ACCELERATE
    accelerate_normalize_amplitudes(state->amplitudes, state->state_dim, norm);
    #else
    // Fallback: Normalize using SIMD (4-8x faster with AVX2)
    simd_normalize_amplitudes(state->amplitudes, state->state_dim, norm);
    #endif
    
    return QS_SUCCESS;
}

double quantum_state_entropy(const quantum_state_t *state) {
    if (!state || !state->amplitudes) return 0.0;
    
    double entropy = 0.0;
    
    for (size_t i = 0; i < state->state_dim; i++) {
        double prob = cabs(state->amplitudes[i]);
        prob = prob * prob;  // |α|²
        
        if (prob > SMALL_PROBABILITY) {
            entropy -= prob * log2(prob);
        }
    }
    
    return entropy;
}

double quantum_state_purity(const quantum_state_t *state) {
    if (!state || !state->amplitudes) return 0.0;
    
    // For pure state: Tr(ρ²) = (Σ|αᵢ|²)² = 1
    // For mixed state: Tr(ρ²) < 1
    double purity = 0.0;
    
    for (size_t i = 0; i < state->state_dim; i++) {
        double prob = cabs(state->amplitudes[i]);
        prob = prob * prob;
        purity += prob * prob;
    }
    
    return purity;
}

double quantum_state_fidelity(const quantum_state_t *state1, const quantum_state_t *state2) {
    if (!state1 || !state2 || !state1->amplitudes || !state2->amplitudes) return 0.0;
    if (state1->num_qubits != state2->num_qubits) return 0.0;
    
    // Fidelity F = |⟨ψ|φ⟩|²
    complex_t inner_product = 0.0;
    
    for (size_t i = 0; i < state1->state_dim; i++) {
        // ⟨ψ|φ⟩ = Σ ψᵢ* φᵢ
        inner_product += conj(state1->amplitudes[i]) * state2->amplitudes[i];
    }
    
    double fidelity = cabs(inner_product);
    return fidelity * fidelity;
}

complex_t quantum_state_get_amplitude(const quantum_state_t *state, uint64_t basis_index) {
    if (!state || !state->amplitudes || basis_index >= state->state_dim) {
        return 0.0;
    }
    return state->amplitudes[basis_index];
}

double quantum_state_get_probability(const quantum_state_t *state, uint64_t basis_index) {
    if (!state || !state->amplitudes || basis_index >= state->state_dim) {
        return 0.0;
    }
    
    complex_t amplitude = state->amplitudes[basis_index];
    double prob = cabs(amplitude);
    return prob * prob;
}

// ============================================================================
// ENTANGLEMENT MEASURES
// ============================================================================

double quantum_state_entanglement_entropy(
    const quantum_state_t *state,
    const int *qubits_subsystem_a,
    size_t num_qubits_a
) {
    // Validate inputs
    if (!state || !qubits_subsystem_a || num_qubits_a == 0) return 0.0;
    if (num_qubits_a >= state->num_qubits) return 0.0;
    
    // Calculate dimensions
    size_t num_qubits_b = state->num_qubits - num_qubits_a;
    size_t dim_a = 1ULL << num_qubits_a;
    
    // Initialize all pointers to NULL for cleanup
    int *qubits_b = NULL;
    int *in_a = NULL;
    complex_t *rho_a = NULL;
    double *eigenvalues = NULL;
    complex_t *eigenvectors = NULL;
    double entropy = 0.0;
    
    // Allocate memory with goto cleanup pattern
    qubits_b = (int *)calloc(num_qubits_b, sizeof(int));
    if (!qubits_b) goto cleanup;
    
    in_a = (int *)calloc(state->num_qubits, sizeof(int));
    if (!in_a) goto cleanup;
    
    // Mark which qubits are in A
    for (size_t i = 0; i < num_qubits_a; i++) {
        if (qubits_subsystem_a[i] >= 0 && qubits_subsystem_a[i] < (int)state->num_qubits) {
            in_a[qubits_subsystem_a[i]] = 1;
        }
    }
    
    // Build list of qubits in B
    size_t b_idx = 0;
    for (size_t i = 0; i < state->num_qubits; i++) {
        if (!in_a[i]) {
            qubits_b[b_idx++] = i;
        }
    }
    
    // Get reduced density matrix
    rho_a = (complex_t *)calloc(dim_a * dim_a, sizeof(complex_t));
    if (!rho_a) goto cleanup;
    
    qs_error_t err = quantum_state_partial_trace(state, qubits_b, num_qubits_b, rho_a);
    if (err != QS_SUCCESS) goto cleanup;
    
    // Allocate for eigenvalue decomposition
    eigenvalues = (double *)calloc(dim_a, sizeof(double));
    if (!eigenvalues) goto cleanup;
    
    eigenvectors = (complex_t *)calloc(dim_a * dim_a, sizeof(complex_t));
    if (!eigenvectors) goto cleanup;
    
    // Compute eigenvalues
    int status = hermitian_eigen_decomposition(
        rho_a, dim_a, eigenvalues, eigenvectors, 0, 0);
    
    if (status != 0) goto cleanup;
    
    // Calculate von Neumann entropy: S = -Σᵢ λᵢ log₂(λᵢ)
    for (size_t i = 0; i < dim_a; i++) {
        double lambda = eigenvalues[i];
        if (lambda > SMALL_PROBABILITY) {
            entropy -= lambda * log2(lambda);
        }
    }
    
cleanup:
    // Cleanup all allocations (safe even if NULL)
    if (qubits_b) free(qubits_b);
    if (in_a) free(in_a);
    if (rho_a) free(rho_a);
    if (eigenvalues) free(eigenvalues);
    if (eigenvectors) free(eigenvectors);
    
    return entropy;
}

qs_error_t quantum_state_partial_trace(
    const quantum_state_t *state,
    const int *qubits_to_trace,
    size_t num_traced,
    complex_t *reduced_density
) {
    if (!state || !qubits_to_trace || !reduced_density) {
        return QS_ERROR_INVALID_STATE;
    }
    
    if (num_traced == 0 || num_traced >= state->num_qubits) {
        return QS_ERROR_INVALID_DIMENSION;
    }
    
    // Calculate dimensions
    size_t num_kept = state->num_qubits - num_traced;
    size_t dim_kept = 1ULL << num_kept;
    size_t dim_traced = 1ULL << num_traced;
    
    // Create mask for kept qubits
    uint64_t kept_mask = 0;
    uint64_t traced_mask = 0;
    
    // Build masks
    int *is_traced = (int *)calloc(state->num_qubits, sizeof(int));
    if (!is_traced) return QS_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < num_traced; i++) {
        if (qubits_to_trace[i] < 0 || qubits_to_trace[i] >= (int)state->num_qubits) {
            free(is_traced);
            return QS_ERROR_INVALID_QUBIT;
        }
        is_traced[qubits_to_trace[i]] = 1;
        traced_mask |= (1ULL << qubits_to_trace[i]);
    }
    
    for (size_t i = 0; i < state->num_qubits; i++) {
        if (!is_traced[i]) {
            kept_mask |= (1ULL << i);
        }
    }
    
    // Initialize reduced density matrix to zero
    memset(reduced_density, 0, dim_kept * dim_kept * sizeof(complex_t));
    
    // Compute partial trace: ρ_A = Tr_B(|ψ⟩⟨ψ|)
    // ρ_A(i,j) = Σ_k ⟨i,k|ψ⟩⟨ψ|j,k⟩ = Σ_k ψ*_{i,k} ψ_{j,k}
    
    for (uint64_t kept_i = 0; kept_i < dim_kept; kept_i++) {
        for (uint64_t kept_j = 0; kept_j < dim_kept; kept_j++) {
            complex_t sum = 0.0;
            
            // Sum over all traced qubits
            for (uint64_t traced = 0; traced < dim_traced; traced++) {
                // Construct full basis indices
                uint64_t basis_i = 0;
                uint64_t basis_j = 0;
                
                // Interleave kept and traced qubit indices
                size_t kept_bit_i = 0;
                size_t kept_bit_j = 0;
                size_t traced_bit = 0;
                
                for (size_t qubit = 0; qubit < state->num_qubits; qubit++) {
                    if (is_traced[qubit]) {
                        // Use traced index
                        if (traced & (1ULL << traced_bit)) {
                            basis_i |= (1ULL << qubit);
                            basis_j |= (1ULL << qubit);
                        }
                        traced_bit++;
                    } else {
                        // Use kept index
                        if (kept_i & (1ULL << kept_bit_i)) {
                            basis_i |= (1ULL << qubit);
                        }
                        if (kept_j & (1ULL << kept_bit_j)) {
                            basis_j |= (1ULL << qubit);
                        }
                        kept_bit_i++;
                        kept_bit_j++;
                    }
                }
                
                // Add contribution: ψ*_i × ψ_j
                sum += conj(state->amplitudes[basis_i]) * state->amplitudes[basis_j];
            }
            
            // Store in reduced density matrix
            reduced_density[kept_i * dim_kept + kept_j] = sum;
        }
    }
    
    free(is_traced);
    return QS_SUCCESS;
}

// ============================================================================
// MEASUREMENT HISTORY
// ============================================================================

void quantum_state_record_measurement(quantum_state_t *state, uint64_t outcome) {
    if (!state || !state->measurement_outcomes) return;
    
    if (state->num_measurements < state->max_measurements) {
        state->measurement_outcomes[state->num_measurements] = outcome;
        state->num_measurements++;
    } else {
        // Circular buffer: overwrite oldest
        memmove(state->measurement_outcomes, 
                state->measurement_outcomes + 1,
                (state->max_measurements - 1) * sizeof(uint64_t));
        state->measurement_outcomes[state->max_measurements - 1] = outcome;
    }
}

size_t quantum_state_get_measurement_history(
    const quantum_state_t *state,
    uint64_t *outcomes,
    size_t max_outcomes
) {
    if (!state || !outcomes) return 0;
    
    size_t count = state->num_measurements < max_outcomes ? 
                   state->num_measurements : max_outcomes;
    
    memcpy(outcomes, state->measurement_outcomes, count * sizeof(uint64_t));
    return count;
}

void quantum_state_clear_measurements(quantum_state_t *state) {
    if (!state) return;
    state->num_measurements = 0;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void quantum_state_print(const quantum_state_t *state, size_t max_terms) {
    if (!state || !state->amplitudes) return;
    
    printf("Quantum State (");
    printf("%zu qubits, dim=%zu):\n", state->num_qubits, state->state_dim);
    
    size_t terms_printed = 0;
    for (size_t i = 0; i < state->state_dim && terms_printed < max_terms; i++) {
        complex_t amp = state->amplitudes[i];
        double prob = cabs(amp);
        prob = prob * prob;
        
        if (prob > SMALL_PROBABILITY) {
            char basis_str[MAX_QUBITS + 1];
            quantum_basis_state_string(i, state->num_qubits, basis_str, sizeof(basis_str));
            
            printf("  ");
            if (creal(amp) != 0.0) printf("%.6f", creal(amp));
            if (cimag(amp) > 0.0) {
                printf("+%.6fi", cimag(amp));
            } else if (cimag(amp) < 0.0) {
                printf("%.6fi", cimag(amp));
            }
            printf(" |%s⟩ (p=%.6f)\n", basis_str, prob);
            
            terms_printed++;
        }
    }
    
    if (terms_printed < state->state_dim) {
        printf("  ... (%zu more terms)\n", state->state_dim - terms_printed);
    }
    
    printf("Properties:\n");
    printf("  Entropy: %.6f bits\n", quantum_state_entropy(state));
    printf("  Purity: %.6f\n", quantum_state_purity(state));
    printf("  Measurements: %zu\n", state->num_measurements);
}

void quantum_basis_state_string(
    uint64_t basis_index,
    size_t num_qubits,
    char *buffer,
    size_t buffer_size
) {
    if (!buffer || buffer_size < num_qubits + 1) return;
    
    for (size_t i = 0; i < num_qubits; i++) {
        size_t bit_pos = num_qubits - 1 - i;
        buffer[i] = (basis_index & (1ULL << bit_pos)) ? '1' : '0';
    }
    buffer[num_qubits] = '\0';
}
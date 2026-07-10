#ifndef QUANTUM_STATE_H
#define QUANTUM_STATE_H

#include <stdint.h>
#include <stddef.h>
#include <complex.h>
#include <math.h>

// Use C99 complex types
typedef double _Complex complex_t;

/**
 * @file quantum_state.h
 * @brief Advanced quantum state vector simulation engine
 *
 * Implements full quantum circuit simulation with:
 * - Up to 32 qubits (4.3B dimensional state space) - Phase 4 scaling
 * - Universal gate set (Pauli, Hadamard, Phase, CNOT, Toffoli)
 * - Wavefunction collapse on measurement
 * - Quantum entanglement and superposition
 * - Bell inequality violation capability
 *
 * M2 Ultra with 192GB RAM can handle:
 * - 32 qubits: 4.3B states = 68.7GB (comfortable)
 * - 30 qubits: 1.07B states = 17.2GB (easy)
 * - 28 qubits: 268M states = 4.3GB (trivial)
 */

#define MAX_QUBITS 32  // Phase 4: Scale to 32 qubits (4.3B states, 68.7GB)
#define MAX_STATE_DIM (1ULL << MAX_QUBITS)  // 2^32 = 4,294,967,296
#define RECOMMENDED_MAX_QUBITS 28  // Sweet spot: 268M states = 4.3GB (fast + safe)

// Error codes specific to quantum simulation
typedef enum {
    QS_SUCCESS = 0,
    QS_ERROR_INVALID_QUBIT = -1,
    QS_ERROR_INVALID_STATE = -2,
    QS_ERROR_NOT_NORMALIZED = -3,
    QS_ERROR_OUT_OF_MEMORY = -4,
    QS_ERROR_INVALID_DIMENSION = -5
} qs_error_t;

/**
 * @brief Quantum state representation
 * 
 * Represents a pure quantum state |ψ⟩ = Σ αᵢ|i⟩ where:
 * - αᵢ are complex amplitudes
 * - Σ|αᵢ|² = 1 (normalization)
 * - |i⟩ are computational basis states
 */
typedef struct {
    size_t num_qubits;              // Number of qubits
    size_t state_dim;               // 2^num_qubits
    complex_t *amplitudes;          // State vector coefficients
    
    // Quantum properties
    double global_phase;            // Global phase factor
    double entanglement_entropy;    // Von Neumann entropy
    double purity;                  // Tr(ρ²)
    double fidelity;                // State fidelity
    
    // Measurement history for verification
    uint64_t *measurement_outcomes;
    size_t num_measurements;
    size_t max_measurements;
    
    // Memory management
    int owns_memory;                // 1 if we allocated amplitudes
} quantum_state_t;

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

/**
 * @brief Initialize quantum state in |0...0⟩
 *
 * Phase 4: Scales to 32 qubits (4.3B states, 68.7GB with 192GB RAM)
 * Uses Accelerate framework with AMX-aligned memory (64-byte boundaries)
 * for optimal M2 Ultra performance.
 *
 * Memory Requirements:
 * - 20 qubits: 1M states = 16MB
 * - 22 qubits: 4.2M states = 67MB
 * - 24 qubits: 16.8M states = 268MB
 * - 26 qubits: 67M states = 1.1GB
 * - 28 qubits: 268M states = 4.3GB (recommended max for speed)
 * - 30 qubits: 1.07B states = 17.2GB
 * - 32 qubits: 4.3B states = 68.7GB (feasible with 192GB RAM!)
 *
 * @param state Pointer to quantum state structure
 * @param num_qubits Number of qubits (1-32, recommended 28 for speed/memory balance)
 * @return QS_SUCCESS or error code
 */
qs_error_t quantum_state_init(quantum_state_t *state, size_t num_qubits);

/**
 * @brief Free quantum state resources
 * @param state Quantum state to free
 */
void quantum_state_free(quantum_state_t *state);

/**
 * @brief Create arbitrary quantum state from amplitudes
 * @param state Quantum state to initialize
 * @param amplitudes Array of complex amplitudes
 * @param dim Dimension of state space
 * @return QS_SUCCESS or error code
 */
qs_error_t quantum_state_from_amplitudes(
    quantum_state_t *state,
    const complex_t *amplitudes,
    size_t dim
);

/**
 * @brief Clone quantum state (deep copy)
 * @param dest Destination state
 * @param src Source state
 * @return QS_SUCCESS or error code
 */
qs_error_t quantum_state_clone(quantum_state_t *dest, const quantum_state_t *src);

/**
 * @brief Reset state to |0...0⟩
 * @param state Quantum state to reset
 */
void quantum_state_reset(quantum_state_t *state);

// ============================================================================
// STATE PROPERTIES
// ============================================================================

/**
 * @brief Check if state is normalized (Σ|αᵢ|² = 1)
 * @param state Quantum state
 * @param tolerance Tolerance for normalization check
 * @return 1 if normalized, 0 otherwise
 */
int quantum_state_is_normalized(const quantum_state_t *state, double tolerance);

/**
 * @brief Normalize quantum state
 * @param state Quantum state to normalize
 * @return QS_SUCCESS or error code
 */
qs_error_t quantum_state_normalize(quantum_state_t *state);

/**
 * @brief Calculate von Neumann entropy S = -Tr(ρ log₂ ρ)
 * @param state Quantum state
 * @return Entropy in bits
 */
double quantum_state_entropy(const quantum_state_t *state);

/**
 * @brief Calculate state purity Tr(ρ²)
 * @param state Quantum state
 * @return Purity (1 for pure state, <1 for mixed)
 */
double quantum_state_purity(const quantum_state_t *state);

/**
 * @brief Calculate fidelity F = |⟨ψ|φ⟩|²
 * @param state1 First quantum state
 * @param state2 Second quantum state
 * @return Fidelity between 0 and 1
 */
double quantum_state_fidelity(const quantum_state_t *state1, const quantum_state_t *state2);

/**
 * @brief Get probability amplitude for basis state |i⟩
 * @param state Quantum state
 * @param basis_index Index of basis state
 * @return Complex amplitude αᵢ
 */
complex_t quantum_state_get_amplitude(const quantum_state_t *state, uint64_t basis_index);

/**
 * @brief Get probability for measuring basis state |i⟩
 * @param state Quantum state
 * @param basis_index Index of basis state
 * @return Probability |αᵢ|²
 */
double quantum_state_get_probability(const quantum_state_t *state, uint64_t basis_index);

// ============================================================================
// ENTANGLEMENT MEASURES
// ============================================================================

/**
 * @brief Calculate entanglement entropy between subsystems
 * 
 * Computes von Neumann entropy of reduced density matrix.
 * For pure bipartite states: S(ρ_A) = S(ρ_B) quantifies entanglement.
 * 
 * @param state Full quantum state
 * @param qubits_subsystem_a Qubits in subsystem A
 * @param num_qubits_a Number of qubits in A
 * @return Entanglement entropy in bits
 */
double quantum_state_entanglement_entropy(
    const quantum_state_t *state,
    const int *qubits_subsystem_a,
    size_t num_qubits_a
);

/**
 * @brief Compute reduced density matrix for subsystem
 * 
 * Partial trace: ρ_A = Tr_B(|ψ⟩⟨ψ|)
 * 
 * @param state Full quantum state
 * @param qubits_to_trace Qubits to trace out
 * @param num_traced Number of qubits to trace
 * @param reduced_density Output: reduced density matrix
 * @return QS_SUCCESS or error code
 */
qs_error_t quantum_state_partial_trace(
    const quantum_state_t *state,
    const int *qubits_to_trace,
    size_t num_traced,
    complex_t *reduced_density
);

// ============================================================================
// MEASUREMENT HISTORY
// ============================================================================

/**
 * @brief Record measurement outcome
 * @param state Quantum state
 * @param outcome Measurement outcome (basis index)
 */
void quantum_state_record_measurement(quantum_state_t *state, uint64_t outcome);

/**
 * @brief Get measurement history
 * @param state Quantum state
 * @param outcomes Output array for outcomes
 * @param max_outcomes Maximum outcomes to retrieve
 * @return Number of measurements retrieved
 */
size_t quantum_state_get_measurement_history(
    const quantum_state_t *state,
    uint64_t *outcomes,
    size_t max_outcomes
);

/**
 * @brief Clear measurement history
 * @param state Quantum state
 */
void quantum_state_clear_measurements(quantum_state_t *state);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Print quantum state (debug)
 * @param state Quantum state
 * @param max_terms Maximum number of terms to print
 */
void quantum_state_print(const quantum_state_t *state, size_t max_terms);

/**
 * @brief Get string representation of basis state
 * @param basis_index Basis state index
 * @param num_qubits Number of qubits
 * @param buffer Output buffer
 * @param buffer_size Size of buffer
 */
void quantum_basis_state_string(
    uint64_t basis_index,
    size_t num_qubits,
    char *buffer,
    size_t buffer_size
);

#endif /* QUANTUM_STATE_H */
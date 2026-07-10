#ifndef QUANTUM_GATES_H
#define QUANTUM_GATES_H

#include "quantum_state.h"
#include "quantum_entropy.h"
#include <stdint.h>

/**
 * @file quantum_gates.h
 * @brief Complete universal quantum gate set
 * 
 * Implements all gates needed for universal quantum computation:
 * - Single-qubit gates: Pauli, Hadamard, Phase, Rotation
 * - Two-qubit gates: CNOT, CZ, SWAP, Controlled-Phase
 * - Three-qubit gates: Toffoli, Fredkin
 * - Multi-qubit gates: Multi-controlled gates, QFT
 * 
 * All gates preserve state normalization and properly handle
 * quantum superposition, entanglement, and phase.
 */

// ============================================================================
// SINGLE-QUBIT GATES
// ============================================================================

/**
 * @brief Pauli-X gate (NOT gate, bit flip)
 * |0⟩ → |1⟩, |1⟩ → |0⟩
 * Matrix: [0 1; 1 0]
 */
qs_error_t gate_pauli_x(quantum_state_t *state, int qubit);

/**
 * @brief Pauli-Y gate (bit and phase flip)
 * |0⟩ → i|1⟩, |1⟩ → -i|0⟩
 * Matrix: [0 -i; i 0]
 */
qs_error_t gate_pauli_y(quantum_state_t *state, int qubit);

/**
 * @brief Pauli-Z gate (phase flip)
 * |0⟩ → |0⟩, |1⟩ → -|1⟩
 * Matrix: [1 0; 0 -1]
 */
qs_error_t gate_pauli_z(quantum_state_t *state, int qubit);

/**
 * @brief Hadamard gate (creates superposition)
 * |0⟩ → (|0⟩ + |1⟩)/√2
 * |1⟩ → (|0⟩ - |1⟩)/√2
 * Matrix: [1 1; 1 -1]/√2
 */
qs_error_t gate_hadamard(quantum_state_t *state, int qubit);

/**
 * @brief S gate (Phase gate, √Z)
 * |0⟩ → |0⟩, |1⟩ → i|1⟩
 * Matrix: [1 0; 0 i]
 */
qs_error_t gate_s(quantum_state_t *state, int qubit);

/**
 * @brief S† gate (Inverse S gate)
 * |0⟩ → |0⟩, |1⟩ → -i|1⟩
 * Matrix: [1 0; 0 -i]
 */
qs_error_t gate_s_dagger(quantum_state_t *state, int qubit);

/**
 * @brief T gate (π/8 gate, √S)
 * |0⟩ → |0⟩, |1⟩ → e^(iπ/4)|1⟩
 * Matrix: [1 0; 0 e^(iπ/4)]
 */
qs_error_t gate_t(quantum_state_t *state, int qubit);

/**
 * @brief T† gate (Inverse T gate)
 * |0⟩ → |0⟩, |1⟩ → e^(-iπ/4)|1⟩
 * Matrix: [1 0; 0 e^(-iπ/4)]
 */
qs_error_t gate_t_dagger(quantum_state_t *state, int qubit);

/**
 * @brief Arbitrary phase gate
 * |0⟩ → |0⟩, |1⟩ → e^(iθ)|1⟩
 * @param theta Phase angle in radians
 */
qs_error_t gate_phase(quantum_state_t *state, int qubit, double theta);

/**
 * @brief Rotation around X axis
 * R_X(θ) = exp(-iθX/2) = cos(θ/2)I - i*sin(θ/2)X
 * @param theta Rotation angle in radians
 */
qs_error_t gate_rx(quantum_state_t *state, int qubit, double theta);

/**
 * @brief Rotation around Y axis
 * R_Y(θ) = exp(-iθY/2) = cos(θ/2)I - i*sin(θ/2)Y
 * @param theta Rotation angle in radians
 */
qs_error_t gate_ry(quantum_state_t *state, int qubit, double theta);

/**
 * @brief Rotation around Z axis
 * R_Z(θ) = exp(-iθZ/2) = [e^(-iθ/2) 0; 0 e^(iθ/2)]
 * @param theta Rotation angle in radians
 */
qs_error_t gate_rz(quantum_state_t *state, int qubit, double theta);

/**
 * @brief Arbitrary single-qubit unitary U3 gate
 * U3(θ,φ,λ) - most general single-qubit gate
 * @param theta Rotation angle
 * @param phi Phase angle
 * @param lambda Global phase
 */
qs_error_t gate_u3(quantum_state_t *state, int qubit, double theta, double phi, double lambda);

// ============================================================================
// TWO-QUBIT GATES
// ============================================================================

/**
 * @brief CNOT gate (Controlled-NOT)
 * Flips target if control is |1⟩
 * Creates entanglement
 * @param control Control qubit index
 * @param target Target qubit index
 */
qs_error_t gate_cnot(quantum_state_t *state, int control, int target);

/**
 * @brief CZ gate (Controlled-Z)
 * Applies Z to target if control is |1⟩
 * @param control Control qubit index
 * @param target Target qubit index
 */
qs_error_t gate_cz(quantum_state_t *state, int control, int target);

/**
 * @brief CY gate (Controlled-Y)
 * Applies Y to target if control is |1⟩
 * @param control Control qubit index
 * @param target Target qubit index
 */
qs_error_t gate_cy(quantum_state_t *state, int control, int target);

/**
 * @brief SWAP gate
 * Swaps the states of two qubits
 * @param qubit1 First qubit index
 * @param qubit2 Second qubit index
 */
qs_error_t gate_swap(quantum_state_t *state, int qubit1, int qubit2);

/**
 * @brief Controlled-Phase gate
 * Applies phase if control is |1⟩
 * @param control Control qubit index
 * @param target Target qubit index
 * @param theta Phase angle
 */
qs_error_t gate_cphase(quantum_state_t *state, int control, int target, double theta);

/**
 * @brief Controlled-Rotation X gate
 * @param control Control qubit index
 * @param target Target qubit index
 * @param theta Rotation angle
 */
qs_error_t gate_crx(quantum_state_t *state, int control, int target, double theta);

/**
 * @brief Controlled-Rotation Y gate
 * @param control Control qubit index
 * @param target Target qubit index
 * @param theta Rotation angle
 */
qs_error_t gate_cry(quantum_state_t *state, int control, int target, double theta);

/**
 * @brief Controlled-Rotation Z gate
 * @param control Control qubit index
 * @param target Target qubit index
 * @param theta Rotation angle
 */
qs_error_t gate_crz(quantum_state_t *state, int control, int target, double theta);

// ============================================================================
// THREE-QUBIT GATES
// ============================================================================

/**
 * @brief Toffoli gate (CCNOT, Controlled-Controlled-NOT)
 * Flips target if both controls are |1⟩
 * Universal for classical computation
 * @param control1 First control qubit
 * @param control2 Second control qubit
 * @param target Target qubit
 */
qs_error_t gate_toffoli(quantum_state_t *state, int control1, int control2, int target);

/**
 * @brief Fredkin gate (CSWAP, Controlled-SWAP)
 * Swaps two targets if control is |1⟩
 * @param control Control qubit
 * @param target1 First target qubit
 * @param target2 Second target qubit
 */
qs_error_t gate_fredkin(quantum_state_t *state, int control, int target1, int target2);

// ============================================================================
// MULTI-QUBIT GATES
// ============================================================================

/**
 * @brief Multi-controlled X gate (generalized Toffoli)
 * Applies X to target if all controls are |1⟩
 * @param controls Array of control qubit indices
 * @param num_controls Number of control qubits
 * @param target Target qubit index
 */
qs_error_t gate_mcx(quantum_state_t *state, const int *controls, size_t num_controls, int target);

/**
 * @brief Multi-controlled Z gate
 * Applies Z to target if all controls are |1⟩
 * @param controls Array of control qubit indices
 * @param num_controls Number of control qubits
 * @param target Target qubit index
 */
qs_error_t gate_mcz(quantum_state_t *state, const int *controls, size_t num_controls, int target);

/**
 * @brief Quantum Fourier Transform
 * Implements QFT on specified qubits
 * Critical for Shor's algorithm and phase estimation
 * @param qubits Array of qubit indices to transform
 * @param num_qubits Number of qubits
 */
qs_error_t gate_qft(quantum_state_t *state, const int *qubits, size_t num_qubits);

/**
 * @brief Inverse Quantum Fourier Transform
 * @param qubits Array of qubit indices
 * @param num_qubits Number of qubits
 */
qs_error_t gate_iqft(quantum_state_t *state, const int *qubits, size_t num_qubits);

// ============================================================================
// MEASUREMENT
// ============================================================================

/**
 * @brief Measurement basis types
 */
typedef enum {
    MEASURE_COMPUTATIONAL,  // Z-basis: |0⟩, |1⟩
    MEASURE_HADAMARD,      // X-basis: |+⟩, |-⟩
    MEASURE_CIRCULAR,      // Y-basis: |↻⟩, |↺⟩
    MEASURE_CUSTOM         // Custom basis
} measurement_basis_t;

/**
 * @brief Measurement result
 */
typedef struct {
    int outcome;           // 0 or 1
    double probability;    // P(outcome)
    double entropy;        // Measurement entropy
} measurement_result_t;

/**
 * @brief Measure single qubit
 * Collapses wavefunction to measurement outcome
 *
 * SECURITY: Uses cryptographically secure entropy for measurement simulation.
 * The entropy source must be unpredictable to maintain security properties.
 *
 * @param state Quantum state
 * @param qubit Qubit to measure
 * @param basis Measurement basis
 * @param entropy Secure entropy source for measurement sampling
 * @return Measurement result
 */
measurement_result_t quantum_measure(
    quantum_state_t *state,
    int qubit,
    measurement_basis_t basis,
    quantum_entropy_ctx_t *entropy
);

/**
 * @brief Measure multiple qubits
 * @param state Quantum state
 * @param qubits Array of qubit indices
 * @param num_qubits Number of qubits to measure
 * @param outcomes Output array for outcomes
 * @param entropy Secure entropy source
 * @return QS_SUCCESS or error
 */
qs_error_t quantum_measure_multi(
    quantum_state_t *state,
    const int *qubits,
    size_t num_qubits,
    int *outcomes,
    quantum_entropy_ctx_t *entropy
);

/**
 * @brief Non-destructive measurement (peek)
 * Gets probability without collapsing state
 * @param state Quantum state
 * @param qubit Qubit to peek
 * @param outcome Outcome to check (0 or 1)
 * @return Probability of outcome
 */
double quantum_peek_probability(
    const quantum_state_t *state,
    int qubit,
    int outcome
);

/**
 * @brief PERFORMANCE-CRITICAL: Fast batch measurement of all qubits
 *
 * Measures entire basis state in ONE pass instead of measuring qubits separately.
 * 8x faster for 8-qubit systems!
 *
 * @param state Quantum state (will be collapsed)
 * @param entropy Secure entropy source for sampling
 * @return Measured basis state index (contains all qubit outcomes)
 */
uint64_t quantum_measure_all_fast(
    quantum_state_t *state,
    quantum_entropy_ctx_t *entropy
);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Apply arbitrary single-qubit unitary
 * @param state Quantum state
 * @param qubit Target qubit
 * @param matrix 2x2 unitary matrix
 */
qs_error_t apply_single_qubit_gate(
    quantum_state_t *state,
    int qubit,
    const complex_t matrix[2][2]
);

/**
 * @brief Apply arbitrary two-qubit unitary
 * @param state Quantum state
 * @param qubit1 First qubit
 * @param qubit2 Second qubit
 * @param matrix 4x4 unitary matrix
 */
qs_error_t apply_two_qubit_gate(
    quantum_state_t *state,
    int qubit1,
    int qubit2,
    const complex_t matrix[4][4]
);

/**
 * @brief Check if gate preserves normalization
 * @param state Quantum state (after gate application)
 * @return 1 if normalized, 0 otherwise
 */
int verify_gate_normalization(const quantum_state_t *state);

#endif /* QUANTUM_GATES_H */
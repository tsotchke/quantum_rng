/*
 * quantum_measure.c - quantum state measurement
 *
 * Basis-state, partial, and fast full-state measurement. Kept separate from
 * gate application (quantum_gates.c) because measurement is conceptually the
 * dual of unitary evolution and is the only consumer-facing "readout" path.
 */
#include "quantum_gates.h"
#include "quantum_entropy.h"
#include "quantum_constants.h"
#include "simd_ops.h"
#include "quantum_bit_utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

qs_error_t quantum_measure_multi(
    quantum_state_t *state,
    const int *qubits,
    size_t num_qubits,
    int *outcomes,
    quantum_entropy_ctx_t *entropy
) {
    if (!state || !qubits || !outcomes) return QS_ERROR_INVALID_STATE;
    if (!entropy) return QS_ERROR_INVALID_STATE;
    
    for (size_t i = 0; i < num_qubits; i++) {
        measurement_result_t result = quantum_measure(state, qubits[i], MEASURE_COMPUTATIONAL, entropy);
        outcomes[i] = result.outcome;
    }
    
    return QS_SUCCESS;
}

double quantum_peek_probability(
    const quantum_state_t *state,
    int qubit,
    int outcome
) {
    if (!state || !state->amplitudes || !check_qubit_valid(state, qubit)) {
        return 0.0;
    }
    
    double prob = 0.0;
    for (uint64_t i = 0; i < state->state_dim; i++) {
        if (get_bit(i, qubit) == outcome) {
            double amp = cabs(state->amplitudes[i]);
            prob += amp * amp;
        }
    }
    
    return prob;
}

/*
 * PERFORMANCE-CRITICAL: Fast batch measurement of all qubits.
 * Samples the complete basis state in ONE pass instead of measuring each
 * qubit separately (8x speedup for measurement-heavy workloads).
 */
uint64_t quantum_measure_all_fast(
    quantum_state_t *state,
    quantum_entropy_ctx_t *entropy
) {
    if (!state || !state->amplitudes || !entropy) {
        return 0;
    }
    
    double random;
    if (quantum_entropy_get_double(entropy, &random) != 0) {
        return 0;
    }
    
    double cumulative = 0.0;
    for (uint64_t i = 0; i < state->state_dim; i++) {
        const double mag = cabs(state->amplitudes[i]);
        const double prob = mag * mag;
        cumulative += prob;
        
        if (random < cumulative) {
            memset(state->amplitudes, 0, state->state_dim * sizeof(complex_t));
            state->amplitudes[i] = 1.0;
            quantum_state_record_measurement(state, i);
            return i;
        }
    }
    
    // Numerical precision fallback
    uint64_t final_state = state->state_dim - 1;
    memset(state->amplitudes, 0, state->state_dim * sizeof(complex_t));
    state->amplitudes[final_state] = 1.0;
    quantum_state_record_measurement(state, final_state);
    
    return final_state;
}

/*
 * quantum_bit_utils.h - tiny inline bit/qubit index helpers shared by the
 * gate and measurement modules.
 *
 * Declared `static inline` so every translation unit that includes this header
 * gets its own copy (no link-time symbol, no cross-TU dependency). This keeps
 * quantum_gates.c and quantum_measure.c self-contained without duplicating
 * the logic by hand.
 */
#ifndef QUANTUM_BIT_UTILS_H
#define QUANTUM_BIT_UTILS_H

#include "quantum_state.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int check_qubit_valid(const quantum_state_t *state, int qubit) {
    return (qubit >= 0 && qubit < (int)state->num_qubits);
}

static inline uint64_t flip_bit(uint64_t n, int bit_pos) {
    return n ^ (1ULL << bit_pos);
}

static inline int get_bit(uint64_t n, int bit_pos) {
    return (int)((n >> bit_pos) & 1ULL);
}

#ifdef __cplusplus
}
#endif

#endif /* QUANTUM_BIT_UTILS_H */

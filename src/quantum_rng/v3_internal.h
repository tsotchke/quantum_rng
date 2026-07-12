/*
 * v3_internal.h - private, project-internal declarations for the quantum_rng_v3
 * implementation modules.
 *
 * This header is NOT part of the public API. It exists so the v3 implementation
 * can be split across several cohesive translation units (init, extraction,
 * generation, Grover sampling, verification, stats, utils) without leaking these
 * helpers into quantum_rng_v3.h. Each helper is defined exactly once
 * (in v3_extract.c / v3_init.c) and referenced by the other modules through
 * this header.
 */
#ifndef QRNG_V3_INTERNAL_H
#define QRNG_V3_INTERNAL_H

#include "quantum_rng_v3.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Entropy callback bridging the hardware pool (Layer 1) to quantum measurements
 * (Layer 2). Defined in v3_init.c. */
int entropy_pool_callback(void *user_data, uint8_t *buffer, size_t size);

/* Evolve a quantum state by applying `num_gates` random gates selected with
 * hardware entropy. Defined in v3_extract.c. */
qs_error_t evolve_quantum_state(quantum_state_t *state,
                                quantum_entropy_ctx_t *entropy_ctx,
                                size_t num_gates);

/* Direct-mode entropy extraction: evolve + batch-measure the quantum state,
 * conditioning each measurement with an independent entropy word. */
int extract_quantum_entropy(qrng_v3_ctx_t *ctx, uint8_t *buffer, size_t size);

/* Grover-mode entropy extraction: prepare a Grover-amplified distribution once
 * per batch, then measure many times. */
int extract_grover_entropy(qrng_v3_ctx_t *ctx, uint8_t *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* QRNG_V3_INTERNAL_H */

/*
 * secure_internal.h - private, project-internal declarations for the secure_rng
 * implementation modules.
 *
 * Not part of the public API. Lets the secure_rng implementation be split across
 * cohesive translation units (init, generation, stats, self-test) without leaking
 * these helpers into secure_rng.h. Shared static helpers are defined once (in
 * secure_init.c for the entropy/lock helpers, reseed.c for reseeding) and
 * referenced by the other modules through this header.
 */
#ifndef SECURE_RNG_INTERNAL_H
#define SECURE_RNG_INTERNAL_H

#include "secure_rng.h"
#include "../common/secure_memory.h"
#include "../common/validation.h"
#include "../quantum_rng/quantum_state.h"
#include "../quantum_rng/bell_test.h"
#include "../quantum_rng/quantum_entropy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal constants (shared across modules). */
#define SECURE_RNG_VERSION_MAJOR 2
#define SECURE_RNG_VERSION_MINOR 0
#define SECURE_RNG_VERSION_PATCH 0

#define STARTUP_ENTROPY_SIZE 2048
#define RESEED_ENTROPY_SIZE 1024
#define MIN_ENTROPY_FOR_RESEED 256
#define DEFAULT_RESEED_INTERVAL (1024 * 1024)  /* 1MB */
#define DEFAULT_HYBRID_THRESHOLD 1024           /* FAST < 1KB, QUANTUM >= 1KB */

/* Thread-safety lock helpers (defined in secure_init.c). */
secure_rng_error_t lock_write(secure_rng_ctx_t *ctx);
secure_rng_error_t lock_read(const secure_rng_ctx_t *ctx);
secure_rng_error_t unlock(const secure_rng_ctx_t *ctx);

/* Error-callback dispatcher (defined in secure_init.c). */
void invoke_error_callback(secure_rng_ctx_t *ctx,
                           secure_rng_error_t error,
                           const char *message);

/* Collect + health-test entropy (defined in secure_init.c). */
secure_rng_error_t collect_tested_entropy(secure_rng_ctx_t *ctx,
                                          uint8_t *buffer,
                                          size_t size);

/* Whether auto-reseed is due (defined in secure_init.c). */
int reseed_needed(const secure_rng_ctx_t *ctx);

/* VERIFIED-mode CHSH Bell certification (defined in secure_init.c). */
secure_rng_error_t run_bell_certification(secure_rng_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_RNG_INTERNAL_H */

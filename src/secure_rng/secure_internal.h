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
#ifdef __cplusplus
extern "C" {
#endif

/* Internal helpers are NOT part of the public shared-library ABI. Mark them
 * hidden so they never appear in the dynamic export surface (a private header
 * alone does not hide externally-linked symbols). Definitions in the .c files
 * carry the same attribute. */
#ifndef SECURE_RNG_INTERNAL
#if defined(__GNUC__) || defined(__clang__)
#define SECURE_RNG_INTERNAL __attribute__((visibility("hidden")))
#else
#define SECURE_RNG_INTERNAL
#endif
#endif

/* Thread-safety lock helpers (defined in secure_init.c). */
SECURE_RNG_INTERNAL secure_rng_error_t secure_rng_lock_write(secure_rng_ctx_t *ctx);
SECURE_RNG_INTERNAL secure_rng_error_t secure_rng_lock_read(const secure_rng_ctx_t *ctx);
SECURE_RNG_INTERNAL secure_rng_error_t secure_rng_unlock(const secure_rng_ctx_t *ctx);

/* Error-callback dispatcher (defined in secure_init.c). */
SECURE_RNG_INTERNAL void secure_rng_invoke_error_callback(secure_rng_ctx_t *ctx,
                           secure_rng_error_t error,
                           const char *message);

/* Collect + health-test entropy (defined in secure_init.c). */
SECURE_RNG_INTERNAL secure_rng_error_t secure_rng_collect_tested_entropy(secure_rng_ctx_t *ctx,
                                          uint8_t *buffer,
                                          size_t size);

/* Whether auto-reseed is due (defined in secure_init.c). */
SECURE_RNG_INTERNAL int secure_rng_reseed_needed(const secure_rng_ctx_t *ctx);

/* VERIFIED-mode CHSH Bell certification (defined in secure_init.c). */
SECURE_RNG_INTERNAL secure_rng_error_t secure_rng_run_bell_certification(secure_rng_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_RNG_INTERNAL_H */

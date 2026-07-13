#ifndef SLERMES_QRNG_H
#define SLERMES_QRNG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A small, honest, deterministic PRNG built on splitmix64 mixing.
 *
 * This is a classical pseudo-random generator. It is NOT a source of
 * true entropy and it is NOT quantum. When seeded with a fixed seed it
 * is fully reproducible (useful for tests); when seeded from the
 * operating system it is merely "unpredictable to the extent the OS
 * entropy source is".
 */

typedef struct qrng_ctx qrng_ctx;

/* Create a context. If seed is NULL (and seed_len == 0) the context is
 * seeded from the best available OS entropy source (getrandom(2) /
 * /dev/urandom, falling back to time+pid). The "from_os" out-arg (may be
 * NULL) reports whether an OS source was actually used. */
qrng_ctx *qrng_create(const void *seed, size_t seed_len, int *from_os);

/* Free a context. */
void qrng_free(qrng_ctx *ctx);

/* Next 64-bit value. */
uint64_t qrng_next_u64(qrng_ctx *ctx);

/* Next double in [0, 1). */
double qrng_next_double(qrng_ctx *ctx);

/* Uniform integer in [lo, hi] inclusive. */
int64_t qrng_range(qrng_ctx *ctx, int64_t lo, int64_t hi);

/* Fill a buffer with `n` bytes of output. */
void qrng_fill(qrng_ctx *ctx, void *buf, size_t n);

/* ---- Honest entropy estimation ----------------------------------------
 * These measure the ACTUAL statistical properties of a stream of bytes
 * produced by the generator. They make NO claim about "true" entropy;
 * they report measured quantities and simple pass/fail heuristics.
 */

typedef struct {
    double shannon_bits_per_byte; /* measured Shannon entropy, 0..8 */
    double chi_square;            /* Pearson chi-square for byte frequencies */
    double chi_square_p;          /* approximate p-value (0..1); lower = worse */
    long   runs_total;            /* number of runs (same-bit streaks) observed */
    double runs_expected;         /* expected runs for an independent stream */
    int    ok;                    /* 1 if all simple heuristics pass */
} qrng_entropy_report;

/* Estimate entropy from `n` bytes in `data`. */
qrng_entropy_report qrng_entropy_estimate(const void *data, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_QRNG_H */

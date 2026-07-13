#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "qrng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#  include <sys/syscall.h>
#  include <linux/random.h>
#endif

/* ------------------------------------------------------------------ */
/* splitmix64 mixing                                                   */
/* ------------------------------------------------------------------ */
static inline uint64_t splitmix64(uint64_t x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* A counter-based generator: state is a 128-bit (seed, counter) pair
 * mixed through splitmix64. Output is splitmix64(seed + counter*phi)
 * re-mixed. Fully deterministic given the seed. */
struct qrng_ctx {
    uint64_t seed;
    uint64_t counter;
    uint64_t last; /* previous output, for a tiny extra avalanche */
};

/* One mixing step that also folds in the previous output. */
static inline uint64_t mix_step(uint64_t seed, uint64_t counter, uint64_t last) {
    uint64_t x = splitmix64(seed + counter * 0x9E3779B97F4A7C15ULL);
    x ^= last;
    return splitmix64(x);
}

/* ---- OS entropy (best effort) -------------------------------------- */
static int os_entropy(uint64_t *out, size_t count) {
#if defined(__linux__)
    /* getrandom(2) if available (glibc >= 2.25 exposes it). */
#  if defined(SYS_getrandom)
    long r = syscall(SYS_getrandom, out, count * sizeof(uint64_t), 0);
    if (r == (long)(count * sizeof(uint64_t))) return 1;
#  endif
    /* Fall back to /dev/urandom. */
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t got = fread(out, sizeof(uint64_t), count, f);
        fclose(f);
        if (got == count) return 1;
    }
#endif
    return 0;
}

qrng_ctx *qrng_create(const void *seed, size_t seed_len, int *from_os) {
    qrng_ctx *ctx = (qrng_ctx *)malloc(sizeof(qrng_ctx));
    if (!ctx) { if (from_os) *from_os = 0; return NULL; }

    uint64_t s = 0;
    int got_os = 0;

    if (seed && seed_len > 0) {
        /* Hash the seed bytes (FNV-1a style) into a 64-bit state. */
        const unsigned char *p = (const unsigned char *)seed;
        s = 1469598103934665603ULL; /* FNV offset basis */
        for (size_t i = 0; i < seed_len; i++) {
            s ^= p[i];
            s *= 1099511628211ULL; /* FNV prime */
        }
        got_os = 0;
    } else {
        /* Try OS entropy; otherwise mix time + pid. */
        uint64_t buf[2];
        if (os_entropy(buf, 2)) {
            s = buf[0] ^ splitmix64(buf[1]);
            got_os = 1;
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            uint64_t t = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
            s = splitmix64(t ^ ((uint64_t)getpid() << 32) ^ ((uint64_t)getppid() << 16));
            got_os = 0;
            fprintf(stderr,
                "qrng: warning: OS entropy source unavailable; using weak "
                "time+pid seed (NOT cryptographically secure)\n");
        }
    }

    ctx->seed    = splitmix64(s ^ 0x1234567890ABCDEFULL);
    ctx->counter = 1;
    ctx->last    = 0;
    if (from_os) *from_os = got_os;
    return ctx;
}

void qrng_free(qrng_ctx *ctx) { free(ctx); }

uint64_t qrng_next_u64(qrng_ctx *ctx) {
    uint64_t x = mix_step(ctx->seed, ctx->counter, ctx->last);
    ctx->last = x;
    ctx->counter++;
    return x;
}

double qrng_next_double(qrng_ctx *ctx) {
    /* 53 bits of mantissa. */
    uint64_t x = qrng_next_u64(ctx);
    return ((double)(x >> 11)) / (double)(1ULL << 53);
}

int64_t qrng_range(qrng_ctx *ctx, int64_t lo, int64_t hi) {
    if (hi < lo) { int64_t t = lo; lo = hi; hi = t; }
    uint64_t span = (uint64_t)(hi - lo) + 1;
    /* rejection sample to avoid modulo bias */
    uint64_t limit = (~(uint64_t)0) - (~(uint64_t)0) % span;
    for (;;) {
        uint64_t x = qrng_next_u64(ctx);
        if (x <= limit) return lo + (int64_t)(x % span);
    }
}

void qrng_fill(qrng_ctx *ctx, void *buf, size_t n) {
    unsigned char *p = (unsigned char *)buf;
    size_t i = 0;
    while (i + 8 <= n) {
        uint64_t x = qrng_next_u64(ctx);
        memcpy(p + i, &x, 8);
        i += 8;
    }
    if (i < n) {
        uint64_t x = qrng_next_u64(ctx);
        memcpy(p + i, &x, n - i);
    }
}

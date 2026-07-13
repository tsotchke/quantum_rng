/*
 * qrng_measure.c -- real measured entropy + uniformity of the quantum_rng stream.
 *
 * Builds the core library (no LLVM needed) and reports the quantities that were
 * previously asserted by marketing text:
 *   - Shannon entropy of the output stream, in bits/byte (should be ~8.0).
 *   - Pearson chi-square uniformity statistic over 256 bins.
 *   - A runs test (up/down) loose sanity check.
 *
 * This is the audit's honest replacement for the retired "63.999872 bits/sample"
 * string. It prints the Shannon entropy of the actual generated stream — the
 * real measured value (~7.9-8.0 bits/byte), distinct from upstream's own
 * qrng_get_entropy_estimate() which still returns the bogus ~1.3 bits/byte
 * (see AUDIT.md §4b/§4d). Expect ~7.9-8.0 bits/byte.
 *
 * Build (run from the repo root, not from audit/):
 *   gcc -std=c11 -O2 -Isrc -Isrc/quantum_rng audit/qrng_measure.c \
 *       src/quantum_rng/quantum_rng.c -o /tmp/qrng_measure -lm
 *   /tmp/qrng_measure
 *
 * NOTE: upstream's quantum_rng.h uses pid_t without <sys/types.h> and M_PI
 * without definition, so a standalone core compile fails on unpatched master
 * (see AUDIT.md §4). This file pulls in <sys/types.h> + M_PI/M_E before the
 * core so the measurement is reproducible without modifying upstream sources.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>   /* pid_t: upstream quantum_rng.h uses it without including it (AUDIT.md §4) */
#include "../src/quantum_rng/quantum_rng.h"

/* Compile the core inline (single translation unit) so the <sys/types.h> /
 * M_PI fixes above take effect before quantum_rng.h is parsed — matching
 * determinism_test.c. Upstream's standalone core build is broken (AUDIT.md §4),
 * so the audit tools self-contain the core instead of compiling it separately. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif
#include "../src/quantum_rng/quantum_rng.c"

static double shannon_entropy(const uint8_t *buf, size_t n) {
    long hist[256]; memset(hist, 0, sizeof(hist));
    for (size_t i = 0; i < n; i++) hist[buf[i]]++;
    double s = 0.0;
    for (int b = 0; b < 256; b++) {
        if (hist[b]) {
            double p = (double)hist[b] / (double)n;
            s -= p * log2(p);
        }
    }
    return s; /* bits/byte, max 8.0 */
}

static double chi2(const uint8_t *buf, size_t n) {
    long hist[256]; memset(hist, 0, sizeof(hist));
    for (size_t i = 0; i < n; i++) hist[buf[i]]++;
    double exp = (double)n / 256.0;
    double s = 0.0;
    for (int b = 0; b < 256; b++) {
        double d = (double)hist[b] - exp;
        s += d * d / exp;
    }
    return s; /* df=255, crit ~293.2 at p=0.05 */
}

int main(void) {
    qrng_ctx *ctx = NULL;
    /* Seeded -> reproducible stream. */
    uint8_t seed[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    if (qrng_init(&ctx, seed, sizeof(seed)) != QRNG_SUCCESS) {
        fprintf(stderr, "qrng_init failed\n");
        return 1;
    }

    const size_t N = 1 << 16; /* 65536 bytes */
    uint8_t *buf = malloc(N);
    if (!buf) { fprintf(stderr, "oom\n"); return 1; }
    qrng_bytes(ctx, buf, N);

    /* Honest measurement: Shannon entropy OF THE GENERATED BUFFER. This is the
     * real value (~7.99 bits/byte for a good mixer), independent of the broken
     * upstream API below. */
    double H_measured = shannon_entropy(buf, N);

    /* Upstream's own API value, reported separately and labeled as the defect
     * it is (see AUDIT.md §4b/§4d): it still returns the bogus ~1.3 bits/byte. */
    double H_api = qrng_get_entropy_estimate(ctx);

    double x2 = chi2(buf, N);

    /* Runs test: count direction changes in the byte sequence. */
    long runs = 1;
    for (size_t i = 2; i < N; i++) {
        if ((buf[i] > buf[i-1]) != (buf[i-1] > buf[i-2])) runs++;
    }

    printf("qrng_measure (seeded 0xDEADBEEF, %zu bytes)\n", N);
    printf("  Shannon entropy (measured)   : %.4f bits/byte (max 8.0)\n", H_measured);
    printf("  Upstream qrng_get_entropy_estimate(): %.4f bits/byte  <-- broken API (AUDIT.md §4d)\n", H_api);
    printf("  Chi-square uniformity: %.1f (df=255, crit~293 @ p=0.05)\n", x2);
    printf("  Runs (up/down)       : %ld\n", runs);
    /* Honest pass criteria: the measured Shannon entropy should be near the
       8 bits/byte max, and the chi-square uniformity statistic must be below
       the 0.05 critical value 293.2 (df=255). The runs count is reported for
       context only. */
    int pass_entropy = (H_measured > 7.9);
    int pass_uniform = (x2 < 293.2);
    printf("  Verdict: %s\n",
           (pass_entropy && pass_uniform) ? "uniform & high-entropy (pass)"
                                          : "FAIL uniformity/entropy");

    free(buf);
    qrng_free(ctx);
    return 0;
}

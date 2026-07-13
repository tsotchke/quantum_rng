#define _GNU_SOURCE
#include "qrng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

/* ---- Honest entropy measurement -------------------------------------- *
 * These are MEASUREMENTS of the byte stream, not claims about the
 * generator's "true" entropy. A deterministic, seeded PRNG has no true
 * entropy at all; what we can report is how well the output stream
 * statistically resembles an independent uniform source. */
qrng_entropy_report qrng_entropy_estimate(const void *data, size_t n) {
    qrng_entropy_report r;
    memset(&r, 0, sizeof(r));

    if (n == 0) { r.ok = 0; return r; }

    /* Byte-frequency counts. */
    double freq[256];
    for (int i = 0; i < 256; i++) freq[i] = 0.0;
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < n; i++) freq[p[i]]++;

    /* Shannon entropy (bits per byte). */
    double H = 0.0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0.0) {
            double q = freq[i] / (double)n;
            H -= q * log2(q);
        }
    }
    r.shannon_bits_per_byte = H;

    /* Pearson chi-square for uniformity over 256 bins. Expected = n/256. */
    double expected = (double)n / 256.0;
    double chi = 0.0;
    for (int i = 0; i < 256; i++) {
        double d = freq[i] - expected;
        chi += (d * d) / expected;
    }
    r.chi_square = chi;

    /* Approximate chi-square p-value via the Wilson-Hilferty
     * transformation for the chi-square distribution with 255 dof:
     *   p ≈ P(X > chi) ≈ 1 - Φ( ((chi/df)^(1/3) - (1 - 2/(9*df))) /
     *                            sqrt(2/(9*df)) )
     * where Φ is the standard normal CDF. Good enough for a heuristic. */
    double df = 255.0;
    double t = pow(chi / df, 1.0 / 3.0) - (1.0 - 2.0 / (9.0 * df));
    t /= sqrt(2.0 / (9.0 * df));
    /* standard normal CDF via erf. */
    r.chi_square_p = 0.5 * erfc(-t / M_SQRT2);

    /* Runs test (Wald-Wolfowitz style) on the raw bit stream.
     * A run = a maximal consecutive sequence of equal bits. For an
     * independent fair bit stream, expected runs over N bits ≈ (N+1)/2. */
    long runs = 0;
    int prev = -1;
    size_t nbits = n * 8;
    for (size_t i = 0; i < nbits; i++) {
        int b = (p[i >> 3] >> (7 - (i & 7))) & 1;
        if (prev < 0) { runs = 1; prev = b; }
        else if (b != prev) { runs++; prev = b; }
    }
    r.runs_total = runs;
    r.runs_expected = (nbits + 1.0) / 2.0;

    /* Heuristic pass criteria (NOT a proof of randomness):
     *  - Shannon entropy within 1% of 8.0 bits/byte,
     *  - chi-square p-value in [0.01, 0.99] (not suspiciously low/high),
     *  - runs count within 5% of the expected value. */
    int ok = 1;
    if (H < 7.92) ok = 0;
    if (r.chi_square_p < 0.01 || r.chi_square_p > 0.99) ok = 0;
    if (r.runs_expected > 0 &&
        fabs((double)runs - r.runs_expected) > 0.05 * r.runs_expected) ok = 0;
    r.ok = ok;

    return r;
}

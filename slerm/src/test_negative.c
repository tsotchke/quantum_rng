#include "qrng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int nfails = 0;
#define CHECK(c, m) do { \
    if (!(c)) { fprintf(stderr, "FAIL: %s\n", m); nfails++; } \
    else fprintf(stderr, "ok:   %s\n", m); \
} while (0)

/* Negative / edge inputs that must not crash and must behave sanely. */
static void test_range_lo_eq_hi(void) {
    qrng_ctx *c = qrng_create("r", 1, NULL);
    /* lo==hi must always return lo and never go out of bounds. */
    int ok = 1;
    for (int i = 0; i < 100000; i++)
        if (qrng_range(c, 7, 7) != 7) { ok = 0; break; }
    CHECK(ok, "qrng_range lo==hi returns lo exactly");
    qrng_free(c);
}

static void test_range_reversed(void) {
    qrng_ctx *c = qrng_create("r2", 2, NULL);
    int ok = 1;
    for (int i = 0; i < 100000; i++) {
        int64_t x = qrng_range(c, -3, 4);
        if (x < -3 || x > 4) { ok = 0; break; }
    }
    CHECK(ok, "qrng_range with lo>hi (reversed) stays in [hi,lo]");
    qrng_free(c);
}

static void test_entropy_empty_and_tiny(void) {
    unsigned char one = 0xAB;
    qrng_entropy_report r1 = qrng_entropy_estimate(&one, 1);
    CHECK(r1.shannon_bits_per_byte >= 0.0 && r1.shannon_bits_per_byte <= 8.0,
          "entropy on 1 byte is finite/in-range");
    qrng_entropy_report r0 = qrng_entropy_estimate(NULL, 0);
    CHECK(r0.ok == 0, "entropy on empty input reports not-ok (no crash)");
}

static void test_double_range(void) {
    qrng_ctx *c = qrng_create("d", 1, NULL);
    int ok = 1;
    for (int i = 0; i < 100000; i++) {
        double x = qrng_next_double(c);
        if (x < 0.0 || x >= 1.0) { ok = 0; break; }
    }
    CHECK(ok, "qrng_next_double in [0,1)");
    qrng_free(c);
}

static void test_create_null(void) {
    int from_os = -1;
    qrng_ctx *c = qrng_create(NULL, 0, &from_os);
    CHECK(c != NULL, "qrng_create(NULL,0) succeeds (OS or fallback seed)");
    CHECK(from_os == 0 || from_os == 1, "from_os is set (0 fallback / 1 OS)");
    uint64_t a = qrng_next_u64(c), b = qrng_next_u64(c);
    CHECK(a != b, "unseeded stream still varies between draws");
    qrng_free(c);
}

int main(void) {
    test_range_lo_eq_hi();
    test_range_reversed();
    test_entropy_empty_and_tiny();
    test_double_range();
    test_create_null();
    if (nfails) { fprintf(stderr, "\n%d FAILURE(S)\n", nfails); return 1; }
    fprintf(stderr, "\nALL NEGATIVE TESTS PASSED\n");
    return 0;
}

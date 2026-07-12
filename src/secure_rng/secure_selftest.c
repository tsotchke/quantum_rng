/*
 * secure_selftest.c - built-in self-test exercising the full secure_rng API
 */
#include "secure_internal.h"

int secure_rng_self_test(int verbose) {
    if (verbose) {
        printf("=== Secure RNG Self-Test ===\n\n");
    }

    if (verbose) printf("Testing initialization... ");
    secure_rng_ctx_t *ctx;
    secure_rng_error_t err = secure_rng_init(&ctx);
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        return 0;
    }
    if (verbose) printf("PASSED\n");

    if (verbose) printf("Testing byte generation... ");
    uint8_t bytes[100];
    err = secure_rng_bytes(ctx, bytes, sizeof(bytes));
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    if (verbose) printf("Testing uint64 generation... ");
    uint64_t val64;
    err = secure_rng_uint64(ctx, &val64);
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    if (verbose) printf("Testing double generation... ");
    double valdbl;
    err = secure_rng_double(ctx, &valdbl);
    if (err != SECURE_RNG_SUCCESS || valdbl < 0.0 || valdbl >= 1.0) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    if (verbose) printf("Testing range generation... ");
    int32_t valrange;
    err = secure_rng_range32(ctx, 1, 100, &valrange);
    if (err != SECURE_RNG_SUCCESS || valrange < 1 || valrange > 100) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    if (verbose) printf("Testing reseed... ");
    err = secure_rng_reseed(ctx);
    if (err != SECURE_RNG_SUCCESS) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    if (verbose) printf("Testing statistics... ");
    secure_rng_stats_t stats;
    err = secure_rng_get_stats(ctx, &stats);
    if (err != SECURE_RNG_SUCCESS || stats.bytes_generated == 0) {
        if (verbose) printf("FAILED\n");
        secure_rng_free(ctx);
        return 0;
    }
    if (verbose) printf("PASSED\n");

    if (verbose) printf("Testing cleanup... ");
    secure_rng_free(ctx);
    if (verbose) printf("PASSED\n\n");

    if (verbose) {
        printf("All self-tests PASSED\n");
    }

    return 1;
}

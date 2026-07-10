/**
 * @file key_derivation.c
 * @brief Educational password-based key derivation demo.
 *
 * Construction (PBKDF2-like, iterated SHA-256):
 *
 *   state_0 = SHA-256(password || salt || key_size || quantum_mix)
 *   state_i = SHA-256(state_{i-1} || password || salt || i)   (iterations times)
 *   key     = SHA-256(state || salt || block_index) blocks, truncated to key_size
 *
 * The derivation itself is fully DETERMINISTIC given (password, salt,
 * iterations, key_size, quantum_mix) — a KDF must be reproducible so a key
 * can be re-derived for verification. The quantum RNG's role is generating
 * the salt when the caller does not supply one: each new salt gets fresh
 * quantum entropy conditioned through qrng_measure_state().
 *
 * HONESTY NOTE: this is a teaching example. It is not memory-hard and has
 * no security proof; for real systems use Argon2id or scrypt.
 */

#include "key_derivation.h"
#include "sha256.h"
#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Generate a fresh random salt using the quantum RNG (conditioned through a
 * quantum measurement pass). Falls back to failure if no entropy source. */
static int generate_quantum_salt(uint8_t salt[SALT_SIZE]) {
    uint8_t seed[32];
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return -1;
    if (fread(seed, 1, sizeof(seed), f) != sizeof(seed)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    qrng_ctx *ctx = NULL;
    if (qrng_init(&ctx, seed, sizeof(seed)) != QRNG_SUCCESS) {
        return -1;
    }

    int rc = 0;
    if (qrng_bytes(ctx, salt, SALT_SIZE) != QRNG_SUCCESS ||
        qrng_measure_state(ctx, salt, SALT_SIZE) != QRNG_SUCCESS) {
        rc = -1;
    }

    qrng_free(ctx);
    return rc;
}

kdf_result_t derive_key(const kdf_config_t *config) {
    kdf_result_t result = {0};
    struct timespec start, end;

    if (!config) return result;

    /* Validate parameters. */
    size_t key_size = config->key_size;
    if (key_size < MIN_KEY_SIZE || key_size > MAX_KEY_SIZE) return result;

    uint32_t iterations = config->iterations;
    if (iterations == 0) iterations = 1;

    clock_gettime(CLOCK_MONOTONIC, &start);

    result.derived_key = malloc(key_size);
    if (!result.derived_key) return result;
    result.key_size = key_size;

    /* Salt: caller-supplied (reproducible derivation) or fresh quantum salt. */
    if (config->salt_length > 0) {
        size_t n = (size_t)config->salt_length;
        if (n > SALT_SIZE) n = SALT_SIZE;
        memset(result.salt, 0, SALT_SIZE);
        memcpy(result.salt, config->salt, n);
    } else {
        if (generate_quantum_salt(result.salt) != 0) {
            free(result.derived_key);
            result.derived_key = NULL;
            result.key_size = 0;
            return result;
        }
    }

    size_t password_len = strlen(config->password);

    /* state_0 binds all parameters. */
    uint8_t state[SHA256_DIGEST_SIZE];
    sha256_ctx_t h;
    uint16_t ks16 = (uint16_t)key_size;
    sha256_init(&h);
    sha256_update(&h, config->password, password_len);
    sha256_update(&h, result.salt, SALT_SIZE);
    sha256_update(&h, &ks16, sizeof(ks16));
    sha256_update(&h, &config->quantum_mix, sizeof(config->quantum_mix));
    sha256_final(&h, state);

    /* Iterated stretching (deliberately serial). */
    uint32_t progress_step = iterations / 100;
    if (progress_step == 0) progress_step = 1;

    for (uint32_t i = 0; i < iterations; i++) {
        sha256_init(&h);
        sha256_update(&h, state, sizeof(state));
        sha256_update(&h, config->password, password_len);
        sha256_update(&h, result.salt, SALT_SIZE);
        sha256_update(&h, &i, sizeof(i));
        sha256_final(&h, state);

        if (config->show_progress && i % progress_step == 0) {
            fprintf(stderr, "\rProgress: %u%%", i / progress_step);
            fflush(stderr);
        }
    }

    if (config->show_progress) {
        fprintf(stderr, "\rProgress: 100%%\n");
    }

    /* Expand the final state to key_size bytes (counter mode). */
    size_t produced = 0;
    uint32_t block = 0;
    while (produced < key_size) {
        uint8_t out[SHA256_DIGEST_SIZE];
        sha256_init(&h);
        sha256_update(&h, state, sizeof(state));
        sha256_update(&h, result.salt, SALT_SIZE);
        sha256_update(&h, &block, sizeof(block));
        sha256_final(&h, out);

        size_t take = key_size - produced;
        if (take > SHA256_DIGEST_SIZE) take = SHA256_DIGEST_SIZE;
        memcpy(result.derived_key + produced, out, take);
        produced += take;
        block++;
    }

    result.entropy_estimate = estimate_entropy(result.derived_key, key_size);

    clock_gettime(CLOCK_MONOTONIC, &end);
    result.memory_used = sizeof(state) + key_size;
    result.time_taken = (uint64_t)((end.tv_sec - start.tv_sec) * 1000 +
                                   (end.tv_nsec - start.tv_nsec) / 1000000);

    return result;
}

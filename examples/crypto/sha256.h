/**
 * @file sha256.h
 * @brief Self-contained SHA-256 (FIPS 180-4) for the example programs.
 *
 * This is a straightforward, dependency-free SHA-256 implementation used by
 * the educational crypto examples so that "hash" always means a real
 * cryptographic hash. It is not hardware-accelerated and not side-channel
 * hardened; for production use a vetted library (e.g. libcrypto).
 *
 * All functions are static so this header can be included from multiple
 * translation units without link conflicts.
 */

#ifndef EXAMPLES_CRYPTO_SHA256_H
#define EXAMPLES_CRYPTO_SHA256_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    size_t   buffer_len;
} sha256_ctx_t;

static const uint32_t sha256_k_[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t sha256_rotr_(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

static inline void sha256_compress_(sha256_ctx_t *ctx, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = sha256_rotr_(w[i - 15], 7) ^ sha256_rotr_(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = sha256_rotr_(w[i - 2], 17) ^ sha256_rotr_(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++) {
        uint32_t s1 = sha256_rotr_(e, 6) ^ sha256_rotr_(e, 11) ^ sha256_rotr_(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + s1 + ch + sha256_k_[i] + w[i];
        uint32_t s0 = sha256_rotr_(a, 2) ^ sha256_rotr_(a, 13) ^ sha256_rotr_(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static inline void sha256_init(sha256_ctx_t *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->bitlen = 0;
    ctx->buffer_len = 0;
}

static inline void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;

    ctx->bitlen += (uint64_t)len * 8;

    while (len > 0) {
        size_t space = SHA256_BLOCK_SIZE - ctx->buffer_len;
        size_t take = (len < space) ? len : space;

        memcpy(ctx->buffer + ctx->buffer_len, p, take);
        ctx->buffer_len += take;
        p += take;
        len -= take;

        if (ctx->buffer_len == SHA256_BLOCK_SIZE) {
            sha256_compress_(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

static inline void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]) {
    uint64_t bitlen = ctx->bitlen;
    uint8_t pad = 0x80;
    uint8_t zero = 0x00;
    uint8_t len_be[8];
    int i;

    /* Append 0x80, then zeros until 8 bytes remain in the block. */
    sha256_update(ctx, &pad, 1);
    ctx->bitlen -= 8; /* padding does not count toward message length */
    while (ctx->buffer_len != SHA256_BLOCK_SIZE - 8) {
        sha256_update(ctx, &zero, 1);
        ctx->bitlen -= 8;
    }

    for (i = 0; i < 8; i++) {
        len_be[i] = (uint8_t)(bitlen >> (56 - 8 * i));
    }
    sha256_update(ctx, len_be, 8);

    for (i = 0; i < 8; i++) {
        digest[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }

    /* Do not leave message material in the context. */
    memset(ctx, 0, sizeof(*ctx));
}

/** One-shot convenience wrapper: digest = SHA-256(data). */
static inline void sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

#endif /* EXAMPLES_CRYPTO_SHA256_H */

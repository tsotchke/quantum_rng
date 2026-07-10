/**
 * @file secure_token.c
 * @brief Quantum-random secure token generation demo.
 *
 * Tokens are drawn from the quantum RNG with rejection sampling (no modulo
 * bias). Each token carries a SHA-256 based integrity checksum.
 *
 * HONESTY NOTE: the "signature" field is an UNKEYED integrity checksum —
 * it detects accidental corruption, not forgery. A real system would use
 * an HMAC with a server-side secret key or a digital signature.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "../../src/quantum_rng/quantum_rng.h"
#include "secure_token.h"
#include "sha256.h"

#define DEFAULT_TOKEN_LENGTH 32
#define DEFAULT_EXPIRATION_TIME 3600 // 1 hour
#define CHARSET_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define CHARSET_LOWER "abcdefghijklmnopqrstuvwxyz"
#define CHARSET_NUMBERS "0123456789"
#define CHARSET_SPECIAL "!@#$%^&*()-_=+[]{}|;:,.<>?"

static void generate_signature(const unsigned char* token, size_t length, unsigned char* signature);
static int build_charset(const TokenConfig* config, char* charset, size_t* charset_length);

// Lazily-initialized quantum RNG shared by this module.
// (The previous code called quantum_rng_generate(), which no longer exists
// in the v3 API.)
static int quantum_random_bytes(unsigned char* buffer, size_t len) {
    static qrng_ctx* g_ctx = NULL;

    if (!g_ctx) {
        if (qrng_init(&g_ctx, NULL, 0) != QRNG_SUCCESS) {
            return -1;
        }
    }

    return (qrng_bytes(g_ctx, buffer, len) == QRNG_SUCCESS) ? 0 : -1;
}

// Uniform random index in [0, range) using rejection sampling.
// BUG FIX: the previous code used rand_byte % charset_length, which biases
// toward the beginning of the character set.
static int quantum_uniform_index(size_t range, size_t* out) {
    if (range == 0 || range > 256 || !out) return -1;

    unsigned int limit = 256u - (256u % (unsigned int)range);
    for (;;) {
        unsigned char byte;
        if (quantum_random_bytes(&byte, 1) != 0) return -1;
        if (byte < limit) {
            *out = byte % range;
            return 0;
        }
    }
}

void init_token_config(TokenConfig* config) {
    if (!config) return;

    config->token_length = DEFAULT_TOKEN_LENGTH;
    config->expiration_time = DEFAULT_EXPIRATION_TIME;
    config->use_uppercase = 1;
    config->use_lowercase = 1;
    config->use_numbers = 1;
    config->use_special = 0;
}

int generate_secure_token(const TokenConfig* config, TokenMetadata* metadata) {
    if (!config || !metadata || config->token_length == 0) {
        return -1;
    }

    // Allocate token buffer (+1: keep it NUL-terminated so it can safely be
    // used as a C string, e.g. by token_to_string and validate_token)
    metadata->token = (unsigned char*)malloc(config->token_length + 1);
    if (!metadata->token) {
        return -2;
    }

    // Build character set
    char charset[256];
    size_t charset_length;
    if (build_charset(config, charset, &charset_length) != 0) {
        free(metadata->token);
        metadata->token = NULL;
        return -3;
    }

    // Generate token using quantum randomness (bias-free sampling)
    for (size_t i = 0; i < config->token_length; i++) {
        size_t idx;
        if (quantum_uniform_index(charset_length, &idx) != 0) {
            free(metadata->token);
            metadata->token = NULL;
            return -4;
        }
        metadata->token[i] = (unsigned char)charset[idx];
    }
    metadata->token[config->token_length] = '\0';

    // Set metadata
    metadata->length = config->token_length;
    metadata->creation_time = time(NULL);
    metadata->expiration_time = metadata->creation_time + config->expiration_time;

    // Generate integrity checksum
    generate_signature(metadata->token, metadata->length, metadata->signature);

    return 0;
}

int validate_token(const unsigned char* token, const TokenMetadata* metadata) {
    if (!token || !metadata || !metadata->token) {
        return -1;
    }

    // Check length
    if (strlen((const char*)token) != metadata->length) {
        return -2;
    }

    // Compare tokens
    if (memcmp(token, metadata->token, metadata->length) != 0) {
        return -3;
    }

    // Verify integrity checksum
    unsigned char computed_signature[64];
    generate_signature(token, metadata->length, computed_signature);

    if (memcmp(computed_signature, metadata->signature, 64) != 0) {
        return -4;
    }

    // Check expiration
    if (is_token_expired(metadata)) {
        return -5;
    }

    return 0;
}

int is_token_expired(const TokenMetadata* metadata) {
    if (!metadata) return -1;

    time_t current_time = time(NULL);
    return current_time > metadata->expiration_time;
}

int revoke_token(TokenMetadata* metadata) {
    if (!metadata) return -1;

    // Set expiration to the past so is_token_expired() reports expired
    metadata->expiration_time = time(NULL) - 1;
    return 0;
}

int token_to_string(const TokenMetadata* metadata, char* str, size_t str_size) {
    // Format: token:creation:expiration:signature(128 hex chars)
    // Worst case: length + 1 + 20 + 1 + 20 + 1 + 128 + 1
    if (!metadata || !metadata->token || !str ||
        str_size < metadata->length + 128 + 64) {
        return -1;
    }

    int written = snprintf(str, str_size, "%s:%ld:%ld:", (const char*)metadata->token,
                           (long)metadata->creation_time,
                           (long)metadata->expiration_time);
    if (written < 0 || (size_t)written >= str_size) {
        return -2;
    }

    // Append signature as hex
    size_t offset = (size_t)written;
    if (str_size - offset < 64 * 2 + 1) {
        return -2;
    }
    for (size_t i = 0; i < 64; i++) {
        snprintf(str + offset + i * 2, 3, "%02x", metadata->signature[i]);
    }

    return 0;
}

int token_from_string(const char* str, TokenMetadata* metadata) {
    if (!str || !metadata) return -1;

    // Parse token
    const char* creation_ptr = strchr(str, ':');
    if (!creation_ptr || creation_ptr == str) return -2;

    size_t token_length = (size_t)(creation_ptr - str);
    metadata->token = (unsigned char*)malloc(token_length + 1);
    if (!metadata->token) return -3;

    memcpy(metadata->token, str, token_length);
    metadata->token[token_length] = '\0';
    metadata->length = token_length;

    // Parse creation time
    char* end = NULL;
    long creation = strtol(creation_ptr + 1, &end, 10);
    if (!end || *end != ':') {
        free(metadata->token);
        metadata->token = NULL;
        return -4;
    }
    metadata->creation_time = (time_t)creation;

    // Parse expiration time
    long expiration = strtol(end + 1, &end, 10);
    if (!end || *end != ':') {
        free(metadata->token);
        metadata->token = NULL;
        return -5;
    }
    metadata->expiration_time = (time_t)expiration;

    // Parse signature: exactly 128 hex characters must remain
    const char* sig_hex = end + 1;
    if (strlen(sig_hex) < 128) {
        free(metadata->token);
        metadata->token = NULL;
        return -6;
    }
    for (size_t i = 0; i < 64; i++) {
        char hex[3] = { sig_hex[i * 2], sig_hex[i * 2 + 1], '\0' };
        if (!isxdigit((unsigned char)hex[0]) || !isxdigit((unsigned char)hex[1])) {
            free(metadata->token);
            metadata->token = NULL;
            return -6;
        }
        metadata->signature[i] = (unsigned char)strtol(hex, NULL, 16);
    }

    return 0;
}

void cleanup_token_metadata(TokenMetadata* metadata) {
    if (!metadata) return;

    free(metadata->token);
    metadata->token = NULL;
    metadata->length = 0;
}

int generate_multiple_tokens(const TokenConfig* config,
                           TokenMetadata* tokens,
                           size_t num_tokens) {
    if (!config || !tokens || num_tokens == 0) {
        return -1;
    }

    for (size_t i = 0; i < num_tokens; i++) {
        int result = generate_secure_token(config, &tokens[i]);
        if (result != 0) {
            // Clean up previously generated tokens
            for (size_t j = 0; j < i; j++) {
                cleanup_token_metadata(&tokens[j]);
            }
            return result;
        }

        // Ensure uniqueness
        for (size_t j = 0; j < i; j++) {
            if (tokens[i].length == tokens[j].length &&
                memcmp(tokens[i].token, tokens[j].token, tokens[i].length) == 0) {
                cleanup_token_metadata(&tokens[i]);
                i--; // Retry this token
                break;
            }
        }
    }

    return (int)num_tokens;
}

// Internal helper functions

// Deterministic SHA-256 based integrity checksum over the token and a
// domain-separation label. 64-byte field is H || SHA-256(H).
//
// BUG FIX: the previous version XORed *fresh random bytes* into the
// signature on every call, so recomputing it during validate_token() could
// never reproduce the stored value — validation always failed.
static void generate_signature(const unsigned char* token, size_t length, unsigned char* signature) {
    static const char label[] = "secure-token-checksum-v1";
    uint64_t len64 = (uint64_t)length;

    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, label, sizeof(label) - 1);
    sha256_update(&ctx, &len64, sizeof(len64));
    sha256_update(&ctx, token, length);
    sha256_final(&ctx, signature);

    sha256(signature, SHA256_DIGEST_SIZE, signature + SHA256_DIGEST_SIZE);
}

static int build_charset(const TokenConfig* config, char* charset, size_t* charset_length) {
    if (!config || !charset || !charset_length) {
        return -1;
    }

    *charset_length = 0;

    if (config->use_uppercase) {
        strcpy(charset + *charset_length, CHARSET_UPPER);
        *charset_length += strlen(CHARSET_UPPER);
    }

    if (config->use_lowercase) {
        strcpy(charset + *charset_length, CHARSET_LOWER);
        *charset_length += strlen(CHARSET_LOWER);
    }

    if (config->use_numbers) {
        strcpy(charset + *charset_length, CHARSET_NUMBERS);
        *charset_length += strlen(CHARSET_NUMBERS);
    }

    if (config->use_special) {
        strcpy(charset + *charset_length, CHARSET_SPECIAL);
        *charset_length += strlen(CHARSET_SPECIAL);
    }

    if (*charset_length == 0) {
        return -2;
    }

    return 0;
}

// ============================================================================
// DEMONSTRATION DRIVER
// ============================================================================
//
// Exercises the module end to end: generation at several lengths, validation
// (including a tamper-detection check), serialize/parse round-trip, revocation,
// and a measured character-class distribution over a batch of tokens.

#ifndef SECURE_TOKEN_NO_MAIN

#include <getopt.h>

static void classify_bytes(const unsigned char* buf, size_t len,
                           size_t counts[4]) {
    // counts: [0]=upper [1]=lower [2]=digit [3]=special
    for (size_t i = 0; i < len; i++) {
        unsigned char c = buf[i];
        if (c >= 'A' && c <= 'Z')       counts[0]++;
        else if (c >= 'a' && c <= 'z')  counts[1]++;
        else if (c >= '0' && c <= '9')  counts[2]++;
        else                            counts[3]++;
    }
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -n <count>    tokens to generate for the distribution batch (default 64)\n");
    printf("  -l <length>   token length used for the batch (default 32)\n");
    printf("  -s            include special characters in the batch charset\n");
    printf("  -h            show this help and exit\n");
}

int main(int argc, char** argv) {
    size_t batch_count = 64;
    size_t batch_length = 32;
    int    batch_special = 0;

    int opt;
    while ((opt = getopt(argc, argv, "n:l:sh")) != -1) {
        switch (opt) {
            case 'n': {
                long v = strtol(optarg, NULL, 10);
                if (v <= 0) { fprintf(stderr, "count must be > 0\n"); return 1; }
                batch_count = (size_t)v;
                break;
            }
            case 'l': {
                long v = strtol(optarg, NULL, 10);
                if (v <= 0) { fprintf(stderr, "length must be > 0\n"); return 1; }
                batch_length = (size_t)v;
                break;
            }
            case 's': batch_special = 1; break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    printf("=== Quantum Secure Token Demo ===\n\n");

    // ---- 1. Generate tokens at several lengths and validate each ----
    printf("1. Generation and validation at several lengths\n");
    printf("   %-8s %-6s %s\n", "length", "valid", "token");
    printf("   -------- ------ --------------------------------------------\n");

    const size_t demo_lengths[] = { 16, 32, 48 };
    const size_t num_demo = sizeof(demo_lengths) / sizeof(demo_lengths[0]);

    TokenMetadata demo_tok;         // reused; freed each iteration
    for (size_t i = 0; i < num_demo; i++) {
        TokenConfig cfg;
        init_token_config(&cfg);
        cfg.token_length = demo_lengths[i];
        cfg.use_special = 1;

        if (generate_secure_token(&cfg, &demo_tok) != 0) {
            fprintf(stderr, "   generation failed at length %zu\n", demo_lengths[i]);
            return 1;
        }

        int v = validate_token(demo_tok.token, &demo_tok);
        printf("   %-8zu %-6s %s\n",
               demo_lengths[i], v == 0 ? "yes" : "NO", (const char*)demo_tok.token);
        cleanup_token_metadata(&demo_tok);
    }
    printf("\n");

    // ---- 2. Tamper detection ----
    printf("2. Tamper detection (integrity checksum)\n");
    {
        TokenConfig cfg;
        init_token_config(&cfg);
        cfg.token_length = 24;
        if (generate_secure_token(&cfg, &demo_tok) != 0) {
            fprintf(stderr, "   generation failed\n");
            return 1;
        }

        printf("   original : %s -> validate = %d (expect 0)\n",
               (const char*)demo_tok.token, validate_token(demo_tok.token, &demo_tok));

        // Flip one character of a COPY and validate against original metadata.
        unsigned char tampered[64];
        memcpy(tampered, demo_tok.token, demo_tok.length + 1);
        tampered[0] = (tampered[0] == 'A') ? 'B' : 'A';
        printf("   tampered : %s -> validate = %d (expect < 0)\n",
               (const char*)tampered, validate_token(tampered, &demo_tok));
        cleanup_token_metadata(&demo_tok);
    }
    printf("\n");

    // ---- 3. Serialize / parse round-trip ----
    printf("3. Serialize / parse round-trip\n");
    {
        TokenConfig cfg;
        init_token_config(&cfg);
        cfg.token_length = 20;
        if (generate_secure_token(&cfg, &demo_tok) != 0) {
            fprintf(stderr, "   generation failed\n");
            return 1;
        }

        char serialized[512];
        if (token_to_string(&demo_tok, serialized, sizeof(serialized)) != 0) {
            fprintf(stderr, "   serialization failed\n");
            cleanup_token_metadata(&demo_tok);
            return 1;
        }
        printf("   serialized: %s\n", serialized);

        TokenMetadata parsed;
        memset(&parsed, 0, sizeof(parsed));
        if (token_from_string(serialized, &parsed) != 0) {
            fprintf(stderr, "   parse failed\n");
            cleanup_token_metadata(&demo_tok);
            return 1;
        }

        int match = (parsed.length == demo_tok.length) &&
                    memcmp(parsed.token, demo_tok.token, demo_tok.length) == 0 &&
                    memcmp(parsed.signature, demo_tok.signature, 64) == 0;
        printf("   parsed token matches original: %s\n", match ? "yes" : "NO");
        printf("   parsed token validates       : %d (expect 0)\n",
               validate_token(parsed.token, &parsed));

        cleanup_token_metadata(&parsed);
        cleanup_token_metadata(&demo_tok);
    }
    printf("\n");

    // ---- 4. Revocation ----
    printf("4. Revocation\n");
    {
        TokenConfig cfg;
        init_token_config(&cfg);
        cfg.token_length = 20;
        if (generate_secure_token(&cfg, &demo_tok) != 0) {
            fprintf(stderr, "   generation failed\n");
            return 1;
        }
        printf("   expired before revoke: %d (expect 0)\n", is_token_expired(&demo_tok));
        revoke_token(&demo_tok);
        printf("   expired after  revoke: %d (expect 1)\n", is_token_expired(&demo_tok));
        printf("   validate after revoke: %d (expect < 0, token now expired)\n",
               validate_token(demo_tok.token, &demo_tok));
        cleanup_token_metadata(&demo_tok);
    }
    printf("\n");

    // ---- 5. Batch statistics: measured character-class distribution ----
    printf("5. Batch statistics (%zu unique tokens, length %zu%s)\n",
           batch_count, batch_length, batch_special ? ", with special chars" : "");

    TokenMetadata* batch = (TokenMetadata*)calloc(batch_count, sizeof(TokenMetadata));
    if (!batch) {
        fprintf(stderr, "   out of memory\n");
        return 1;
    }

    TokenConfig bcfg;
    init_token_config(&bcfg);
    bcfg.token_length = batch_length;
    bcfg.use_special  = batch_special;

    int made = generate_multiple_tokens(&bcfg, batch, batch_count);
    if (made != (int)batch_count) {
        fprintf(stderr, "   batch generation failed (%d)\n", made);
        free(batch);
        return 1;
    }

    size_t counts[4] = {0, 0, 0, 0};
    size_t total_bytes = 0;
    size_t valid = 0;
    for (size_t i = 0; i < batch_count; i++) {
        classify_bytes(batch[i].token, batch[i].length, counts);
        total_bytes += batch[i].length;
        if (validate_token(batch[i].token, &batch[i]) == 0) valid++;
    }

    const char* names[4] = { "uppercase", "lowercase", "digits", "special" };
    printf("   generated : %zu   all-unique+valid: %zu/%zu\n",
           batch_count, valid, batch_count);
    printf("   sampled bytes: %zu\n", total_bytes);
    for (int i = 0; i < 4; i++) {
        double pct = total_bytes ? 100.0 * (double)counts[i] / (double)total_bytes : 0.0;
        printf("     %-10s %8zu   %5.1f%%\n", names[i], counts[i], pct);
    }

    for (size_t i = 0; i < batch_count; i++) cleanup_token_metadata(&batch[i]);
    free(batch);

    printf("\nDone.\n");
    return 0;
}

#endif // SECURE_TOKEN_NO_MAIN

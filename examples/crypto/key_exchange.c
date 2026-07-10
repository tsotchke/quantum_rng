/**
 * @file key_exchange.c
 * @brief Educational key exchange DEMO.
 *
 * HONESTY NOTE: this is a toy protocol for demonstrating the *structure* of
 * a key exchange (key material generation, transcript hashing, session key
 * derivation). The "shared secret" is the XOR of the two public values, so
 * a passive eavesdropper who sees both public keys can compute it. It is
 * NOT a secure key exchange — real systems use (EC)DH or a KEM. Hashing is
 * real SHA-256; randomness comes from the quantum RNG.
 */

#include "key_exchange.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ENTROPY_POOL_SIZE 4096

static uint8_t entropy_pool[ENTROPY_POOL_SIZE];
static size_t pool_position = 0;
static int pool_initialized = 0;

static void init_entropy_pool(qrng_ctx *ctx);
static void get_entropy_bytes(qrng_ctx *ctx, uint8_t *buffer, size_t len);

void init_exchange_config(exchange_config_t *config) {
    config->role = ROLE_INITIATOR;
    strncpy(config->seed, "key_exchange", sizeof(config->seed) - 1);
    config->seed[sizeof(config->seed) - 1] = '\0';
    config->seed_length = (int)strlen(config->seed);
    config->output_mode = OUTPUT_NORMAL;
    config->show_progress = 1;
    config->output_file[0] = '\0';
    config->verify_entropy = 1;
    config->rounds = 3;
    config->interactive = 0;
}

static void init_entropy_pool(qrng_ctx *ctx) {
    uint8_t temp_pool[ENTROPY_POOL_SIZE];
    double pool_entropy;
    int attempts = 0;
    const int max_attempts = 10;
    
    do {
        if (qrng_bytes(ctx, temp_pool, ENTROPY_POOL_SIZE) != 0) {
            fprintf(stderr, "Error: Failed to get quantum random bytes\n");
            exit(1);
        }
        
        for (size_t i = 0; i < ENTROPY_POOL_SIZE; i++) {
            uint8_t rot = (temp_pool[i] << 3) | (temp_pool[i] >> 5);
            temp_pool[i] = rot ^ (i & 0xFF);
            if (i > 0) {
                temp_pool[i] ^= temp_pool[i-1];
            }
        }
        
        pool_entropy = estimate_entropy(temp_pool, ENTROPY_POOL_SIZE);
        
        if (pool_entropy >= MIN_ENTROPY) {
            memcpy(entropy_pool, temp_pool, ENTROPY_POOL_SIZE);
            break;
        }
        
        attempts++;
    } while (attempts < max_attempts);
    
    if (attempts == max_attempts) {
        fprintf(stderr, "Warning: Could not achieve desired entropy pool quality\n");
        memcpy(entropy_pool, temp_pool, ENTROPY_POOL_SIZE);
    }

    pool_position = 0;
    pool_initialized = 1;
}

static void get_entropy_bytes(qrng_ctx *ctx, uint8_t *buffer, size_t len) {
    // BUG FIX: the pool was previously consumed before ever being filled,
    // silently handing out all-zero "entropy" on first use.
    if (!pool_initialized || pool_position >= ENTROPY_POOL_SIZE) {
        init_entropy_pool(ctx);
    }
    
    while (len > 0) {
        size_t available = ENTROPY_POOL_SIZE - pool_position;
        size_t to_copy = (len < available) ? len : available;
        
        memcpy(buffer, entropy_pool + pool_position, to_copy);
        pool_position += to_copy;
        buffer += to_copy;
        len -= to_copy;
        
        if (len > 0) {
            init_entropy_pool(ctx);
        }
    }
}

/*
 * BUG FIX: the previous "enhanced_sha256" was not a hash at all — it drew
 * random bytes from a qrng seeded with the input plus a mutable global
 * entropy pool, so its output was not even a deterministic function of the
 * input. It is replaced by real SHA-256 (see sha256.h).
 */

void generate_key_material(qrng_ctx *ctx, key_material_t *keys) {
    uint8_t temp[KEY_SIZE * 4];
    uint8_t mixed[KEY_SIZE];
    double key_entropy;
    
    // Get initial quantum randomness
    if (qrng_bytes(ctx, keys->private_key, KEY_SIZE) != 0) {
        fprintf(stderr, "Error: Failed to generate initial private key\n");
        exit(1);
    }
    
    // Multiple rounds of mixing with fresh quantum randomness
    for (int round = 0; round < 4; round++) {
        // Get fresh quantum randomness
        if (qrng_bytes(ctx, temp, sizeof(temp)) != 0) {
            fprintf(stderr, "Error: Failed to get quantum random bytes\n");
            exit(1);
        }
        
        // Mix with previous key
        for (int i = 0; i < KEY_SIZE; i++) {
            mixed[i] = keys->private_key[i];
            // XOR with multiple quantum random bytes
            for (int j = 0; j < 4; j++) {
                mixed[i] ^= temp[i + j * KEY_SIZE];
            }
            // Rotate and mix
            mixed[i] = (mixed[i] << 3) | (mixed[i] >> 5);
            if (i > 0) {
                mixed[i] ^= mixed[i-1];
            }
        }
        memcpy(keys->private_key, mixed, KEY_SIZE);
    }
    
    // Mix in bytes from the conditioned entropy pool as a final pass
    uint8_t pool_bytes[KEY_SIZE];
    get_entropy_bytes(ctx, pool_bytes, KEY_SIZE);
    for (int i = 0; i < KEY_SIZE; i++) {
        keys->private_key[i] ^= pool_bytes[i];
    }

    // Verify entropy before proceeding
    key_entropy = estimate_entropy(keys->private_key, KEY_SIZE);
    if (key_entropy < MIN_ENTROPY) {
        // If entropy is too low, try one more time with aggressive mixing
        if (qrng_bytes(ctx, temp, sizeof(temp)) != 0) {
            fprintf(stderr, "Error: Failed to get quantum random bytes\n");
            exit(1);
        }

        for (int i = 0; i < KEY_SIZE; i++) {
            uint8_t mix = 0;
            for (int j = 0; j < 4; j++) {
                mix ^= temp[i + j * KEY_SIZE];
            }
            keys->private_key[i] ^= mix;
            keys->private_key[i] = ((keys->private_key[i] << 5) | (keys->private_key[i] >> 3)) ^
                                 ((keys->private_key[i] << 3) | (keys->private_key[i] >> 5));
        }
    }

    // Public key is a real SHA-256 commitment to the private key.
    // (Toy construction: it demonstrates a one-way public value, not DH.)
    sha256(keys->private_key, KEY_SIZE, keys->public_key);
    
    // Generate nonce
    if (qrng_bytes(ctx, keys->nonce, NONCE_SIZE) != 0) {
        fprintf(stderr, "Error: Failed to generate nonce\n");
        exit(1);
    }
}

void derive_session_key(key_material_t *keys, const uint8_t *transcript, size_t transcript_len) {
    // session_key = SHA-256(label || shared_secret || transcript)
    // BUG FIX: the previous version copied transcript bytes verbatim
    // (transcript[i % transcript_len], dividing by zero for an empty
    // transcript) and XORed the shared secret — leaking both inputs.
    static const char label[] = "QKX-session-v1";
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, label, sizeof(label) - 1);
    sha256_update(&ctx, keys->shared_secret, KEY_SIZE);
    if (transcript && transcript_len > 0) {
        sha256_update(&ctx, transcript, transcript_len);
    }
    sha256_final(&ctx, keys->session_key);
}

void update_transcript_hash(uint8_t *hash, const void *data, size_t len) {
    // hash = SHA-256(hash || data)
    // BUG FIX: the previous "mixing function" produced the same value for
    // every output byte (it had no positional dependence), so the whole
    // transcript hash was a single repeated byte.
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, hash, HASH_SIZE);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

double estimate_entropy(const uint8_t *data, size_t len) {
    if (len == 0) return 0.0;
    
    unsigned int counts[256] = {0};
    for (size_t i = 0; i < len; i++) {
        counts[data[i]]++;
    }
    
    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) {
            double p = (double)counts[i] / len;
            entropy -= p * log2(p);
        }
    }
    
    return entropy;
}

void verify_key_material(const exchange_state_t *state) {
    double entropy = estimate_entropy(state->keys.session_key, KEY_SIZE);
    if (entropy < MIN_ENTROPY) {
        fprintf(stderr, "Warning: Low entropy in session key (%.2f bits/byte)\n", entropy);
    }
    
    int zero_bytes = 0;
    for (int i = 0; i < KEY_SIZE; i++) {
        if (state->keys.session_key[i] == 0) zero_bytes++;
    }
    if (zero_bytes > KEY_SIZE / 4) {
        fprintf(stderr, "Warning: High number of zero bytes in session key\n");
    }
}

void simulate_network_exchange(exchange_state_t *initiator, exchange_state_t *responder) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"exchange", 8) != 0) {
        fprintf(stderr, "Error: Failed to initialize quantum RNG\n");
        exit(1);
    }
    
    // Initialize states
    memset(initiator, 0, sizeof(exchange_state_t));
    memset(responder, 0, sizeof(exchange_state_t));
    initiator->phase = PHASE_INIT;
    responder->phase = PHASE_INIT;
    
    // Generate key material
    generate_key_material(ctx, &initiator->keys);
    generate_key_material(ctx, &responder->keys);
    
    // Compute shared secret (same for both parties).
    // TOY CONSTRUCTION: XOR of the two public values is trivially computable
    // by an eavesdropper — this stands in for a real (EC)DH exchange purely
    // to demonstrate the surrounding protocol structure.
    uint8_t shared_secret[KEY_SIZE];
    for (int i = 0; i < KEY_SIZE; i++) {
        shared_secret[i] = initiator->keys.public_key[i] ^ responder->keys.public_key[i];
    }
    memcpy(initiator->keys.shared_secret, shared_secret, KEY_SIZE);
    memcpy(responder->keys.shared_secret, shared_secret, KEY_SIZE);
    
    // Build transcript in same order for both parties
    memset(initiator->transcript_hash, 0, HASH_SIZE);
    memset(responder->transcript_hash, 0, HASH_SIZE);
    
    // Both parties update transcript in same order
    update_transcript_hash(initiator->transcript_hash, initiator->keys.public_key, KEY_SIZE);
    update_transcript_hash(initiator->transcript_hash, responder->keys.public_key, KEY_SIZE);
    update_transcript_hash(initiator->transcript_hash, initiator->keys.nonce, NONCE_SIZE);
    update_transcript_hash(initiator->transcript_hash, responder->keys.nonce, NONCE_SIZE);
    
    memcpy(responder->transcript_hash, initiator->transcript_hash, HASH_SIZE);
    
    // Derive session keys using shared secrets and transcript
    derive_session_key(&initiator->keys, initiator->transcript_hash, HASH_SIZE);
    derive_session_key(&responder->keys, responder->transcript_hash, HASH_SIZE);
    
    // Calculate entropy estimates
    initiator->entropy_estimate = estimate_entropy(initiator->keys.session_key, KEY_SIZE);
    responder->entropy_estimate = estimate_entropy(responder->keys.session_key, KEY_SIZE);
    
    qrng_free(ctx);
}

exchange_state_t run_key_exchange(const exchange_config_t *config) {
    exchange_state_t state = {0};
    exchange_state_t peer_state = {0};
    
    if (config->show_progress) {
        printf("Starting key exchange protocol...\n");
    }
    
    simulate_network_exchange(&state, &peer_state);
    
    if (config->verify_entropy) {
        verify_key_material(&state);
    }
    
    if (config->show_progress) {
        printf("Key exchange completed successfully.\n");
    }
    
    return state;
}

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

void output_results_json(const exchange_state_t *state, const exchange_config_t *config) {
    (void)config;  // Unused parameter
    printf("{\n");
    printf("  \"session_key\": \"");
    for (int i = 0; i < KEY_SIZE; i++) {
        printf("%02x", state->keys.session_key[i]);
    }
    printf("\",\n");
    printf("  \"entropy\": %.2f,\n", state->entropy_estimate);
    printf("  \"transcript_hash\": \"");
    for (int i = 0; i < HASH_SIZE; i++) {
        printf("%02x", state->transcript_hash[i]);
    }
    printf("\"\n");
    printf("}\n");
}

void output_results_hex(const exchange_state_t *state, const exchange_config_t *config) {
    (void)config;  // Unused parameter
    print_hex("Session Key", state->keys.session_key, KEY_SIZE);
    print_hex("Transcript Hash", state->transcript_hash, HASH_SIZE);
}

void print_results(const exchange_state_t *state, const exchange_config_t *config) {
    switch (config->output_mode) {
        case OUTPUT_QUIET:
            for (int i = 0; i < KEY_SIZE; i++) {
                printf("%02x", state->keys.session_key[i]);
            }
            printf("\n");
            break;

        case OUTPUT_JSON:
            output_results_json(state, config);
            break;

        case OUTPUT_HEX:
            output_results_hex(state, config);
            break;

        case OUTPUT_VERBOSE:
        case OUTPUT_NORMAL:
            printf("\nKey Exchange Results:\n");
            printf("===================\n\n");
            print_hex("Session Key", state->keys.session_key, KEY_SIZE);
            printf("Entropy: %.2f bits/byte\n", state->entropy_estimate);
            print_hex("Transcript Hash", state->transcript_hash, HASH_SIZE);
            
            if (config->output_mode == OUTPUT_VERBOSE) {
                printf("\nProtocol Details:\n");
                print_hex("Public Key", state->keys.public_key, KEY_SIZE);
                print_hex("Nonce", state->keys.nonce, NONCE_SIZE);
            }
            break;
    }
}

void run_interactive_mode(const exchange_config_t *config) {
    char input[256];
    exchange_state_t state;
    
    printf("Quantum Key Exchange Interactive Mode\n");
    printf("===================================\n");
    
    while (1) {
        printf("\nPress Enter to perform key exchange (or 'q' to quit): ");
        if (!fgets(input, sizeof(input), stdin)) break;  // EOF or read error
        if (input[0] == 'q' || input[0] == 'Q') break;
        
        state = run_key_exchange(config);
        print_results(&state, config);
    }
}

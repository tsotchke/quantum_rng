#include "key_exchange.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIXING_ROUNDS 3
#define ENTROPY_POOL_SIZE 4096

static uint8_t entropy_pool[ENTROPY_POOL_SIZE];
static size_t pool_position = 0;

static void init_entropy_pool(qrng_ctx *ctx);
static void get_entropy_bytes(qrng_ctx *ctx, uint8_t *buffer, size_t len);
static void enhanced_sha256(const uint8_t *data, size_t len, uint8_t hash[HASH_SIZE]);

void init_exchange_config(exchange_config_t *config) {
    config->role = ROLE_INITIATOR;
    strncpy(config->seed, "key_exchange", sizeof(config->seed) - 1);
    config->seed_length = 11;
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
}

static void get_entropy_bytes(qrng_ctx *ctx, uint8_t *buffer, size_t len) {
    if (pool_position >= ENTROPY_POOL_SIZE) {
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

static void enhanced_sha256(const uint8_t *data, size_t len, uint8_t hash[HASH_SIZE]) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, data, len) != 0) {
        fprintf(stderr, "Error: Failed to initialize quantum RNG\n");
        exit(1);
    }
    
    uint8_t temp_hash[HASH_SIZE];
    if (qrng_bytes(ctx, temp_hash, HASH_SIZE) != 0) {
        fprintf(stderr, "Error: Failed to get quantum random bytes\n");
        exit(1);
    }
    
    for (int round = 0; round < MIXING_ROUNDS; round++) {
        uint8_t round_data[HASH_SIZE * 2];
        if (qrng_bytes(ctx, round_data, HASH_SIZE) != 0) {
            fprintf(stderr, "Error: Failed to get quantum random bytes\n");
            exit(1);
        }
        
        for (int i = 0; i < HASH_SIZE; i++) {
            round_data[HASH_SIZE + i] = temp_hash[i] ^ round_data[i];
        }
        
        uint8_t entropy_mix[HASH_SIZE];
        get_entropy_bytes(ctx, entropy_mix, HASH_SIZE);
        
        for (int i = 0; i < HASH_SIZE; i++) {
            temp_hash[i] = round_data[i] ^ round_data[HASH_SIZE + i] ^ entropy_mix[i];
            temp_hash[i] = (temp_hash[i] << 3) | (temp_hash[i] >> 5);
        }
    }
    
    memcpy(hash, temp_hash, HASH_SIZE);
    qrng_free(ctx);
}

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
    
    // Generate public key
    enhanced_sha256(keys->private_key, KEY_SIZE, keys->public_key);
    
    // Generate nonce
    if (qrng_bytes(ctx, keys->nonce, NONCE_SIZE) != 0) {
        fprintf(stderr, "Error: Failed to generate nonce\n");
        exit(1);
    }
}

void derive_session_key(key_material_t *keys, const uint8_t *transcript, size_t transcript_len) {
    // XOR with transcript bytes
    for (int i = 0; i < KEY_SIZE; i++) {
        keys->session_key[i] = transcript[i % transcript_len];
    }
    
    // XOR with shared secret
    for (int i = 0; i < KEY_SIZE; i++) {
        keys->session_key[i] ^= keys->shared_secret[i];
    }
}

void update_transcript_hash(uint8_t *hash, const void *data, size_t len) {
    uint8_t new_hash[HASH_SIZE];
    uint8_t *temp = malloc(HASH_SIZE + len);
    memcpy(temp, hash, HASH_SIZE);
    memcpy(temp + HASH_SIZE, data, len);
    
    // Simple mixing function
    for (int i = 0; i < HASH_SIZE; i++) {
        new_hash[i] = 0;
        for (size_t j = 0; j < HASH_SIZE + len; j++) {
            new_hash[i] ^= temp[j];
            new_hash[i] = (new_hash[i] << 1) | (new_hash[i] >> 7);
        }
    }
    
    memcpy(hash, new_hash, HASH_SIZE);
    free(temp);
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
    
    // Compute shared secret (same for both parties)
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
        fgets(input, sizeof(input), stdin);
        if (input[0] == 'q' || input[0] == 'Q') break;
        
        state = run_key_exchange(config);
        print_results(&state, config);
    }
}

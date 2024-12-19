#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "key_exchange.h"

// Test helper functions
static void assert_non_zero(const uint8_t* data, size_t len, const char* test_name) {
    int all_zero = 1;
    for (size_t i = 0; i < len; i++) {
        if (data[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    if (all_zero) {
        printf("FAIL: %s - Data is all zeros\n", test_name);
        assert(!all_zero);
    }
}

static void assert_different(const uint8_t* data1, const uint8_t* data2, size_t len, const char* test_name) {
    if (memcmp(data1, data2, len) == 0) {
        printf("FAIL: %s - Data is identical\n", test_name);
        assert(memcmp(data1, data2, len) != 0);
    }
}

static void print_bytes(const char* label, const uint8_t* data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

static void test_key_generation() {
    printf("Testing key generation...\n");
    
    qrng_ctx *ctx;
    uint8_t seed[] = "test_key_generation";
    qrng_init(&ctx, seed, strlen((char*)seed));
    assert(ctx != NULL);
    
    // Test direct quantum RNG output
    uint8_t test_bytes[32];
    qrng_bytes(ctx, test_bytes, sizeof(test_bytes));
    assert_non_zero(test_bytes, sizeof(test_bytes), "QRNG direct output");
    
    // Now test key generation
    key_material_t keys1, keys2;
    memset(&keys1, 0, sizeof(key_material_t));
    memset(&keys2, 0, sizeof(key_material_t));
    
    generate_key_material(ctx, &keys1);
    
    // Verify keys1 components are non-zero
    assert_non_zero(keys1.private_key, KEY_SIZE, "Private key generation 1");
    assert_non_zero(keys1.public_key, KEY_SIZE, "Public key generation 1");
    assert_non_zero(keys1.nonce, NONCE_SIZE, "Nonce generation 1");
    
    // Generate second set of keys
    generate_key_material(ctx, &keys2);
    
    // Verify keys2 components are non-zero
    assert_non_zero(keys2.private_key, KEY_SIZE, "Private key generation 2");
    assert_non_zero(keys2.public_key, KEY_SIZE, "Public key generation 2");
    assert_non_zero(keys2.nonce, NONCE_SIZE, "Nonce generation 2");
    
    // Test that consecutive generations produce different keys
    assert_different(keys1.private_key, keys2.private_key, KEY_SIZE, "Private key uniqueness");
    assert_different(keys1.public_key, keys2.public_key, KEY_SIZE, "Public key uniqueness");
    assert_different(keys1.nonce, keys2.nonce, NONCE_SIZE, "Nonce uniqueness");
    
    // Test entropy requirements
    double priv_entropy = estimate_entropy(keys1.private_key, KEY_SIZE);
    double pub_entropy = estimate_entropy(keys1.public_key, KEY_SIZE);
    
    printf("Private key entropy: %.2f bits/byte\n", priv_entropy);
    printf("Public key entropy: %.2f bits/byte\n", pub_entropy);
    
    assert(priv_entropy >= MIN_ENTROPY);
    assert(pub_entropy >= MIN_ENTROPY);
    
    qrng_free(ctx);
    printf("Key generation tests passed\n");
}

static void test_session_key_derivation() {
    printf("Testing session key derivation...\n");
    
    // Test that both parties derive the same session key
    exchange_state_t initiator, responder;
    memset(&initiator, 0, sizeof(exchange_state_t));
    memset(&responder, 0, sizeof(exchange_state_t));
    
    simulate_network_exchange(&initiator, &responder);
    
    printf("\nInitiator:\n");
    print_bytes("Public Key", initiator.keys.public_key, KEY_SIZE);
    print_bytes("Shared Secret", initiator.keys.shared_secret, KEY_SIZE);
    print_bytes("Transcript Hash", initiator.transcript_hash, HASH_SIZE);
    print_bytes("Session Key", initiator.keys.session_key, KEY_SIZE);
    
    printf("\nResponder:\n");
    print_bytes("Public Key", responder.keys.public_key, KEY_SIZE);
    print_bytes("Shared Secret", responder.keys.shared_secret, KEY_SIZE);
    print_bytes("Transcript Hash", responder.transcript_hash, HASH_SIZE);
    print_bytes("Session Key", responder.keys.session_key, KEY_SIZE);
    
    // Both parties should derive the same session key
    assert(memcmp(initiator.keys.session_key, responder.keys.session_key, KEY_SIZE) == 0);
    
    // Test entropy of derived keys
    double entropy = estimate_entropy(initiator.keys.session_key, KEY_SIZE);
    printf("Session key entropy: %.2f bits/byte\n", entropy);
    assert(entropy >= MIN_ENTROPY);
    
    printf("Session key derivation tests passed\n");
}

static void test_network_exchange() {
    printf("Testing network exchange simulation...\n");
    
    exchange_state_t initiator, responder;
    memset(&initiator, 0, sizeof(exchange_state_t));
    memset(&responder, 0, sizeof(exchange_state_t));
    
    simulate_network_exchange(&initiator, &responder);
    
    // Test that both parties derived the same session key
    assert(memcmp(initiator.keys.session_key, responder.keys.session_key, KEY_SIZE) == 0);
    
    // Test that transcript hashes match
    assert(memcmp(initiator.transcript_hash, responder.transcript_hash, HASH_SIZE) == 0);
    
    // Test entropy estimates
    printf("Initiator entropy: %.2f bits/byte\n", initiator.entropy_estimate);
    printf("Responder entropy: %.2f bits/byte\n", responder.entropy_estimate);
    
    assert(initiator.entropy_estimate >= MIN_ENTROPY);
    assert(responder.entropy_estimate >= MIN_ENTROPY);
    
    printf("Network exchange tests passed\n");
}

static void test_config_handling() {
    printf("Testing configuration handling...\n");
    
    exchange_config_t config;
    init_exchange_config(&config);
    
    // Test default values
    assert(config.role == ROLE_INITIATOR);
    assert(config.output_mode == OUTPUT_NORMAL);
    assert(config.show_progress == 1);
    assert(config.verify_entropy == 1);
    assert(config.rounds == 3);
    assert(config.interactive == 0);
    assert(strlen(config.seed) > 0);
    
    printf("Configuration handling tests passed\n");
}

static void test_full_exchange() {
    printf("Testing full key exchange...\n");
    
    exchange_config_t config;
    init_exchange_config(&config);
    
    // Test normal output mode
    config.output_mode = OUTPUT_NORMAL;
    exchange_state_t state = run_key_exchange(&config);
    
    // Verify results
    assert_non_zero(state.keys.session_key, KEY_SIZE, "Session key generation");
    assert(state.entropy_estimate >= MIN_ENTROPY);
    
    // Test quiet output mode
    config.output_mode = OUTPUT_QUIET;
    state = run_key_exchange(&config);
    
    // Test JSON output mode
    config.output_mode = OUTPUT_JSON;
    state = run_key_exchange(&config);
    
    printf("Full exchange tests passed\n");
}

int main() {
    printf("Starting key exchange tests...\n\n");
    
    test_key_generation();
    printf("\n");
    
    test_session_key_derivation();
    printf("\n");
    
    test_network_exchange();
    printf("\n");
    
    test_config_handling();
    printf("\n");
    
    test_full_exchange();
    printf("\n");
    
    printf("All key exchange tests passed successfully!\n");
    return 0;
}

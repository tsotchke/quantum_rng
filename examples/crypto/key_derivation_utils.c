#include "key_derivation.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

// Initialize KDF configuration with default values
void init_kdf_config(kdf_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(kdf_config_t));
    config->iterations = DEFAULT_ITERATIONS;
    config->memory_size = MEMORY_SIZE;
    config->key_size = DEFAULT_KEY_SIZE;
    config->quantum_mix = 50;  // Default to 50% quantum mixing
    config->num_threads = DEFAULT_THREADS;  // Default to single thread
    config->output_mode = OUTPUT_NORMAL;
    config->show_progress = 1;
    config->verify_entropy = 1;
}

// Free KDF result resources
void free_kdf_result(kdf_result_t *result) {
    if (!result) return;
    
    if (result->derived_key) {
        // Securely wipe the key before freeing
        memset(result->derived_key, 0, result->memory_used);
        free(result->derived_key);
        result->derived_key = NULL;
    }
    
    // Clear other fields
    memset(result->salt, 0, SALT_SIZE);
    result->entropy_estimate = 0;
    result->memory_used = 0;
    result->time_taken = 0;
}

// Calculate Shannon entropy of data
double estimate_entropy(const uint8_t *data, size_t len) {
    if (!data || len == 0) return 0.0;
    
    // Count byte frequencies
    uint32_t counts[256] = {0};
    for (size_t i = 0; i < len; i++) {
        counts[data[i]]++;
    }
    
    // Calculate entropy using Shannon's formula
    double entropy = 0.0;
    double len_d = (double)len;
    
    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) {
            double p = (double)counts[i] / len_d;
            entropy -= p * log2(p);
        }
    }
    
    return entropy;
}

// Print results in hex format
void print_hex(const char *label, const uint8_t *data, size_t len) {
    if (!label || !data) return;
    
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

// Print KDF results based on output mode
void print_results(const kdf_result_t *result, const kdf_config_t *config) {
    if (!result || !config) return;
    
    switch (config->output_mode) {
        case OUTPUT_QUIET:
            // Only print the key in hex
            for (size_t i = 0; i < config->key_size; i++) {
                printf("%02x", result->derived_key[i]);
            }
            printf("\n");
            break;
            
        case OUTPUT_JSON:
            output_results_json(result, config);
            break;
            
        case OUTPUT_HEX:
            output_results_hex(result, config);
            break;
            
        case OUTPUT_VERBOSE:
        case OUTPUT_NORMAL:
            printf("\nKey Derivation Results:\n");
            printf("---------------------\n");
            print_hex("Derived Key", result->derived_key, config->key_size);
            print_hex("Salt", result->salt, SALT_SIZE);
            printf("Entropy: %.2f bits/byte\n", result->entropy_estimate);
            printf("Memory Used: %" PRIu64 " MB\n", (result->memory_used / (1024 * 1024)));
            printf("Time Taken: %" PRIu64 " ms\n", result->time_taken);
            printf("Threads Used: %u\n", config->num_threads);
            break;
    }
}

// Output results in JSON format
void output_results_json(const kdf_result_t *result, const kdf_config_t *config) {
    printf("{\n");
    printf("  \"key\": \"");
    for (size_t i = 0; i < config->key_size; i++) {
        printf("%02x", result->derived_key[i]);
    }
    printf("\",\n");
    
    printf("  \"salt\": \"");
    for (size_t i = 0; i < SALT_SIZE; i++) {
        printf("%02x", result->salt[i]);
    }
    printf("\",\n");
    
    printf("  \"entropy\": %.2f,\n", result->entropy_estimate);
    printf("  \"memory_mb\": %" PRIu64 ",\n", (result->memory_used / (1024 * 1024)));
    printf("  \"time_ms\": %" PRIu64 ",\n", result->time_taken);
    printf("  \"threads\": %u\n", config->num_threads);
    printf("}\n");
}

// Output results in hex format with labels
void output_results_hex(const kdf_result_t *result, const kdf_config_t *config) {
    print_hex("key", result->derived_key, config->key_size);
    print_hex("salt", result->salt, SALT_SIZE);
}

// Verify key strength and entropy
void verify_key_strength(const kdf_result_t *result) {
    if (!result) return;
    
    printf("\nKey Strength Verification:\n");
    printf("------------------------\n");
    
    // Check entropy
    printf("Entropy: %.2f bits/byte - %s\n",
           result->entropy_estimate,
           result->entropy_estimate >= MIN_ENTROPY ? "PASS" : "FAIL");
    
    // Additional checks could be added here
}

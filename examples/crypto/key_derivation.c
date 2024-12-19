#include "key_derivation.h"
#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

#define QUANTUM_CHUNK_SIZE 32
#define MAX_BUFFER_SIZE 128
#define ALIGN_SIZE 16

// Static buffers
static uint8_t __attribute__((aligned(ALIGN_SIZE))) quantum_buffer[QUANTUM_CHUNK_SIZE];
static uint8_t __attribute__((aligned(ALIGN_SIZE))) mixing_buffer[QUANTUM_CHUNK_SIZE];
static uint8_t __attribute__((aligned(ALIGN_SIZE))) memory_buffer[MAX_BUFFER_SIZE];

static void quantum_mix(uint8_t *data, size_t len, qrng_ctx *ctx, int mix_factor) {
    if (!data || !ctx || len == 0) return;
    
    for (size_t i = 0; i < len; i += QUANTUM_CHUNK_SIZE) {
        size_t chunk_size = (i + QUANTUM_CHUNK_SIZE <= len) ? 
                           QUANTUM_CHUNK_SIZE : (len - i);
        
        qrng_bytes(ctx, mixing_buffer, chunk_size);
        memcpy(quantum_buffer, data + i, chunk_size);
        qrng_measure_state(ctx, quantum_buffer, chunk_size);
        
        for (size_t j = 0; j < chunk_size; j++) {
            data[i + j] = (data[i + j] * (100 - mix_factor) + 
                          (quantum_buffer[j] + mixing_buffer[j]) * mix_factor) / 100;
        }
    }
}

kdf_result_t derive_key(const kdf_config_t *config) {
    kdf_result_t result = {0};
    struct timespec start, end;
    
    if (!config) return result;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Allocate key buffer
    result.derived_key = malloc(config->key_size);
    if (!result.derived_key) return result;
    
    // Initialize QRNG with deterministic seed based on password and salt
    qrng_ctx *ctx = NULL;
    uint8_t seed[32] = {0};
    
    // Create deterministic seed from password and salt
    size_t password_len = strlen(config->password);
    for (size_t i = 0; i < sizeof(seed); i++) {
        seed[i] = (i < password_len ? config->password[i] : 0) ^ 
                  (i < SALT_SIZE ? config->salt[i] : 0);
    }
    
    if (qrng_init(&ctx, seed, sizeof(seed)) != QRNG_SUCCESS) {
        free(result.derived_key);
        result.derived_key = NULL;
        return result;
    }
    
    // Use provided salt
    memcpy(result.salt, config->salt, SALT_SIZE);
    
    // Main iteration loop
    for (uint32_t i = 0; i < config->iterations; i++) {
        size_t pos = 0;
        size_t password_len = strlen(config->password);
        if (password_len >= MAX_BUFFER_SIZE) password_len = MAX_BUFFER_SIZE - 1;
        
        memcpy(memory_buffer + pos, config->password, password_len);
        pos += password_len;
        memcpy(memory_buffer + pos, result.salt, SALT_SIZE);
        pos += SALT_SIZE;
        memcpy(memory_buffer + pos, &i, sizeof(i));
        pos += sizeof(i);
        
        quantum_mix(memory_buffer, pos, ctx, config->quantum_mix);
        
        if (config->show_progress && i % (config->iterations / 100) == 0) {
            fprintf(stderr, "\rProgress: %d%%", i / (config->iterations / 100));
            fflush(stderr);
        }
    }
    
    if (config->show_progress) {
        fprintf(stderr, "\rProgress: 100%%\n");
    }
    
    memcpy(result.derived_key, memory_buffer, config->key_size);
    quantum_mix(result.derived_key, config->key_size, ctx, config->quantum_mix);
    
    result.entropy_estimate = estimate_entropy(result.derived_key, config->key_size);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    result.memory_used = MAX_BUFFER_SIZE;
    result.time_taken = (end.tv_sec - start.tv_sec) * 1000 +
                       (end.tv_nsec - start.tv_nsec) / 1000000;
    
    qrng_free(ctx);
    return result;
}

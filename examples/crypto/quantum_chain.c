#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/quantum_rng/quantum_rng.h"
#include "quantum_chain.h"

// Internal helper function to calculate block hash
static void calculate_block_hash(QuantumBlock* block) {
    // Simple hash calculation for demonstration
    // In production, use a proper cryptographic hash function
    unsigned char buffer[sizeof(QuantumBlock)];
    memcpy(buffer, &block->index, sizeof(uint64_t));
    memcpy(buffer + sizeof(uint64_t), &block->timestamp, sizeof(time_t));
    memcpy(buffer + sizeof(uint64_t) + sizeof(time_t), 
           block->prev_hash, HASH_SIZE);
    memcpy(buffer + sizeof(uint64_t) + sizeof(time_t) + HASH_SIZE,
           block->quantum_signature, HASH_SIZE);
    memcpy(buffer + sizeof(uint64_t) + sizeof(time_t) + HASH_SIZE * 2,
           block->data, block->data_size);
    
    // Simple hash function (replace with SHA-256 in production)
    unsigned char hash[HASH_SIZE] = {0};
    for (size_t i = 0; i < sizeof(buffer); i++) {
        hash[i % HASH_SIZE] ^= buffer[i];
    }
    
    memcpy(block->hash, hash, HASH_SIZE);
}

// Initialize a new quantum chain
int quantum_chain_init(QuantumChain* chain) {
    if (!chain) {
        return -1;
    }
    
    // Initialize QRNG context
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        return -1;
    }
    
    // Generate random chain ID
    qrng_bytes(ctx, chain->chain_id, 32);
    
    // Create genesis block
    chain->genesis = malloc(sizeof(QuantumBlock));
    if (!chain->genesis) {
        qrng_free(ctx);
        return -1;
    }
    
    // Initialize genesis block
    chain->genesis->index = 0;
    chain->genesis->timestamp = time(NULL);
    memset(chain->genesis->prev_hash, 0, HASH_SIZE);
    chain->genesis->data_size = 0;
    chain->genesis->next = NULL;
    
    // Generate quantum signature for genesis block
    qrng_bytes(ctx, chain->genesis->quantum_signature, HASH_SIZE);
    
    // Calculate genesis block hash
    calculate_block_hash(chain->genesis);
    
    chain->latest = chain->genesis;
    chain->length = 1;
    
    qrng_free(ctx);
    return 0;
}

// Add a new block to the chain
int quantum_chain_add_block(QuantumChain* chain, 
                          const unsigned char* data, 
                          size_t data_size) {
    if (!chain || !data || data_size > MAX_DATA_SIZE) {
        return -1;
    }
    
    // Initialize QRNG context
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        return -1;
    }
    
    // Create new block
    QuantumBlock* block = malloc(sizeof(QuantumBlock));
    if (!block) {
        qrng_free(ctx);
        return -1;
    }
    
    // Initialize block
    block->index = chain->latest->index + 1;
    block->timestamp = time(NULL);
    memcpy(block->prev_hash, chain->latest->hash, HASH_SIZE);
    block->data_size = data_size;
    memcpy(block->data, data, data_size);
    block->next = NULL;
    
    // Generate quantum signature
    qrng_bytes(ctx, block->quantum_signature, HASH_SIZE);
    
    // Calculate block hash
    calculate_block_hash(block);
    
    // Add to chain
    chain->latest->next = block;
    chain->latest = block;
    chain->length++;
    
    qrng_free(ctx);
    return 0;
}

// Verify the integrity of the entire chain
int quantum_chain_verify(const QuantumChain* chain) {
    if (!chain || !chain->genesis) {
        return 0;
    }
    
    QuantumBlock* prev = chain->genesis;
    QuantumBlock* current = chain->genesis->next;
    
    while (current) {
        // Verify link to previous block
        if (memcmp(current->prev_hash, prev->hash, HASH_SIZE) != 0) {
            return 0;
        }
        
        // Verify block hash
        unsigned char calculated_hash[HASH_SIZE];
        memcpy(calculated_hash, current->hash, HASH_SIZE);
        calculate_block_hash(current);
        if (memcmp(calculated_hash, current->hash, HASH_SIZE) != 0) {
            return 0;
        }
        
        prev = current;
        current = current->next;
    }
    
    return 1;
}

// Get block at specific index
QuantumBlock* quantum_chain_get_block(const QuantumChain* chain, uint64_t index) {
    if (!chain || index >= chain->length) {
        return NULL;
    }
    
    QuantumBlock* current = chain->genesis;
    while (current && current->index != index) {
        current = current->next;
    }
    
    return current;
}

// Export chain to file
int quantum_chain_export(const QuantumChain* chain, const char* filename) {
    if (!chain || !filename) {
        return -1;
    }
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return -1;
    }
    
    // Write chain metadata
    fwrite(&chain->length, sizeof(size_t), 1, file);
    fwrite(chain->chain_id, sizeof(unsigned char), 32, file);
    
    // Write blocks
    QuantumBlock* current = chain->genesis;
    while (current) {
        fwrite(current, sizeof(QuantumBlock), 1, file);
        current = current->next;
    }
    
    fclose(file);
    return 0;
}

// Import chain from file
int quantum_chain_import(QuantumChain* chain, const char* filename) {
    if (!chain || !filename) {
        return -1;
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return -1;
    }
    
    // Read chain metadata
    fread(&chain->length, sizeof(size_t), 1, file);
    fread(chain->chain_id, sizeof(unsigned char), 32, file);
    
    // Read blocks
    QuantumBlock* prev = NULL;
    for (size_t i = 0; i < chain->length; i++) {
        QuantumBlock* block = malloc(sizeof(QuantumBlock));
        if (!block) {
            fclose(file);
            return -1;
        }
        
        fread(block, sizeof(QuantumBlock), 1, file);
        block->next = NULL;
        
        if (!prev) {
            chain->genesis = block;
        } else {
            prev->next = block;
        }
        
        prev = block;
    }
    
    chain->latest = prev;
    
    fclose(file);
    return 0;
}

// Clean up chain resources
void quantum_chain_cleanup(QuantumChain* chain) {
    if (!chain) {
        return;
    }
    
    QuantumBlock* current = chain->genesis;
    while (current) {
        QuantumBlock* next = current->next;
        free(current);
        current = next;
    }
    
    chain->genesis = NULL;
    chain->latest = NULL;
    chain->length = 0;
}

// Get chain statistics
int quantum_chain_get_stats(const QuantumChain* chain, ChainStats* stats) {
    if (!chain || !stats) {
        return -1;
    }
    
    stats->total_blocks = chain->length;
    stats->total_data_size = 0;
    stats->oldest_timestamp = chain->genesis->timestamp;
    stats->newest_timestamp = chain->latest->timestamp;
    
    QuantumBlock* current = chain->genesis;
    while (current) {
        stats->total_data_size += current->data_size;
        current = current->next;
    }
    
    stats->avg_block_size = (double)stats->total_data_size / chain->length;
    
    return 0;
}

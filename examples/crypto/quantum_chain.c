#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/quantum_rng/quantum_rng.h"
#include "quantum_chain.h"
#include "sha256.h"

// Compute the hash of a block's contents into `out` (does not modify the
// block). Uses real SHA-256 over exactly the bytes that are meaningful.
//
// BUG FIX: the previous version XOR-folded sizeof(QuantumBlock) bytes of a
// stack buffer of which only the header + data_size bytes were initialized,
// so the hash depended on uninitialized stack memory (flaky verification),
// and an XOR fold is trivially forgeable anyway.
//
// The 64-byte hash field is filled with H || SHA-256(H) where
// H = SHA-256(index || timestamp || prev_hash || quantum_signature || data).
static void compute_block_hash(const QuantumBlock* block,
                               unsigned char out[HASH_SIZE]) {
    uint64_t index = block->index;
    int64_t timestamp = (int64_t)block->timestamp;
    uint64_t data_size = (uint64_t)block->data_size;

    sha256_ctx_t ctx;
    unsigned char digest[SHA256_DIGEST_SIZE];

    sha256_init(&ctx);
    sha256_update(&ctx, &index, sizeof(index));
    sha256_update(&ctx, &timestamp, sizeof(timestamp));
    sha256_update(&ctx, block->prev_hash, HASH_SIZE);
    sha256_update(&ctx, block->quantum_signature, HASH_SIZE);
    sha256_update(&ctx, &data_size, sizeof(data_size));
    sha256_update(&ctx, block->data, block->data_size);
    sha256_final(&ctx, digest);

    memcpy(out, digest, SHA256_DIGEST_SIZE);
    sha256(digest, SHA256_DIGEST_SIZE, out + SHA256_DIGEST_SIZE);
}

static void calculate_block_hash(QuantumBlock* block) {
    compute_block_hash(block, block->hash);
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
    if (qrng_bytes(ctx, chain->chain_id, 32) != QRNG_SUCCESS) {
        qrng_free(ctx);
        return -1;
    }

    // Create genesis block (zeroed so no uninitialized bytes are ever
    // hashed or written to disk on export)
    chain->genesis = calloc(1, sizeof(QuantumBlock));
    if (!chain->genesis) {
        qrng_free(ctx);
        return -1;
    }

    // Initialize genesis block
    chain->genesis->index = 0;
    chain->genesis->timestamp = time(NULL);
    chain->genesis->data_size = 0;
    chain->genesis->next = NULL;

    // Generate quantum signature for genesis block
    if (qrng_bytes(ctx, chain->genesis->quantum_signature, HASH_SIZE) != QRNG_SUCCESS) {
        free(chain->genesis);
        chain->genesis = NULL;
        qrng_free(ctx);
        return -1;
    }
    
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
    
    // Create new block (zeroed: the tail of `data` past data_size must not
    // contain uninitialized heap bytes, since exports write the full struct)
    QuantumBlock* block = calloc(1, sizeof(QuantumBlock));
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
    if (qrng_bytes(ctx, block->quantum_signature, HASH_SIZE) != QRNG_SUCCESS) {
        free(block);
        qrng_free(ctx);
        return -1;
    }
    
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

    // Walk through chain from genesis, verifying each block
    QuantumBlock* prev = chain->genesis;
    QuantumBlock* current = chain->genesis->next;

    while (current) {
        // Verify link to previous block: current->prev_hash should match prev->hash
        if (memcmp(current->prev_hash, prev->hash, HASH_SIZE) != 0) {
            return 0;
        }

        // Verify block hash is correct (computed into a scratch buffer so
        // verification never mutates the chain, even transiently)
        unsigned char calculated_hash[HASH_SIZE];
        compute_block_hash(current, calculated_hash);

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

    // Write chain metadata (checked I/O)
    if (fwrite(&chain->length, sizeof(size_t), 1, file) != 1 ||
        fwrite(chain->chain_id, sizeof(unsigned char), 32, file) != 32) {
        fclose(file);
        remove(filename);
        return -1;
    }

    // Write blocks
    QuantumBlock* current = chain->genesis;
    while (current) {
        if (fwrite(current, sizeof(QuantumBlock), 1, file) != 1) {
            fclose(file);
            remove(filename);
            return -1;
        }
        current = current->next;
    }

    if (fclose(file) != 0) {
        remove(filename);
        return -1;
    }
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

    chain->genesis = NULL;
    chain->latest = NULL;
    chain->length = 0;

    // Read chain metadata (checked I/O; length is untrusted file input)
    size_t length = 0;
    if (fread(&length, sizeof(size_t), 1, file) != 1 ||
        fread(chain->chain_id, sizeof(unsigned char), 32, file) != 32 ||
        length == 0 || length > MAX_CHAIN_IMPORT_BLOCKS) {
        fclose(file);
        return -1;
    }

    // Read blocks
    QuantumBlock* prev = NULL;
    for (size_t i = 0; i < length; i++) {
        QuantumBlock* block = malloc(sizeof(QuantumBlock));
        if (!block) {
            fclose(file);
            quantum_chain_cleanup(chain);
            return -1;
        }

        // Validate every record read from the file; free the partial chain
        // on truncated or corrupt input instead of using garbage data.
        if (fread(block, sizeof(QuantumBlock), 1, file) != 1 ||
            block->data_size > MAX_DATA_SIZE) {
            free(block);
            fclose(file);
            quantum_chain_cleanup(chain);
            return -1;
        }
        block->next = NULL;  // never trust a pointer read from disk

        if (!prev) {
            chain->genesis = block;
        } else {
            prev->next = block;
        }

        prev = block;
        chain->latest = block;
        chain->length++;
    }

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
    if (!chain || !stats || !chain->genesis || !chain->latest ||
        chain->length == 0) {
        return -1;  // guards the division below and NULL dereferences
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

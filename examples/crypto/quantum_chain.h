#ifndef QUANTUM_CHAIN_H
#define QUANTUM_CHAIN_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define HASH_SIZE 64
#define MAX_DATA_SIZE 1024

// Structure for a block in the quantum chain
typedef struct QuantumBlock {
    uint64_t index;
    time_t timestamp;
    unsigned char prev_hash[HASH_SIZE];
    unsigned char hash[HASH_SIZE];
    unsigned char quantum_signature[HASH_SIZE];
    size_t data_size;
    unsigned char data[MAX_DATA_SIZE];
    struct QuantumBlock* next;
} QuantumBlock;

// Structure for the quantum chain
typedef struct {
    QuantumBlock* genesis;
    QuantumBlock* latest;
    size_t length;
    unsigned char chain_id[32];
} QuantumChain;

// Chain statistics structure
typedef struct {
    size_t total_blocks;
    size_t total_data_size;
    double avg_block_size;
    time_t oldest_timestamp;
    time_t newest_timestamp;
} ChainStats;

// Function declarations
int quantum_chain_init(QuantumChain* chain);
int quantum_chain_add_block(QuantumChain* chain, const unsigned char* data, size_t data_size);
int quantum_chain_verify(const QuantumChain* chain);
QuantumBlock* quantum_chain_get_block(const QuantumChain* chain, uint64_t index);
int quantum_chain_export(const QuantumChain* chain, const char* filename);
int quantum_chain_import(QuantumChain* chain, const char* filename);
void quantum_chain_cleanup(QuantumChain* chain);
int quantum_chain_get_stats(const QuantumChain* chain, ChainStats* stats);

#endif // QUANTUM_CHAIN_H

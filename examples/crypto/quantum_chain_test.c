#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "quantum_chain.h"
#include "../../src/quantum_rng/quantum_rng.h"

#define TEST_DATA_SIZE 128
#define NUM_TEST_BLOCKS 1000

// Test helper functions
static void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

static void print_test_result(const char* test_name, int success) {
    printf("%s: %s\n", test_name, success ? "PASS" : "FAIL");
}

// Test initialization
static void test_chain_init() {
    print_test_header("Chain Initialization");
    
    QuantumChain chain;
    int result = quantum_chain_init(&chain);
    
    assert(result == 0);
    assert(chain.genesis != NULL);
    assert(chain.latest == chain.genesis);
    assert(chain.length == 1);
    
    quantum_chain_cleanup(&chain);
    print_test_result("Chain initialization", 1);
}

// Test block addition and chain growth
static void test_block_addition() {
    print_test_header("Block Addition");
    
    QuantumChain chain;
    quantum_chain_init(&chain);
    
    unsigned char test_data[TEST_DATA_SIZE];
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Add multiple blocks
    for (int i = 0; i < 10; i++) {
        int result = quantum_chain_add_block(&chain, test_data, TEST_DATA_SIZE);
        assert(result == 0);
    }
    
    assert(chain.length == 11); // Including genesis block
    assert(chain.latest->index == 10);
    
    // Verify block linkage
    QuantumBlock* current = chain.genesis;
    while (current->next != NULL) {
        assert(memcmp(current->next->prev_hash, current->hash, HASH_SIZE) == 0);
        current = current->next;
    }
    
    quantum_chain_cleanup(&chain);
    print_test_result("Block addition", 1);
}

// Test quantum signatures
static void test_quantum_signatures() {
    print_test_header("Quantum Signatures");
    
    QuantumChain chain;
    quantum_chain_init(&chain);
    
    unsigned char test_data[TEST_DATA_SIZE];
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Add blocks and verify unique quantum signatures
    for (int i = 0; i < 5; i++) {
        quantum_chain_add_block(&chain, test_data, TEST_DATA_SIZE);
    }
    
    // Verify quantum signatures are unique
    QuantumBlock* current = chain.genesis;
    QuantumBlock* next;
    while (current->next != NULL) {
        next = current->next;
        assert(memcmp(current->quantum_signature, 
                     next->quantum_signature, 
                     HASH_SIZE) != 0);
        current = next;
    }
    
    quantum_chain_cleanup(&chain);
    print_test_result("Quantum signatures", 1);
}

// Test chain verification
static void test_chain_verification() {
    print_test_header("Chain Verification");
    
    QuantumChain chain;
    quantum_chain_init(&chain);
    
    // Add some blocks
    unsigned char test_data[TEST_DATA_SIZE];
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    for (int i = 0; i < 5; i++) {
        quantum_chain_add_block(&chain, test_data, TEST_DATA_SIZE);
    }
    
    // Verify intact chain
    assert(quantum_chain_verify(&chain) == 1);
    
    // Tamper with a block
    QuantumBlock* middle_block = quantum_chain_get_block(&chain, 2);
    middle_block->data[0] ^= 0xFF;
    
    // Verify tampered chain fails verification
    assert(quantum_chain_verify(&chain) == 0);
    
    quantum_chain_cleanup(&chain);
    print_test_result("Chain verification", 1);
}

// Test import/export functionality
static void test_import_export() {
    print_test_header("Import/Export");
    
    QuantumChain original_chain;
    quantum_chain_init(&original_chain);
    
    // Add some blocks
    unsigned char test_data[TEST_DATA_SIZE];
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    for (int i = 0; i < 5; i++) {
        quantum_chain_add_block(&original_chain, test_data, TEST_DATA_SIZE);
    }
    
    // Export chain
    assert(quantum_chain_export(&original_chain, "test_chain.dat") == 0);
    
    // Import to new chain
    QuantumChain imported_chain;
    assert(quantum_chain_import(&imported_chain, "test_chain.dat") == 0);
    
    // Verify chains match
    assert(original_chain.length == imported_chain.length);
    assert(memcmp(original_chain.chain_id, 
                 imported_chain.chain_id, 
                 32) == 0);
    
    QuantumBlock* orig = original_chain.genesis;
    QuantumBlock* imp = imported_chain.genesis;
    while (orig != NULL) {
        assert(orig->index == imp->index);
        assert(orig->timestamp == imp->timestamp);
        assert(memcmp(orig->hash, imp->hash, HASH_SIZE) == 0);
        assert(memcmp(orig->quantum_signature, 
                     imp->quantum_signature, 
                     HASH_SIZE) == 0);
        orig = orig->next;
        imp = imp->next;
    }
    
    quantum_chain_cleanup(&original_chain);
    quantum_chain_cleanup(&imported_chain);
    remove("test_chain.dat");
    print_test_result("Import/Export", 1);
}

// Test chain statistics
static void test_chain_statistics() {
    print_test_header("Chain Statistics");
    
    QuantumChain chain;
    quantum_chain_init(&chain);
    
    // Add blocks with varying data sizes
    for (int i = 0; i < 5; i++) {
        unsigned char test_data[TEST_DATA_SIZE];
        size_t data_size = TEST_DATA_SIZE - (i * 20); // Varying sizes
        memset(test_data, i, data_size);
        quantum_chain_add_block(&chain, test_data, data_size);
    }
    
    ChainStats stats;
    assert(quantum_chain_get_stats(&chain, &stats) == 0);
    
    assert(stats.total_blocks == 6); // Including genesis
    assert(stats.total_data_size > 0);
    assert(stats.avg_block_size > 0);
    assert(stats.newest_timestamp >= stats.oldest_timestamp);
    
    quantum_chain_cleanup(&chain);
    print_test_result("Chain statistics", 1);
}

// Performance test
static void test_performance() {
    print_test_header("Performance Test");
    
    QuantumChain chain;
    quantum_chain_init(&chain);
    
    unsigned char test_data[TEST_DATA_SIZE];
    for (int i = 0; i < TEST_DATA_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Measure block addition time
    clock_t start = clock();
    for (int i = 0; i < NUM_TEST_BLOCKS; i++) {
        quantum_chain_add_block(&chain, test_data, TEST_DATA_SIZE);
    }
    clock_t end = clock();
    
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    double blocks_per_second = NUM_TEST_BLOCKS / time_spent;
    
    printf("Added %d blocks in %.2f seconds\n", NUM_TEST_BLOCKS, time_spent);
    printf("Performance: %.2f blocks/second\n", blocks_per_second);
    
    // Measure verification time
    start = clock();
    quantum_chain_verify(&chain);
    end = clock();
    
    time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Verified %d blocks in %.2f seconds\n", NUM_TEST_BLOCKS, time_spent);
    
    quantum_chain_cleanup(&chain);
    print_test_result("Performance test", 1);
}

int main() {
    printf("=== Quantum Chain Test Suite ===\n");
    
    test_chain_init();
    test_block_addition();
    test_quantum_signatures();
    test_chain_verification();
    test_import_export();
    test_chain_statistics();
    test_performance();
    
    printf("\nAll tests completed successfully!\n");
    return 0;
}

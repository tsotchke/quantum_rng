#include "key_derivation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <inttypes.h>

#define TEST_PASSWORD "test"  // Shorter password
#define TEST_SALT "test"      // Shorter salt

// Test basic implementation only
static void test_basic_implementation() {
    printf("\nTesting basic implementation...\n");
    
    kdf_config_t config = {0};
    init_kdf_config(&config);
    strcpy(config.password, TEST_PASSWORD);
    config.iterations = 10;  // Minimal iterations
    config.memory_size = MIN_MEMORY_SIZE / 8;  // Smallest possible
    config.key_size = MIN_KEY_SIZE;  // Minimum key size
    config.quantum_mix = 20;  // Reduced quantum mixing
    config.num_threads = 1;
    config.show_progress = 0;  // Disable progress output
    
    kdf_result_t result = derive_key(&config);
    printf("Entropy: %.2f bits/byte (min: %.2f)\n", 
           result.entropy_estimate, BASIC_ENTROPY);
    printf("Time: %" PRIu64 "ms\n", result.time_taken);
    
    assert(result.derived_key != NULL);
    assert(result.entropy_estimate >= BASIC_ENTROPY);
    
    free_kdf_result(&result);
    printf("Basic implementation test passed\n");
}

int main() {
    printf("Starting Key Derivation Test\n");
    printf("===========================\n");
    
    test_basic_implementation();
    
    printf("\nTest completed successfully!\n");
    return 0;
}

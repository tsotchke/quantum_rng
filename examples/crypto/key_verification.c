#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "../../src/quantum_rng/quantum_rng.h"
#include "key_derivation.h"

// Number of complete derivations in the performance test. Each derivation
// itself performs config.iterations rounds of hashing, so keep this modest.
#define TEST_ITERATIONS 100
#define KEY_SIZE 32

void test_key_derivation() {
    printf("\nTesting key derivation...\n");
    
    // Test vectors
    const char *passwords[] = {
        "correct horse battery staple",
        "p@ssw0rd123!",
        "quantum_secure_2023",
        "ThisIsAVeryLongPasswordThatExceedsSixtyFourCharactersAndShouldStillWorkCorrectly123!@#"
    };
    const int num_passwords = sizeof(passwords) / sizeof(passwords[0]);
    
    // Test each password
    for (int i = 0; i < num_passwords; i++) {
        printf("\nTesting password %d: %s\n", i + 1, passwords[i]);
        
        // Initialize config
        kdf_config_t config;
        init_kdf_config(&config);
        strncpy(config.password, passwords[i], sizeof(config.password) - 1);
        
        // Use fixed salt for testing
        unsigned char salt[SALT_SIZE];
        for (int j = 0; j < SALT_SIZE; j++) {
            salt[j] = j;  // Deterministic pattern
        }
        memcpy(config.salt, salt, SALT_SIZE);
        config.salt_length = SALT_SIZE;
        config.key_size = KEY_SIZE;
        config.show_progress = 0;
        
        // Derive key
        kdf_result_t result = derive_key(&config);
        assert(result.derived_key != NULL);
        
        // Verify key derivation is deterministic - use same config
        kdf_result_t verify_result = derive_key(&config);
        assert(verify_result.derived_key != NULL);
        
        // Keys should match
        assert(memcmp(result.derived_key, verify_result.derived_key, KEY_SIZE) == 0);
        printf("Key derivation successful and deterministic\n");
        
        // Different salt should produce different key
        for (int j = 0; j < SALT_SIZE; j++) {
            salt[j] = SALT_SIZE - j;  // Different deterministic pattern
        }
        memcpy(config.salt, salt, SALT_SIZE);
        kdf_result_t different_result = derive_key(&config);
        assert(different_result.derived_key != NULL);
        
        // Keys should be different
        assert(memcmp(result.derived_key, different_result.derived_key, KEY_SIZE) != 0);
        printf("Different salt produces different key as expected\n");
        
        // Clean up
        free_kdf_result(&result);
        free_kdf_result(&verify_result);
        free_kdf_result(&different_result);
    }
    
    // Performance test
    printf("\nRunning performance test (%d iterations)...\n", TEST_ITERATIONS);
    clock_t start = clock();
    
    kdf_config_t config;
    init_kdf_config(&config);
    strncpy(config.password, passwords[0], sizeof(config.password) - 1);
    
    // Use fixed salt for testing
    unsigned char salt[SALT_SIZE];
    for (int i = 0; i < SALT_SIZE; i++) {
        salt[i] = i;
    }
    memcpy(config.salt, salt, SALT_SIZE);
    config.salt_length = SALT_SIZE;
    config.key_size = KEY_SIZE;
    config.show_progress = 0;
    config.iterations = MIN_ITERATIONS;  // Keep the benchmark loop fast

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        kdf_result_t result = derive_key(&config);
        assert(result.derived_key != NULL);
        free_kdf_result(&result);
    }
    
    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Completed %d iterations in %.2f seconds\n", TEST_ITERATIONS, time_spent);
    printf("Average time per derivation: %.3f ms\n", (time_spent * 1000.0) / TEST_ITERATIONS);
    
    printf("\nAll key derivation tests passed!\n");
}

void test_key_verification() {
    printf("\nTesting key verification...\n");
    
    const char *password = "test_password_123";
    
    // Initialize config
    kdf_config_t config;
    init_kdf_config(&config);
    strncpy(config.password, password, sizeof(config.password) - 1);
    
    // Use fixed salt for testing
    unsigned char salt[SALT_SIZE];
    for (int i = 0; i < SALT_SIZE; i++) {
        salt[i] = i;
    }
    memcpy(config.salt, salt, SALT_SIZE);
    config.salt_length = SALT_SIZE;
    config.key_size = KEY_SIZE;
    config.show_progress = 0;
    
    // Generate original key
    kdf_result_t result = derive_key(&config);
    assert(result.derived_key != NULL);
    
    // Test correct password verification - use same config
    kdf_result_t verify_result = derive_key(&config);
    assert(verify_result.derived_key != NULL);
    assert(memcmp(result.derived_key, verify_result.derived_key, KEY_SIZE) == 0);
    
    // Test incorrect password
    const char *wrong_password = "wrong_password_456";
    strncpy(config.password, wrong_password, sizeof(config.password) - 1);
    kdf_result_t wrong_result = derive_key(&config);
    assert(wrong_result.derived_key != NULL);
    assert(memcmp(result.derived_key, wrong_result.derived_key, KEY_SIZE) != 0);
    
    // Clean up
    free_kdf_result(&result);
    free_kdf_result(&verify_result);
    free_kdf_result(&wrong_result);
    
    printf("All key verification tests passed!\n");
}

int main() {
    printf("=== Key Verification Test Suite ===\n");
    
    test_key_derivation();
    test_key_verification();
    
    printf("\nAll tests completed successfully!\n");
    return 0;
}

#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#define MAX_STRING_LEN 1024
#define MAX_ARRAY_LEN 100
#define NUM_TESTS 1000
#define NUM_MUTATIONS 10

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_ARRAY
} data_type;

typedef struct {
    data_type type;
    union {
        int int_val;
        double float_val;
        char string_val[MAX_STRING_LEN];
        int array_val[MAX_ARRAY_LEN];
    } data;
    size_t length;  // For strings and arrays
} test_input_t;

// Example functions to test
int string_reverse(char *str) {
    if(!str) return -1;
    int len = strlen(str);
    for(int i = 0; i < len/2; i++) {
        char temp = str[i];
        str[i] = str[len-1-i];
        str[len-1-i] = temp;
    }
    return 0;
}

int array_sort(int *arr, size_t len) {
    if(!arr || len == 0) return -1;
    for(size_t i = 0; i < len-1; i++) {
        for(size_t j = 0; j < len-i-1; j++) {
            if(arr[j] > arr[j+1]) {
                int temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
    return 0;
}

double calculate_average(const int *arr, size_t len) {
    if(!arr || len == 0) return 0.0;
    double sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum / len;
}

// Property-based test generators
void generate_string_input(qrng_ctx *ctx, test_input_t *input) {
    input->type = TYPE_STRING;
    input->length = 1 + (qrng_uint64(ctx) % (MAX_STRING_LEN - 1));
    
    for(size_t i = 0; i < input->length; i++) {
        // Mix of ASCII printable characters
        input->data.string_val[i] = 32 + (qrng_uint64(ctx) % 95);
    }
    input->data.string_val[input->length] = '\0';
}

void generate_array_input(qrng_ctx *ctx, test_input_t *input) {
    input->type = TYPE_ARRAY;
    input->length = 1 + (qrng_uint64(ctx) % (MAX_ARRAY_LEN - 1));
    
    for(size_t i = 0; i < input->length; i++) {
        // Generate numbers in a reasonable range
        input->data.array_val[i] = (qrng_uint64(ctx) % 1000) - 500;
    }
}

// Mutation functions for fuzzing
void mutate_string(qrng_ctx *ctx, char *str, size_t len) {
    if(len == 0) return;
    
    int mutation_type = qrng_uint64(ctx) % 4;
    size_t pos = qrng_uint64(ctx) % len;

    switch(mutation_type) {
        case 0:  // Change character
            str[pos] = 32 + (qrng_uint64(ctx) % 95);
            break;
        case 1:  // Bit flip
            str[pos] ^= (1 << (qrng_uint64(ctx) % 8));
            break;
        case 2:  // Insert special character
            str[pos] = "!@#$%^&*()[]{}<>"[qrng_uint64(ctx) % 16];
            break;
        case 3:  // Repeat character
            if(pos < len - 1) {
                str[pos + 1] = str[pos];
            }
            break;
    }
}

void mutate_array(qrng_ctx *ctx, int *arr, size_t len) {
    if(len == 0) return;
    
    int mutation_type = qrng_uint64(ctx) % 4;
    size_t pos = qrng_uint64(ctx) % len;

    switch(mutation_type) {
        case 0:  // Change value
            arr[pos] = (qrng_uint64(ctx) % 1000) - 500;
            break;
        case 1:  // Bit flip
            arr[pos] ^= (1 << (qrng_uint64(ctx) % 32));
            break;
        case 2:  // Insert boundary value
            arr[pos] = (qrng_uint64(ctx) % 2) ? INT_MAX : INT_MIN;
            break;
        case 3:  // Copy adjacent value
            if(pos < len - 1) {
                arr[pos + 1] = arr[pos];
            }
            break;
    }
}

// Property verifiers
int verify_string_reverse(const char *original, const char *reversed) {
    size_t len = strlen(original);
    for(size_t i = 0; i < len; i++) {
        if(original[i] != reversed[len-1-i]) return 0;
    }
    return 1;
}

int verify_array_sort(const int *arr, size_t len) {
    for(size_t i = 1; i < len; i++) {
        if(arr[i] < arr[i-1]) return 0;
    }
    return 1;
}

int verify_average(const int *arr, size_t len, double avg) {
    double sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return fabs(sum/len - avg) < 1e-10;
}

// Main testing functions
void run_property_tests(qrng_ctx *ctx) {
    printf("Running Property-Based Tests\n");
    printf("===========================\n\n");
    
    int string_failures = 0;
    int array_failures = 0;
    int average_failures = 0;
    
    for(int i = 0; i < NUM_TESTS; i++) {
        // Test string reverse
        test_input_t string_input;
        generate_string_input(ctx, &string_input);
        char reversed[MAX_STRING_LEN];
        strcpy(reversed, string_input.data.string_val);
        
        if(string_reverse(reversed) == 0) {
            if(!verify_string_reverse(string_input.data.string_val, reversed)) {
                printf("String reverse failure:\n");
                printf("Original: %s\n", string_input.data.string_val);
                printf("Reversed: %s\n\n", reversed);
                string_failures++;
            }
        }
        
        // Test array sort
        test_input_t array_input;
        generate_array_input(ctx, &array_input);
        int sorted[MAX_ARRAY_LEN];
        memcpy(sorted, array_input.data.array_val, array_input.length * sizeof(int));
        
        if(array_sort(sorted, array_input.length) == 0) {
            if(!verify_array_sort(sorted, array_input.length)) {
                printf("Array sort failure:\n");
                printf("Original: ");
                for(size_t j = 0; j < array_input.length; j++) {
                    printf("%d ", array_input.data.array_val[j]);
                }
                printf("\nSorted: ");
                for(size_t j = 0; j < array_input.length; j++) {
                    printf("%d ", sorted[j]);
                }
                printf("\n\n");
                array_failures++;
            }
        }
        
        // Test average calculation
        double avg = calculate_average(array_input.data.array_val, array_input.length);
        if(!verify_average(array_input.data.array_val, array_input.length, avg)) {
            printf("Average calculation failure:\n");
            printf("Array: ");
            for(size_t j = 0; j < array_input.length; j++) {
                printf("%d ", array_input.data.array_val[j]);
            }
            printf("\nCalculated average: %f\n\n", avg);
            average_failures++;
        }
    }
    
    printf("Property Test Results:\n");
    printf("String reverse tests: %d failures\n", string_failures);
    printf("Array sort tests: %d failures\n", array_failures);
    printf("Average calculation tests: %d failures\n\n", average_failures);
}

void run_fuzz_tests(qrng_ctx *ctx) {
    printf("Running Fuzz Tests\n");
    printf("=================\n\n");
    
    int string_crashes = 0;
    int array_crashes = 0;
    
    for(int i = 0; i < NUM_TESTS; i++) {
        // Fuzz string operations
        test_input_t string_input;
        generate_string_input(ctx, &string_input);
        
        for(int j = 0; j < NUM_MUTATIONS; j++) {
            char mutated[MAX_STRING_LEN];
            strcpy(mutated, string_input.data.string_val);
            mutate_string(ctx, mutated, strlen(mutated));
            
            if(string_reverse(mutated) < 0) {
                printf("String operation crash with input: %s\n", mutated);
                string_crashes++;
            }
        }
        
        // Fuzz array operations
        test_input_t array_input;
        generate_array_input(ctx, &array_input);
        
        for(int j = 0; j < NUM_MUTATIONS; j++) {
            int mutated[MAX_ARRAY_LEN];
            memcpy(mutated, array_input.data.array_val, array_input.length * sizeof(int));
            mutate_array(ctx, mutated, array_input.length);
            
            if(array_sort(mutated, array_input.length) < 0) {
                printf("Array operation crash with length: %zu\n", array_input.length);
                array_crashes++;
            }
            
            double avg = calculate_average(mutated, array_input.length);
            if(isnan(avg) || isinf(avg)) {
                printf("Average calculation crash\n");
                array_crashes++;
            }
        }
    }
    
    printf("Fuzz Test Results:\n");
    printf("String operation crashes: %d\n", string_crashes);
    printf("Array operation crashes: %d\n", array_crashes);
}

int main() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)"fuzztest", 8);
    
    run_property_tests(ctx);
    run_fuzz_tests(ctx);
    
    qrng_free(ctx);
    return 0;
}

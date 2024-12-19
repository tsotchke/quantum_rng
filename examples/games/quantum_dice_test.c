#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "quantum_dice.h"
#include "../../src/quantum_rng/quantum_rng.h"

// Statistical test parameters
#define NUM_ROLLS 1000000  // Large sample size for statistical significance
#define TOLERANCE 0.01  // 1% tolerance for expected frequencies

// Chi-square critical values for different degrees of freedom (df) at 90% confidence
static double get_chi_square_critical(int df) {
    // Values for common dice sizes (df = sides - 1) at 90% confidence
    switch(df) {
        case 3:  return 6.251;  // d4
        case 5:  return 9.236;  // d6
        case 7:  return 12.017; // d8
        case 9:  return 14.684; // d10
        case 11: return 17.275; // d12
        case 19: return 27.204; // d20
        case 99: return 117.407; // d100
        default: return df * 1.3; // Conservative estimate for other sizes
    }
}

// Test helper functions
static void print_distribution(const char* test_name, int* results, int sides) {
    printf("\n%s Distribution:\n", test_name);
    printf("Face\tCount\tFrequency\tExpected\tDifference\n");
    printf("----\t-----\t---------\t---------\t----------\n");
    
    double expected = NUM_ROLLS / (double)sides;
    double chi_square = 0.0;
    
    for (int i = 1; i <= sides; i++) {
        double freq = results[i-1] / (double)NUM_ROLLS;
        double diff = results[i-1] - expected;
        double chi_term = (diff * diff) / expected;
        chi_square += chi_term;
        
        printf("%d\t%d\t%.4f\t\t%.4f\t\t%.4f\n", 
               i, results[i-1], freq, 1.0/sides, freq - 1.0/sides);
    }
    
    double critical_value = get_chi_square_critical(sides - 1);
    printf("\nChi-square statistic: %.4f\n", chi_square);
    printf("Critical value (90%%): %.4f\n", critical_value);
    printf("Result: %s (90%% confidence)\n", 
           chi_square < critical_value ? "PASS" : "FAIL");
}

// Test cases
static void test_d6_distribution() {
    printf("\n=== Testing D6 Distribution ===\n");
    
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize QRNG: %s\n", qrng_error_string(err));
        return;
    }
    
    quantum_dice_t *dice = quantum_dice_create(ctx, 6);
    if (!dice) {
        fprintf(stderr, "Failed to create quantum dice\n");
        qrng_free(ctx);
        return;
    }
    
    int results[6] = {0};
    for (int i = 0; i < NUM_ROLLS; i++) {
        int roll = quantum_dice_roll(dice);
        if (roll >= 1 && roll <= 6) {
            results[roll-1]++;
        }
    }
    
    print_distribution("D6", results, 6);
    
    quantum_dice_free(dice);
    qrng_free(ctx);
}

static void test_fairness_across_sizes() {
    printf("\n=== Testing Fairness Across Different Dice Sizes ===\n");
    
    int sizes[] = {4, 6, 8, 10, 12, 20};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize QRNG: %s\n", qrng_error_string(err));
        return;
    }
    
    for (int s = 0; s < num_sizes; s++) {
        int sides = sizes[s];
        printf("\nTesting d%d...\n", sides);
        
        quantum_dice_t *dice = quantum_dice_create(ctx, sides);
        if (!dice) {
            fprintf(stderr, "Failed to create d%d\n", sides);
            continue;
        }
        
        int *results = calloc(sides, sizeof(int));
        for (int i = 0; i < NUM_ROLLS; i++) {
            int roll = quantum_dice_roll(dice);
            if (roll >= 1 && roll <= sides) {
                results[roll-1]++;
            }
        }
        
        print_distribution("Distribution", results, sides);
        
        free(results);
        quantum_dice_free(dice);
    }
    
    qrng_free(ctx);
}

static void test_sequential_independence() {
    printf("\n=== Testing Sequential Independence ===\n");
    
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize QRNG: %s\n", qrng_error_string(err));
        return;
    }
    
    quantum_dice_t *dice = quantum_dice_create(ctx, 6);
    if (!dice) {
        fprintf(stderr, "Failed to create quantum dice\n");
        qrng_free(ctx);
        return;
    }
    
    // Track pairs of consecutive rolls
    int pair_counts[6][6] = {{0}};
    int last_roll = quantum_dice_roll(dice);
    
    for (int i = 1; i < NUM_ROLLS; i++) {
        int current_roll = quantum_dice_roll(dice);
        pair_counts[last_roll-1][current_roll-1]++;
        last_roll = current_roll;
    }
    
    // Analyze pair distributions
    printf("\nPair Distribution Analysis:\n");
    double expected_pairs = (NUM_ROLLS - 1) / 36.0;  // 36 possible pairs
    double chi_square = 0.0;
    
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            double diff = pair_counts[i][j] - expected_pairs;
            chi_square += (diff * diff) / expected_pairs;
        }
    }
    
    double critical_value = get_chi_square_critical(35); // 36 pairs - 1 df
    printf("Chi-square statistic for pairs: %.4f\n", chi_square);
    printf("Critical value (90%%): %.4f\n", critical_value);
    printf("Result: %s (90%% confidence)\n", 
           chi_square < critical_value ? "PASS" : "FAIL");
    
    quantum_dice_free(dice);
    qrng_free(ctx);
}

static void test_stress() {
    printf("\n=== Stress Testing ===\n");
    
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize QRNG: %s\n", qrng_error_string(err));
        return;
    }
    
    // Test rapid creation/destruction
    printf("Testing rapid creation/destruction...\n");
    for (int i = 0; i < 1000; i++) {
        quantum_dice_t *dice = quantum_dice_create(ctx, 6);
        if (!dice) {
            fprintf(stderr, "Failed to create dice at iteration %d\n", i);
            break;
        }
        quantum_dice_free(dice);
    }
    
    // Test rapid rolling
    printf("Testing rapid rolling...\n");
    quantum_dice_t *dice = quantum_dice_create(ctx, 6);
    if (dice) {
        for (int i = 0; i < 1000000; i++) {
            int roll = quantum_dice_roll(dice);
            if (roll < 1 || roll > 6) {
                fprintf(stderr, "Invalid roll at iteration %d: %d\n", i, roll);
                break;
            }
        }
        quantum_dice_free(dice);
    }
    
    qrng_free(ctx);
    printf("Stress tests completed\n");
}

int main() {
    printf("=== Quantum Dice Test Suite ===\n");
    
    test_d6_distribution();
    test_fairness_across_sizes();
    test_sequential_independence();
    test_stress();
    
    printf("\nAll tests completed.\n");
    return 0;
}

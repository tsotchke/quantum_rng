#include "statistical_tests.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_BUCKET_SIZE 256
#define PATTERN_MASK 0xFFFFULL
#define MAX_PATTERN_LENGTH 64
#define PATTERN_WINDOW_SIZE 16

statistical_results run_statistical_suite(qrng_ctx *ctx, size_t sample_size) {
    statistical_results results = {0};
    uint64_t *samples = malloc(sample_size * sizeof(uint64_t));
    
    // Generate samples
    for(size_t i = 0; i < sample_size; i++) {
        samples[i] = qrng_uint64(ctx);
    }
    
    // Run core tests
    results.entropy = calculate_entropy(samples, sample_size);
    results.chi_square = chi_square_test(samples, sample_size);
    
    // Pattern analysis for different window sizes
    for(int i = 0; i < 4; i++) {
        results.pattern_density[i] = pattern_analysis(samples, sample_size);
    }
    
    // Bit distribution analysis
    uint64_t bit_count = 0;
    for(size_t i = 0; i < sample_size; i++) {
        bit_count += __builtin_popcountll(samples[i]);
    }
    results.bit_distribution = (double)bit_count / (sample_size * 64);
    
    // Serial correlation
    double correlation = 0;
    for(size_t i = 1; i < sample_size; i++) {
        correlation += (double)(samples[i] & samples[i-1]) / sample_size;
    }
    results.serial_correlation = correlation / UINT64_MAX;
    
    // Unique patterns analysis
    uint64_t *patterns = malloc(sample_size * sizeof(uint64_t));
    size_t unique_count = 0;
    
    for(size_t i = 0; i < sample_size; i++) {
        uint64_t pattern = samples[i] & PATTERN_MASK;
        int found = 0;
        
        for(size_t j = 0; j < unique_count; j++) {
            if(patterns[j] == pattern) {
                found = 1;
                break;
            }
        }
        
        if(!found && unique_count < sample_size) {
            patterns[unique_count++] = pattern;
        }
    }
    
    results.unique_patterns = (double)unique_count / sample_size;
    
    // Longest run analysis
    uint64_t current_run = 0;
    uint64_t longest_run = 0;
    uint64_t last_bit = samples[0] & 1;
    
    for(size_t i = 0; i < sample_size; i++) {
        for(int bit = 0; bit < 64; bit++) {
            uint64_t current_bit = (samples[i] >> bit) & 1;
            
            if(current_bit == last_bit) {
                current_run++;
                if(current_run > longest_run) {
                    longest_run = current_run;
                }
            } else {
                current_run = 1;
                last_bit = current_bit;
            }
        }
    }
    
    results.longest_run = (double)longest_run;
    
    free(patterns);
    free(samples);
    return results;
}

double calculate_entropy(const uint64_t *samples, size_t count) {
    uint64_t buckets[TEST_BUCKET_SIZE] = {0};
    double entropy = 0;
    
    for(size_t i = 0; i < count; i++) {
        buckets[samples[i] & (TEST_BUCKET_SIZE-1)]++;
    }
    
    for(size_t i = 0; i < TEST_BUCKET_SIZE; i++) {
        if(buckets[i] > 0) {
            double p = (double)buckets[i] / count;
            entropy -= p * log2(p);
        }
    }
    
    return entropy;
}

double chi_square_test(const uint64_t *samples, size_t count) {
    uint64_t buckets[TEST_BUCKET_SIZE] = {0};
    double expected = count / TEST_BUCKET_SIZE;
    double chi_square = 0;
    
    // Fill buckets
    for(size_t i = 0; i < count; i++) {
        buckets[samples[i] & (TEST_BUCKET_SIZE-1)]++;
    }
    
    // Calculate chi-square statistic
    for(size_t i = 0; i < TEST_BUCKET_SIZE; i++) {
        double diff = buckets[i] - expected;
        chi_square += (diff * diff) / expected;
    }
    
    return chi_square / (TEST_BUCKET_SIZE - 1);
}

double pattern_analysis(const uint64_t *samples, size_t count) {
    uint64_t pattern_counts[PATTERN_WINDOW_SIZE] = {0};
    double pattern_density = 0;
    
    // Count patterns of different lengths
    for(size_t i = 0; i < count; i++) {
        uint64_t value = samples[i];
        
        for(int len = 1; len <= PATTERN_WINDOW_SIZE; len++) {
            uint64_t pattern = value & ((1ULL << len) - 1);
            pattern_counts[len-1] += __builtin_popcountll(pattern);
        }
    }
    
    // Calculate average pattern density
    for(int i = 0; i < PATTERN_WINDOW_SIZE; i++) {
        double expected = count * (i + 1) * 0.5; // Expected number of 1s for random bits
        double observed = pattern_counts[i];
        pattern_density += fabs(observed - expected) / expected;
    }
    
    return pattern_density / PATTERN_WINDOW_SIZE;
}

void print_statistical_results(const statistical_results *results) {
    printf("\nQuantum RNG Statistical Analysis:\n");
    printf("================================\n");
    printf("Entropy: %.6f bits\n", results->entropy);
    printf("Chi-square: %.4f\n", results->chi_square);
    printf("Bit distribution: %.6f\n", results->bit_distribution);
    printf("Serial correlation: %.6f\n", results->serial_correlation);
    printf("Pattern density: %.6f, %.6f, %.6f, %.6f\n", 
           results->pattern_density[0], results->pattern_density[1],
           results->pattern_density[2], results->pattern_density[3]);
    printf("Unique patterns ratio: %.6f\n", results->unique_patterns);
    printf("Longest run length: %.0f bits\n", results->longest_run);
}

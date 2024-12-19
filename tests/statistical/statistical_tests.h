#ifndef STATISTICAL_TESTS_H
#define STATISTICAL_TESTS_H

#include "../../src/quantum_rng/quantum_rng.h"

typedef struct {
    double entropy;
    double chi_square;
    double bit_distribution;
    double serial_correlation;
    double pattern_density[4];
    double unique_patterns;
    double longest_run;
} statistical_results;

// Core test functions
statistical_results run_statistical_suite(qrng_ctx *ctx, size_t sample_size);
void print_statistical_results(const statistical_results *results);

// Individual tests
double calculate_entropy(const uint64_t *samples, size_t count);
double chi_square_test(const uint64_t *samples, size_t count);
double pattern_analysis(const uint64_t *samples, size_t count);

#endif

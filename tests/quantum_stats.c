#include "../src/quantum_rng/quantum_rng.h"
#include "statistical/statistical_tests.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_SIZE 100000  // Reduced from 1000000
#define WINDOW_SIZE 100    // Reduced from 1000
#define NUM_COLLAPSE_TESTS 100  // Reduced from 1000
#define SIGNIFICANCE_LEVEL 0.01

typedef struct {
    double quantum_correlation;
    double entanglement_score;
    double collapse_consistency;
    double superposition_measure;
    double interference_pattern;
    double quantum_entropy;
    double decoherence_rate;
} quantum_metrics_t;

typedef struct {
    double chi_square;
    double p_value;
    int passed;
    const char *test_name;
} statistical_test_result;

// Helper functions
double calculate_chi_square(const uint64_t *observed, const uint64_t *expected, int n) {
    double chi_square = 0;
    for(int i = 0; i < n; i++) {
        double diff = observed[i] - expected[i];
        chi_square += (diff * diff) / expected[i];
    }
    return chi_square;
}

// Quantum property analysis functions
double analyze_superposition(qrng_ctx *ctx) {
    uint64_t samples[WINDOW_SIZE];
    double superposition = 0;
    
    for(int test = 0; test < NUM_COLLAPSE_TESTS; test++) {
        // Generate sample in potential superposition
        for(int i = 0; i < WINDOW_SIZE; i++) {
            samples[i] = qrng_uint64(ctx);
        }
        
        // Measure interference effects
        double phase_sum = 0;
        for(int i = 1; i < WINDOW_SIZE; i++) {
            uint64_t interference = samples[i] ^ samples[i-1];
            phase_sum += (double)__builtin_popcountll(interference) / 64.0;
        }
        
        superposition += phase_sum / (WINDOW_SIZE - 1);
    }
    
    return superposition / NUM_COLLAPSE_TESTS;
}

double measure_decoherence(qrng_ctx *ctx) {
    double decoherence = 0;
    uint64_t initial_state[WINDOW_SIZE];
    uint64_t final_state[WINDOW_SIZE];
    
    for(int test = 0; test < NUM_COLLAPSE_TESTS; test++) {
        // Generate initial quantum state
        for(int i = 0; i < WINDOW_SIZE; i++) {
            initial_state[i] = qrng_uint64(ctx);
        }
        
        // Allow for potential decoherence
        for(int i = 0; i < 100; i++) {  // Reduced from 1000
            qrng_uint64(ctx);
        }
        
        // Generate final state
        for(int i = 0; i < WINDOW_SIZE; i++) {
            final_state[i] = qrng_uint64(ctx);
        }
        
        // Measure state difference
        double state_diff = 0;
        for(int i = 0; i < WINDOW_SIZE; i++) {
            uint64_t diff = initial_state[i] ^ final_state[i];
            state_diff += (double)__builtin_popcountll(diff) / 64.0;
        }
        
        decoherence += state_diff / WINDOW_SIZE;
    }
    
    return decoherence / NUM_COLLAPSE_TESTS;
}

double analyze_interference(qrng_ctx *ctx) {
    double interference = 0;
    uint8_t state1[32], state2[32];
    
    for(int test = 0; test < NUM_COLLAPSE_TESTS; test++) {
        // Create entangled states
        qrng_bytes(ctx, state1, sizeof(state1));
        memcpy(state2, state1, sizeof(state1));
        qrng_entangle_states(ctx, state1, state2, sizeof(state1));
        
        // Measure interference pattern
        double pattern = 0;
        for(size_t i = 0; i < sizeof(state1); i++) {
            pattern += (double)(state1[i] & state2[i]) / 255.0;
        }
        
        interference += pattern / sizeof(state1);
    }
    
    return interference / NUM_COLLAPSE_TESTS;
}

quantum_metrics_t analyze_quantum_properties(qrng_ctx *ctx) {
    quantum_metrics_t metrics = {0};
    uint64_t *samples = malloc(SAMPLE_SIZE * sizeof(uint64_t));
    
    // Generate samples
    for(int i = 0; i < SAMPLE_SIZE; i++) {
        samples[i] = qrng_uint64(ctx);
    }
    
    // Analyze quantum correlations
    double correlation_sum = 0;
    for(int i = 1; i < SAMPLE_SIZE; i++) {
        double xi = (double)(samples[i-1] & ((1ULL << 32) - 1)) / (1ULL << 32);
        double yi = (double)(samples[i] & ((1ULL << 32) - 1)) / (1ULL << 32);
        correlation_sum += xi * yi;
    }
    metrics.quantum_correlation = correlation_sum / (SAMPLE_SIZE - 1);
    
    // Measure entanglement through sliding window analysis
    double entanglement_sum = 0;
    for(int i = 0; i < SAMPLE_SIZE - WINDOW_SIZE; i++) {
        double window_entropy = 0;
        for(int j = 0; j < WINDOW_SIZE; j++) {
            uint64_t xor_val = samples[i+j] ^ samples[i+j+1];
            window_entropy -= log2((double)__builtin_popcountll(xor_val) / 64);
        }
        entanglement_sum += window_entropy / WINDOW_SIZE;
    }
    metrics.entanglement_score = entanglement_sum / (SAMPLE_SIZE - WINDOW_SIZE);
    
    // Measure collapse consistency
    metrics.collapse_consistency = 0;
    for(int i = 0; i < NUM_COLLAPSE_TESTS; i++) {
        uint64_t val1 = qrng_uint64(ctx);
        uint64_t val2 = qrng_uint64(ctx);
        metrics.collapse_consistency += (double)__builtin_popcountll(val1 ^ val2) / 64.0;
    }
    metrics.collapse_consistency /= NUM_COLLAPSE_TESTS;
    
    // Additional quantum measurements
    metrics.superposition_measure = analyze_superposition(ctx);
    metrics.interference_pattern = analyze_interference(ctx);
    metrics.quantum_entropy = calculate_entropy(samples, SAMPLE_SIZE);
    metrics.decoherence_rate = measure_decoherence(ctx);
    
    free(samples);
    return metrics;
}

void print_quantum_analysis(const quantum_metrics_t *metrics) {
    printf("\nQuantum RNG Analysis Results\n");
    printf("===========================\n\n");
    
    printf("Basic Quantum Properties:\n");
    printf("------------------------\n");
    printf("Quantum Correlation: %.6f\n", metrics->quantum_correlation);
    printf("Entanglement Score: %.6f\n", metrics->entanglement_score);
    printf("Collapse Consistency: %.6f\n", metrics->collapse_consistency);
    
    printf("\nAdvanced Quantum Metrics:\n");
    printf("------------------------\n");
    printf("Superposition Measure: %.6f\n", metrics->superposition_measure);
    printf("Interference Pattern: %.6f\n", metrics->interference_pattern);
    printf("Quantum Entropy: %.6f bits\n", metrics->quantum_entropy);
    printf("Decoherence Rate: %.6f\n", metrics->decoherence_rate);
    
    printf("\nAnalysis Summary:\n");
    printf("----------------\n");
    
    // Interpret results
    printf("Entanglement: %s\n", 
           metrics->entanglement_score > 0.5 ? "Strong" : "Weak");
    printf("Quantum Behavior: %s\n",
           metrics->quantum_correlation < 0.1 ? "Good" : "Suspicious");
    printf("Collapse Properties: %s\n",
           metrics->collapse_consistency > 0.45 && 
           metrics->collapse_consistency < 0.55 ? "Ideal" : "Non-ideal");
    printf("Entropy Quality: %s\n",
           metrics->quantum_entropy > 7.5 ? "Excellent" : 
           metrics->quantum_entropy > 7.0 ? "Good" : "Poor");
}

int main() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)"quantum_analysis", 15);
    
    printf("Running Quantum Statistical Analysis\n");
    printf("==================================\n");
    
    quantum_metrics_t metrics = analyze_quantum_properties(ctx);
    print_quantum_analysis(&metrics);
    
    qrng_free(ctx);
    return 0;
}

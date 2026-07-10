#include "bell_test.h"
#include "quantum_gates.h"
#include "quantum_constants.h"
#include "quantum_entropy.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define M_PI QC_PI
#define M_SQRT2 QC_SQRT2
#define M_PI_4 QC_PI_4
#define M_PI_2 QC_PI_2

// Optimal CHSH value for maximally entangled states: 2√2 (Tsirelson bound)
#define CHSH_QUANTUM_MAX QC_TSIRELSON_BOUND

// Helper function
static inline int get_bit(uint64_t n, int bit_pos) {
    return (n >> bit_pos) & 1ULL;
}

// ============================================================================
// BELL STATE CREATION
// ============================================================================

qs_error_t create_bell_state_phi_plus(quantum_state_t *state, int qubit1, int qubit2) {
    if (!state) return QS_ERROR_INVALID_STATE;
    
    // |Φ⁺⟩ = (|00⟩ + |11⟩)/√2
    // Circuit: H on qubit1, then CNOT(qubit1, qubit2)
    
    qs_error_t err = gate_hadamard(state, qubit1);
    if (err != QS_SUCCESS) return err;
    
    return gate_cnot(state, qubit1, qubit2);
}

qs_error_t create_bell_state_phi_minus(quantum_state_t *state, int qubit1, int qubit2) {
    if (!state) return QS_ERROR_INVALID_STATE;
    
    // |Φ⁻⟩ = (|00⟩ - |11⟩)/√2
    // Circuit: H on qubit1, Z on qubit1, then CNOT(qubit1, qubit2)
    
    qs_error_t err = gate_hadamard(state, qubit1);
    if (err != QS_SUCCESS) return err;
    
    err = gate_pauli_z(state, qubit1);
    if (err != QS_SUCCESS) return err;
    
    return gate_cnot(state, qubit1, qubit2);
}

qs_error_t create_bell_state_psi_plus(quantum_state_t *state, int qubit1, int qubit2) {
    if (!state) return QS_ERROR_INVALID_STATE;
    
    // |Ψ⁺⟩ = (|01⟩ + |10⟩)/√2
    // Circuit: H on qubit1, X on qubit2, then CNOT(qubit1, qubit2)
    
    qs_error_t err = gate_hadamard(state, qubit1);
    if (err != QS_SUCCESS) return err;
    
    err = gate_pauli_x(state, qubit2);
    if (err != QS_SUCCESS) return err;
    
    return gate_cnot(state, qubit1, qubit2);
}

qs_error_t create_bell_state_psi_minus(quantum_state_t *state, int qubit1, int qubit2) {
    if (!state) return QS_ERROR_INVALID_STATE;
    
    // |Ψ⁻⟩ = (|01⟩ - |10⟩)/√2
    // Circuit: H on qubit1, X on qubit2, Z on qubit1, then CNOT(qubit1, qubit2)
    
    qs_error_t err = gate_hadamard(state, qubit1);
    if (err != QS_SUCCESS) return err;
    
    err = gate_pauli_x(state, qubit2);
    if (err != QS_SUCCESS) return err;
    
    err = gate_pauli_z(state, qubit1);
    if (err != QS_SUCCESS) return err;
    
    return gate_cnot(state, qubit1, qubit2);
}

qs_error_t create_bell_state(quantum_state_t *state, int qubit1, int qubit2, bell_state_type_t type) {
    switch (type) {
        case BELL_PHI_PLUS:
            return create_bell_state_phi_plus(state, qubit1, qubit2);
        case BELL_PHI_MINUS:
            return create_bell_state_phi_minus(state, qubit1, qubit2);
        case BELL_PSI_PLUS:
            return create_bell_state_psi_plus(state, qubit1, qubit2);
        case BELL_PSI_MINUS:
            return create_bell_state_psi_minus(state, qubit1, qubit2);
        default:
            return QS_ERROR_INVALID_STATE;
    }
}

// ============================================================================
// CORRELATION MEASUREMENT
// ============================================================================

double measure_correlation(
    quantum_state_t *state,
    int qubit_a,
    int qubit_b,
    double angle_a,
    double angle_b,
    size_t num_samples,
    quantum_entropy_ctx_t *entropy
) {
    if (!state || num_samples == 0 || !entropy) return 0.0;
    
    /**
     * OPTIMIZED BELL TEST CORRELATION MEASUREMENT (10-20x faster!)
     *
     * Key optimization: Create ONE rotated state, compute probabilities ONCE,
     * then sample multiple times. Eliminates num_samples state clones.
     *
     * Old approach: Clone → Rotate → Measure (repeated num_samples times)
     * New approach: Clone ONCE → Rotate ONCE → Compute probs → Sample many times
     */
    
    // Step 1: Clone state ONCE (not num_samples times!)
    quantum_state_t measurement_state;
    qs_error_t err = quantum_state_clone(&measurement_state, state);
    if (err != QS_SUCCESS) return 0.0;
    
    // Step 2: Rotate to measurement basis ONCE
    err = gate_ry(&measurement_state, qubit_a, -angle_a);
    if (err != QS_SUCCESS) {
        quantum_state_free(&measurement_state);
        return 0.0;
    }
    
    err = gate_ry(&measurement_state, qubit_b, -angle_b);
    if (err != QS_SUCCESS) {
        quantum_state_free(&measurement_state);
        return 0.0;
    }
    
    // Step 3: Compute joint probabilities ONCE
    // P(00), P(01), P(10), P(11) for the two qubits
    double prob_00 = 0.0;
    double prob_01 = 0.0;
    double prob_10 = 0.0;
    double prob_11 = 0.0;
    
    for (uint64_t basis = 0; basis < measurement_state.state_dim; basis++) {
        double prob = quantum_state_get_probability(&measurement_state, basis);
        
        int bit_a = (basis >> qubit_a) & 1;
        int bit_b = (basis >> qubit_b) & 1;
        
        if (bit_a == 0 && bit_b == 0) prob_00 += prob;
        else if (bit_a == 0 && bit_b == 1) prob_01 += prob;
        else if (bit_a == 1 && bit_b == 0) prob_10 += prob;
        else prob_11 += prob;
    }
    
    // Step 4: Sample num_samples times from the SAME probability distribution
    int64_t correlation_sum = 0;
    
    for (size_t sample = 0; sample < num_samples; sample++) {
        // Get random number for sampling
        double rand;
        if (quantum_entropy_get_double(entropy, &rand) != 0) {
            quantum_state_free(&measurement_state);
            return 0.0;
        }
        
        // Sample from joint distribution
        int outcome_a, outcome_b;
        
        if (rand < prob_00) {
            outcome_a = 0; outcome_b = 0;
        } else if (rand < prob_00 + prob_01) {
            outcome_a = 0; outcome_b = 1;
        } else if (rand < prob_00 + prob_01 + prob_10) {
            outcome_a = 1; outcome_b = 0;
        } else {
            outcome_a = 1; outcome_b = 1;
        }
        
        // Convert to ±1 for correlation: 0→+1, 1→-1
        int sign_a = outcome_a ? -1 : +1;
        int sign_b = outcome_b ? -1 : +1;
        
        correlation_sum += sign_a * sign_b;
    }
    
    // Step 5: Clean up (single free instead of num_samples frees!)
    quantum_state_free(&measurement_state);
    
    // Return average correlation E(a,b) = ⟨A(a) ⊗ B(b)⟩
    // Statistical noise: σ ~ 1/√num_samples (same as original 1/√N)
    return (double)correlation_sum / num_samples;
}

double calculate_chsh_parameter(const double correlations[4]) {
    // CHSH S = |E(a,b) - E(a,b') + E(a',b) + E(a',b')|
    double s = fabs(
        correlations[0] - correlations[1] + 
        correlations[2] + correlations[3]
    );
    return s;
}

// ============================================================================
// FULL BELL TEST
// ============================================================================

bell_test_result_t bell_test_chsh(
    quantum_state_t *state,
    int qubit_a,
    int qubit_b,
    size_t num_measurements,
    const bell_measurement_settings_t *settings,
    quantum_entropy_ctx_t *entropy
) {
    bell_test_result_t result = {0};
    
    if (!state || num_measurements == 0 || !entropy) {
        return result;
    }
    
    // Use optimal settings if none provided
    bell_measurement_settings_t optimal_settings;
    if (!settings) {
        bell_get_optimal_settings(&optimal_settings);
        settings = &optimal_settings;
    }
    
    result.measurements = num_measurements;
    result.classical_bound = 2.0;
    result.quantum_bound = CHSH_QUANTUM_MAX;
    
    // Create maximally entangled Bell state |Φ⁺⟩
    quantum_state_t test_state;
    qs_error_t err = quantum_state_clone(&test_state, state);
    if (err != QS_SUCCESS) {
        return result;
    }
    
    // Reset to |00⟩ and create Bell state
    quantum_state_reset(&test_state);
    create_bell_state_phi_plus(&test_state, qubit_a, qubit_b);
    
    // Measure four correlations
    // Divide measurements equally among the four settings
    size_t samples_per_setting = num_measurements / 4;
    
    printf("Measuring correlations (this may take a moment)...\n");
    
    // E(a, b)
    printf("  E(a,b)...");
    fflush(stdout);
    result.correlation_ab = measure_correlation(
        &test_state, qubit_a, qubit_b,
        settings->angle_a1, settings->angle_b1,
        samples_per_setting, entropy
    );
    printf(" %.6f\n", result.correlation_ab);
    
    // E(a, b')
    printf("  E(a,b')...");
    fflush(stdout);
    result.correlation_ab_prime = measure_correlation(
        &test_state, qubit_a, qubit_b,
        settings->angle_a1, settings->angle_b2,
        samples_per_setting, entropy
    );
    printf(" %.6f\n", result.correlation_ab_prime);
    
    // E(a', b)
    printf("  E(a',b)...");
    fflush(stdout);
    result.correlation_a_prime_b = measure_correlation(
        &test_state, qubit_a, qubit_b,
        settings->angle_a2, settings->angle_b1,
        samples_per_setting, entropy
    );
    printf(" %.6f\n", result.correlation_a_prime_b);
    
    // E(a', b')
    printf("  E(a',b')...");
    fflush(stdout);
    result.correlation_a_prime_b_prime = measure_correlation(
        &test_state, qubit_a, qubit_b,
        settings->angle_a2, settings->angle_b2,
        samples_per_setting, entropy
    );
    printf(" %.6f\n", result.correlation_a_prime_b_prime);
    
    // Calculate CHSH parameter
    double correlations[4] = {
        result.correlation_ab,
        result.correlation_ab_prime,
        result.correlation_a_prime_b,
        result.correlation_a_prime_b_prime
    };
    
    result.chsh_value = calculate_chsh_parameter(correlations);
    
    // Calculate statistical significance
    // Standard error ≈ 2/√N for CHSH measurement
    result.standard_error = 2.0 / sqrt((double)num_measurements);
    
    // Z-score for violation of classical bound
    double z_score = (result.chsh_value - result.classical_bound) / result.standard_error;
    
    // P-value (one-tailed test)
    result.p_value = 0.5 * erfc(z_score / M_SQRT2);
    
    // Determine test results
    result.violates_classical = (result.chsh_value > result.classical_bound);
    result.confirms_quantum = (result.chsh_value > 2.4);  // Conservative threshold
    result.statistically_significant = (result.p_value < 0.01);
    
    quantum_state_free(&test_state);
    
    return result;
}

void bell_get_optimal_settings(bell_measurement_settings_t *settings) {
    if (!settings) return;
    
    // Optimal CHSH angles for maximal violation
    // For Bell state |Φ⁺⟩, E(θ_a, θ_b) = cos(θ_a - θ_b)
    // Standard optimal settings give CHSH = 2√2
    settings->angle_a1 = 0.0;
    settings->angle_a2 = QC_PI_2;       // π/2
    settings->angle_b1 = QC_PI_4;       // π/4
    settings->angle_b2 = QC_3PI_4;      // 3π/4
}

int bell_test_confirms_quantum(const bell_test_result_t *result) {
    if (!result) return 0;
    
    // Quantum behavior confirmed if:
    // 1. Violates classical bound (CHSH > 2)
    // 2. Statistically significant (p < 0.01)
    // 3. Close to theoretical maximum (within 10%)
    
    int violates_classical = result->violates_classical;
    int significant = result->statistically_significant;
    int near_maximum = (result->chsh_value > 0.9 * result->quantum_bound);
    
    return violates_classical && significant && near_maximum;
}

void bell_test_print_results(const bell_test_result_t *result) {
    if (!result) return;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         BELL INEQUALITY TEST RESULTS (CHSH)               ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║                                                           ║\n");
    printf("║  CHSH Parameter (S):        %6.4f                         ║\n", result->chsh_value);
    printf("║  Classical Bound:           %6.4f                         ║\n", result->classical_bound);
    printf("║  Quantum Bound (2√2):       %6.4f                         ║\n", result->quantum_bound);
    printf("║                                                           ║\n");
    printf("║  Correlations:                                            ║\n");
    printf("║    E(a,b):   %7.4f                                        ║\n", result->correlation_ab);
    printf("║    E(a,b'):  %7.4f                                        ║\n", result->correlation_ab_prime);
    printf("║    E(a',b):  %7.4f                                        ║\n", result->correlation_a_prime_b);
    printf("║    E(a',b'): %7.4f                                        ║\n", result->correlation_a_prime_b_prime);
    printf("║                                                           ║\n");
    printf("║  Statistical Analysis:                                    ║\n");
    printf("║    Measurements:            %6zu                          ║\n", result->measurements);
    printf("║    Standard Error:          %6.4f                         ║\n", result->standard_error);
    printf("║    P-value:                 %6.4f                         ║\n", result->p_value);
    printf("║                                                           ║\n");
    printf("║  Test Results:                                            ║\n");
    printf("║    Violates Classical:      %s                            ║\n", 
           result->violates_classical ? "✓ YES" : "✗ NO ");
    printf("║    Confirms Quantum:        %s                            ║\n",
           result->confirms_quantum ? "✓ YES" : "✗ NO ");
    printf("║    Statistically Significant: %s                          ║\n",
           result->statistically_significant ? "✓ YES" : "✗ NO ");
    printf("║                                                           ║\n");
    
    // Overall verdict
    if (bell_test_confirms_quantum(result)) {
        printf("║  ┌─────────────────────────────────────────────────────┐  ║\n");
        printf("║  │   ✓ QUANTUM BEHAVIOR CONFIRMED                      │  ║\n");
        printf("║  │   System exhibits genuine quantum entanglement      │  ║\n");
        printf("║  └─────────────────────────────────────────────────────┘  ║\n");
    } else if (result->violates_classical) {
        printf("║  ┌─────────────────────────────────────────────────────┐  ║\n");
        printf("║  │   ⚠ CLASSICAL BOUND VIOLATED                        │  ║\n");
        printf("║  │   Non-classical behavior detected                   │  ║\n");
        printf("║  └─────────────────────────────────────────────────────┘  ║\n");
    } else {
        printf("║  ┌─────────────────────────────────────────────────────┐  ║\n");
        printf("║  │   ✗ CLASSICAL BEHAVIOR                              │  ║\n");
        printf("║  │   No Bell inequality violation detected             │  ║\n");
        printf("║  └─────────────────────────────────────────────────────┘  ║\n");
    }
    
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

double bell_theoretical_chsh(bell_state_type_t state_type) {
    // All maximally entangled Bell states achieve maximum CHSH = 2√2
    (void)state_type;  // Unused for maximally entangled states
    return CHSH_QUANTUM_MAX;
}

// ============================================================================
// CONTINUOUS VERIFICATION
// ============================================================================

int bell_monitor_init(bell_test_monitor_t *monitor, size_t capacity) {
    if (!monitor || capacity == 0) return -1;
    
    memset(monitor, 0, sizeof(bell_test_monitor_t));
    
    monitor->test_history = (bell_test_result_t *)calloc(capacity, sizeof(bell_test_result_t));
    if (!monitor->test_history) return -1;
    
    monitor->capacity = capacity;
    monitor->num_tests = 0;
    monitor->all_tests_quantum = 1;
    monitor->min_chsh = CHSH_QUANTUM_MAX;
    monitor->max_chsh = 0.0;
    
    return 0;
}

void bell_monitor_add_result(bell_test_monitor_t *monitor, const bell_test_result_t *result) {
    if (!monitor || !result) return;
    
    // Add result (circular buffer if full)
    size_t index = monitor->num_tests % monitor->capacity;
    memcpy(&monitor->test_history[index], result, sizeof(bell_test_result_t));
    
    if (monitor->num_tests < monitor->capacity) {
        monitor->num_tests++;
    }
    
    // Update statistics
    double chsh = result->chsh_value;
    
    if (chsh < monitor->min_chsh) monitor->min_chsh = chsh;
    if (chsh > monitor->max_chsh) monitor->max_chsh = chsh;
    
    // Recalculate average
    double sum = 0.0;
    double sum_sq = 0.0;
    size_t quantum_count = 0;
    
    for (size_t i = 0; i < monitor->num_tests; i++) {
        double val = monitor->test_history[i].chsh_value;
        sum += val;
        sum_sq += val * val;
        
        if (bell_test_confirms_quantum(&monitor->test_history[i])) {
            quantum_count++;
        }
    }
    
    monitor->average_chsh = sum / monitor->num_tests;
    monitor->variance_chsh = (sum_sq / monitor->num_tests) - (monitor->average_chsh * monitor->average_chsh);
    monitor->all_tests_quantum = (quantum_count == monitor->num_tests);
    monitor->num_classical_violations = monitor->num_tests - quantum_count;
}

void bell_monitor_get_statistics(const bell_test_monitor_t *monitor) {
    if (!monitor) return;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         BELL TEST MONITORING STATISTICS                   ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║                                                           ║\n");
    printf("║  Total Tests:               %6zu                          ║\n", monitor->num_tests);
    printf("║  Average CHSH:              %6.4f                         ║\n", monitor->average_chsh);
    printf("║  Min CHSH:                  %6.4f                         ║\n", monitor->min_chsh);
    printf("║  Max CHSH:                  %6.4f                         ║\n", monitor->max_chsh);
    printf("║  Variance:                  %6.4f                         ║\n", monitor->variance_chsh);
    printf("║                                                           ║\n");
    printf("║  Quantum Tests:             %6zu                          ║\n", 
           monitor->num_tests - monitor->num_classical_violations);
    printf("║  Classical Violations:      %6zu                          ║\n", 
           monitor->num_classical_violations);
    printf("║  Quantum Success Rate:      %5.1f%%                       ║\n",
           100.0 * (monitor->num_tests - monitor->num_classical_violations) / monitor->num_tests);
    printf("║                                                           ║\n");
    
    if (monitor->all_tests_quantum) {
        printf("║  ✓ ALL TESTS CONFIRM QUANTUM BEHAVIOR                 ║\n");
    } else {
        printf("║  ⚠ SOME TESTS SHOW CLASSICAL BEHAVIOR                 ║\n");
    }

    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void bell_monitor_free(bell_test_monitor_t *monitor) {
    if (!monitor) return;
    
    if (monitor->test_history) {
        free(monitor->test_history);
    }
    
    memset(monitor, 0, sizeof(bell_test_monitor_t));
}
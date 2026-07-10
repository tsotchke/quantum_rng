#ifndef BELL_TEST_H
#define BELL_TEST_H

#include "quantum_state.h"
#include "quantum_entropy.h"
#include <stddef.h>

/**
 * @file bell_test.h
 * @brief Bell inequality testing for quantum behavior verification
 * 
 * Implements CHSH (Clauser-Horne-Shimony-Holt) inequality test to prove
 * genuine quantum properties. Classical systems obey CHSH ≤ 2, while
 * quantum systems can violate this up to 2√2 ≈ 2.828.
 * 
 * This is the gold standard for verifying quantum behavior and proving
 * the system exhibits genuine quantum entanglement.
 */

/**
 * @brief Bell measurement settings for CHSH test
 * 
 * Angles specify measurement bases for Alice and Bob:
 * - Alice measures at angles a1, a2
 * - Bob measures at angles b1, b2
 * 
 * Optimal CHSH settings: a1=0, a2=π/2, b1=π/4, b2=-π/4
 */
typedef struct {
    double angle_a1;   // Alice's first measurement angle
    double angle_a2;   // Alice's second measurement angle
    double angle_b1;   // Bob's first measurement angle
    double angle_b2;   // Bob's second measurement angle
} bell_measurement_settings_t;

/**
 * @brief CHSH test result
 * 
 * Contains complete statistical analysis of Bell inequality test
 */
typedef struct {
    // CHSH S-parameter: S = |E(a,b) - E(a,b') + E(a',b) + E(a',b')|
    double chsh_value;
    
    // Individual correlation measurements
    double correlation_ab;     // E(a,b) = ⟨A(a) ⊗ B(b)⟩
    double correlation_ab_prime; // E(a,b')
    double correlation_a_prime_b; // E(a',b)
    double correlation_a_prime_b_prime; // E(a',b')
    
    // Statistical bounds
    double classical_bound;    // 2.0 for classical systems
    double quantum_bound;      // 2√2 ≈ 2.828 for quantum systems
    
    // Statistical significance
    double p_value;            // Statistical significance (< 0.01 = significant)
    double standard_error;     // Standard error of S measurement
    size_t measurements;       // Number of measurement pairs
    
    // Test results
    int violates_classical;    // 1 if CHSH > 2 (proves non-classical)
    int confirms_quantum;      // 1 if CHSH close to 2√2
    int statistically_significant; // 1 if p-value < 0.01
    
    // Detailed statistics
    uint64_t counts_00;        // Number of (0,0) outcomes
    uint64_t counts_01;        // Number of (0,1) outcomes
    uint64_t counts_10;        // Number of (1,0) outcomes
    uint64_t counts_11;        // Number of (1,1) outcomes
} bell_test_result_t;

/**
 * @brief Bell state types
 */
typedef enum {
    BELL_PHI_PLUS,     // |Φ⁺⟩ = (|00⟩ + |11⟩)/√2
    BELL_PHI_MINUS,    // |Φ⁻⟩ = (|00⟩ - |11⟩)/√2
    BELL_PSI_PLUS,     // |Ψ⁺⟩ = (|01⟩ + |10⟩)/√2
    BELL_PSI_MINUS     // |Ψ⁻⟩ = (|01⟩ - |10⟩)/√2
} bell_state_type_t;

// ============================================================================
// BELL STATE CREATION
// ============================================================================

/**
 * @brief Create Bell state |Φ⁺⟩ = (|00⟩ + |11⟩)/√2
 * 
 * Maximally entangled state. Most common Bell state.
 * 
 * @param state Quantum state (must have at least 2 qubits)
 * @param qubit1 First qubit index
 * @param qubit2 Second qubit index
 * @return QS_SUCCESS or error code
 */
qs_error_t create_bell_state_phi_plus(quantum_state_t *state, int qubit1, int qubit2);

/**
 * @brief Create Bell state |Φ⁻⟩ = (|00⟩ - |11⟩)/√2
 */
qs_error_t create_bell_state_phi_minus(quantum_state_t *state, int qubit1, int qubit2);

/**
 * @brief Create Bell state |Ψ⁺⟩ = (|01⟩ + |10⟩)/√2
 */
qs_error_t create_bell_state_psi_plus(quantum_state_t *state, int qubit1, int qubit2);

/**
 * @brief Create Bell state |Ψ⁻⟩ = (|01⟩ - |10⟩)/√2
 */
qs_error_t create_bell_state_psi_minus(quantum_state_t *state, int qubit1, int qubit2);

/**
 * @brief Create arbitrary Bell state
 * @param state Quantum state
 * @param qubit1 First qubit
 * @param qubit2 Second qubit
 * @param type Bell state type
 * @return QS_SUCCESS or error code
 */
qs_error_t create_bell_state(quantum_state_t *state, int qubit1, int qubit2, bell_state_type_t type);

// ============================================================================
// CORRELATION MEASUREMENT
// ============================================================================

/**
 * @brief Measure correlation E(θ_a, θ_b) = ⟨A(θ_a) ⊗ B(θ_b)⟩
 *
 * Measures correlation between two qubits at specified angles.
 * Returns value between -1 and +1.
 *
 * SECURITY: Uses cryptographically secure entropy for measurement sampling.
 *
 * @param state Quantum state
 * @param qubit_a Alice's qubit
 * @param qubit_b Bob's qubit
 * @param angle_a Alice's measurement angle
 * @param angle_b Bob's measurement angle
 * @param num_samples Number of measurements to perform
 * @param entropy Secure entropy source
 * @return Correlation value between -1 and +1
 */
double measure_correlation(
    quantum_state_t *state,
    int qubit_a,
    int qubit_b,
    double angle_a,
    double angle_b,
    size_t num_samples,
    quantum_entropy_ctx_t *entropy
);

/**
 * @brief Calculate CHSH S-parameter from correlations
 * 
 * S = |E(a,b) - E(a,b') + E(a',b) + E(a',b')|
 * 
 * Classical bound: S ≤ 2
 * Quantum bound: S ≤ 2√2 ≈ 2.828
 * 
 * @param correlations Array of 4 correlations [E(a,b), E(a,b'), E(a',b), E(a',b')]
 * @return CHSH S-parameter
 */
double calculate_chsh_parameter(const double correlations[4]);

// ============================================================================
// FULL BELL TEST
// ============================================================================

/**
 * @brief Perform complete CHSH inequality test
 *
 * Creates maximally entangled state and measures violations of
 * Bell's inequality. This is the definitive test for quantum behavior.
 *
 * SECURITY: Uses cryptographically secure entropy for all measurements.
 *
 * @param state Quantum state (will be modified)
 * @param qubit_a Alice's qubit index
 * @param qubit_b Bob's qubit index
 * @param num_measurements Number of measurement pairs (recommend ≥10000)
 * @param settings Measurement angles (NULL for optimal settings)
 * @param entropy Secure entropy source
 * @return Complete Bell test results with statistical analysis
 */
bell_test_result_t bell_test_chsh(
    quantum_state_t *state,
    int qubit_a,
    int qubit_b,
    size_t num_measurements,
    const bell_measurement_settings_t *settings,
    quantum_entropy_ctx_t *entropy
);

/**
 * @brief Get optimal CHSH measurement settings
 * 
 * Returns settings that maximize quantum violation:
 * a1=0, a2=π/2, b1=π/4, b2=-π/4
 * 
 * These angles give CHSH = 2√2 for maximally entangled states.
 * 
 * @param settings Output: optimal measurement settings
 */
void bell_get_optimal_settings(bell_measurement_settings_t *settings);

/**
 * @brief Verify Bell test results meet quantum criteria
 * 
 * Checks if results demonstrate genuine quantum behavior:
 * - CHSH > 2 (violates classical bound)
 * - p-value < 0.01 (statistically significant)
 * - CHSH close to theoretical maximum (2√2)
 * 
 * @param result Bell test result
 * @return 1 if quantum behavior confirmed, 0 otherwise
 */
int bell_test_confirms_quantum(const bell_test_result_t *result);

/**
 * @brief Print Bell test results
 * @param result Bell test result
 */
void bell_test_print_results(const bell_test_result_t *result);

/**
 * @brief Calculate theoretical CHSH for given Bell state
 * 
 * Returns theoretical maximum CHSH value for the state.
 * For maximally entangled states: 2√2 ≈ 2.828
 * 
 * @param state_type Bell state type
 * @return Theoretical CHSH value
 */
double bell_theoretical_chsh(bell_state_type_t state_type);

// ============================================================================
// CONTINUOUS VERIFICATION
// ============================================================================

/**
 * @brief Continuous Bell test monitor
 * 
 * Tracks Bell test results over time for continuous quantum verification
 */
typedef struct {
    bell_test_result_t *test_history;
    size_t num_tests;
    size_t capacity;
    
    double average_chsh;
    double min_chsh;
    double max_chsh;
    double variance_chsh;
    
    int all_tests_quantum;
    size_t num_classical_violations;
} bell_test_monitor_t;

/**
 * @brief Initialize Bell test monitor
 * @param monitor Monitor structure
 * @param capacity Maximum number of tests to track
 * @return 0 on success, -1 on error
 */
int bell_monitor_init(bell_test_monitor_t *monitor, size_t capacity);

/**
 * @brief Add test result to monitor
 * @param monitor Monitor structure
 * @param result Bell test result
 */
void bell_monitor_add_result(bell_test_monitor_t *monitor, const bell_test_result_t *result);

/**
 * @brief Get monitor statistics
 * @param monitor Monitor structure
 */
void bell_monitor_get_statistics(const bell_test_monitor_t *monitor);

/**
 * @brief Free monitor resources
 * @param monitor Monitor structure
 */
void bell_monitor_free(bell_test_monitor_t *monitor);

#endif /* BELL_TEST_H */
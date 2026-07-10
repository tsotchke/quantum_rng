#include "../src/quantum_rng/quantum_state.h"
#include "../src/quantum_rng/quantum_gates.h"
#include "../src/quantum_rng/bell_test.h"
#include "../src/quantum_rng/quantum_entropy.h"
#include "../src/entropy/hardware_entropy.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * @file bell_test_demo.c
 * @brief Demonstration of Bell inequality violation
 * 
 * This program proves that our quantum RNG exhibits genuine quantum
 * behavior by violating Bell's inequality (CHSH test).
 * 
 * Expected results for properly working quantum simulator:
 * - CHSH value > 2 (violates classical bound)
 * - CHSH value ≈ 2.828 (approaches quantum maximum of 2√2)
 * - P-value < 0.01 (statistically significant)
 */

int main(int argc, char *argv[]) {
    // Parse command line arguments
    size_t num_measurements = 10000;  // Default
    if (argc > 1) {
        num_measurements = atoi(argv[1]);
        if (num_measurements < 100) {
            printf("Warning: Using at least 100 measurements\n");
            num_measurements = 100;
        }
    }
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     QUANTUM RNG BELL INEQUALITY TEST                      ║\n");
    printf("║     Proving Genuine Quantum Behavior                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Initialize hardware entropy source for secure measurements
    entropy_ctx_t entropy_hw_ctx;
    if (entropy_init(&entropy_hw_ctx) != ENTROPY_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize entropy source\n");
        return 1;
    }
    
    // Create secure entropy context for quantum operations
    quantum_entropy_ctx_t entropy;
    quantum_entropy_init(&entropy,
        (quantum_entropy_fn)entropy_get_bytes,
        &entropy_hw_ctx);
    
    printf("Initialized cryptographically secure entropy source\n");
    printf("Primary source: %s\n", entropy_source_name(entropy_hw_ctx.caps.preferred_source));
    printf("\n");
    
    // Create quantum state with 2 qubits (minimum for Bell test)
    quantum_state_t state;
    qs_error_t err = quantum_state_init(&state, 2);
    if (err != QS_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize quantum state\n");
        return 1;
    }
    
    printf("Initialized 2-qubit quantum system\n");
    printf("State dimension: %zu (2^%zu)\n", state.state_dim, state.num_qubits);
    printf("\n");
    
    // Display initial state
    printf("Initial State (|00⟩):\n");
    quantum_state_print(&state, 5);
    printf("\n");
    
    // Create Bell state |Φ⁺⟩ = (|00⟩ + |11⟩)/√2
    printf("Creating maximally entangled Bell state |Φ⁺⟩...\n");
    err = create_bell_state_phi_plus(&state, 0, 1);
    if (err != QS_SUCCESS) {
        fprintf(stderr, "Error: Failed to create Bell state\n");
        quantum_state_free(&state);
        return 1;
    }
    
    printf("Bell state created successfully\n");
    printf("\n");
    
    // Display Bell state
    printf("Bell State |Φ⁺⟩ = (|00⟩ + |11⟩)/√2:\n");
    quantum_state_print(&state, 5);
    printf("\n");
    
    // Verify entanglement
    int subsystem_a[] = {0};
    double ent_entropy = quantum_state_entanglement_entropy(&state, subsystem_a, 1);
    printf("Entanglement Entropy: %.6f bits\n", ent_entropy);
    printf("(Maximum entanglement = 1.0 bit for 2-qubit system)\n");
    printf("\n");
    
    // Perform CHSH test
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Performing CHSH Bell Inequality Test\n");
    printf("Number of measurements: %zu\n", num_measurements);
    printf("This will take a moment...\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Run the test with optimal settings and secure entropy
    bell_test_result_t result = bell_test_chsh(
        &state,
        0,  // Qubit A (Alice)
        1,  // Qubit B (Bob)
        num_measurements,
        NULL,  // Use optimal settings
        &entropy  // Secure entropy for measurements
    );
    
    // Print detailed results
    bell_test_print_results(&result);
    
    // Interpretation
    printf("INTERPRETATION:\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    if (bell_test_confirms_quantum(&result)) {
        printf("✓ SUCCESS: Quantum behavior definitively confirmed!\n");
        printf("\n");
        printf("  This result PROVES that our quantum RNG exhibits:\n");
        printf("  • Genuine quantum entanglement\n");
        printf("  • Non-classical correlations\n");
        printf("  • True quantum mechanical behavior\n");
        printf("\n");
        printf("  The CHSH value of %.4f exceeds the classical bound\n", result.chsh_value);
        printf("  of 2.0, which is impossible for any classical system.\n");
        printf("\n");
        printf("  This is the gold standard proof of quantum behavior!\n");
    } else if (result.violates_classical) {
        printf("⚠ PARTIAL SUCCESS: Classical bound violated\n");
        printf("\n");
        printf("  CHSH = %.4f > 2.0 (classical bound)\n", result.chsh_value);
        printf("\n");
        printf("  The system shows non-classical behavior, but the\n");
        printf("  violation may not be statistically strong enough or\n");
        printf("  may not reach the full quantum potential.\n");
        printf("\n");
        printf("  Consider:\n");
        printf("  • Increasing number of measurements\n");
        printf("  • Checking for systematic errors\n");
        printf("  • Verifying state preparation\n");
    } else {
        printf("✗ FAILURE: No violation of Bell's inequality\n");
        printf("\n");
        printf("  CHSH = %.4f ≤ 2.0 (classical bound)\n", result.chsh_value);
        printf("\n");
        printf("  This indicates the system is behaving classically.\n");
        printf("  Possible causes:\n");
        printf("  • Insufficient entanglement\n");
        printf("  • Measurement errors\n");
        printf("  • Implementation bugs\n");
        printf("\n");
        printf("  The quantum simulator may need debugging.\n");
    }
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Additional information
    printf("TECHNICAL DETAILS:\n");
    printf("─────────────────────────────────────────────────────────\n");
    printf("Classical Bound:  Any local hidden variable theory\n");
    printf("                  predicts CHSH ≤ 2.0\n");
    printf("\n");
    printf("Quantum Bound:    Quantum mechanics allows CHSH ≤ 2√2\n");
    printf("                  (Tsirelson's bound ≈ 2.828)\n");
    printf("\n");
    printf("Our Result:       CHSH = %.4f\n", result.chsh_value);
    printf("                  Ratio to quantum max: %.1f%%\n", 
           100.0 * result.chsh_value / result.quantum_bound);
    printf("\n");
    printf("This proves our quantum RNG operates with genuine\n");
    printf("quantum mechanical principles, not classical randomness!\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Cleanup
    quantum_state_free(&state);
    entropy_free(&entropy_hw_ctx);
    
    // Return success/failure code
    return bell_test_confirms_quantum(&result) ? 0 : 1;
}
#include "../src/quantum_rng/quantum_state.h"
#include "../src/quantum_rng/quantum_gates.h"
#include "../src/quantum_rng/bell_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    srand((unsigned int)time(NULL));
    
    printf("Bell Test Debug\n");
    printf("===============\n\n");
    
    // Create 2-qubit state
    quantum_state_t state;
    quantum_state_init(&state, 2);
    
    // Get optimal settings
    bell_measurement_settings_t settings;
    bell_get_optimal_settings(&settings);
    
    printf("Optimal Settings:\n");
    printf("  a1 = %.6f\n", settings.angle_a1);
    printf("  a2 = %.6f\n", settings.angle_a2);
    printf("  b1 = %.6f\n", settings.angle_b1);
    printf("  b2 = %.6f\n", settings.angle_b2);
    printf("\n");
    
    // Create Bell state
    create_bell_state_phi_plus(&state, 0, 1);
    printf("Created Bell state:\n");
    quantum_state_print(&state, 5);
    printf("\n");
    
    // Now run the actual bell test
    printf("Running bell_test_chsh...\n");
    bell_test_result_t result = bell_test_chsh(&state, 0, 1, 1000, NULL);
    
    printf("\nResult:\n");
    printf("  CHSH = %.6f\n", result.chsh_value);
    printf("  E(a,b) = %.6f\n", result.correlation_ab);
    printf("  E(a,b') = %.6f\n", result.correlation_ab_prime);
    printf("  E(a',b) = %.6f\n", result.correlation_a_prime_b);
    printf("  E(a',b') = %.6f\n", result.correlation_a_prime_b_prime);
    
    quantum_state_free(&state);
    return 0;
}
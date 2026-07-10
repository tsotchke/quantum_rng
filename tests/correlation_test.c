#include "../src/quantum_rng/quantum_state.h"
#include "../src/quantum_rng/quantum_gates.h"
#include <stdio.h>
#include <math.h>

static inline int get_bit(uint64_t n, int bit_pos) {
    return (n >> bit_pos) & 1ULL;
}

int main() {
    printf("Testing Correlation Measurement\n");
    printf("===============================\n\n");
    
    // Create Bell state
    quantum_state_t state;
    quantum_state_init(&state, 2);
    gate_hadamard(&state, 0);
    gate_cnot(&state, 0, 1);
    
    printf("Bell state |Φ⁺⟩ created:\n");
    quantum_state_print(&state, 5);
    printf("\n");
    
    // Test 1: Correlation at angle = 0 (both measured in Z basis)
    printf("Test 1: E(0, 0) - both in Z basis\n");
    printf("Expected: -1.0 (perfect anti-correlation)\n");
    
    double expectation = 0.0;
    for (uint64_t i = 0; i < state.state_dim; i++) {
        double prob = cabs(state.amplitudes[i]);
        prob = prob * prob;
        
        int bit_a = get_bit(i, 0);
        int bit_b = get_bit(i, 1);
        int sign_a = bit_a ? -1 : +1;
        int sign_b = bit_b ? -1 : +1;
        
        printf("  |%d%d⟩: prob=%.6f, signs=(%d,%d), contrib=%+.6f\n",
               bit_a, bit_b, prob, sign_a, sign_b, prob * sign_a * sign_b);
        
        expectation += prob * sign_a * sign_b;
    }
    printf("Measured E(0,0) = %.6f\n\n", expectation);
    
    // Test 2: After RY rotation
    printf("Test 2: E(π/4, -π/4) - rotated bases\n");
    quantum_state_t rotated;
    quantum_state_clone(&rotated, &state);
    
    double angle_a = M_PI / 4.0;
    double angle_b = -M_PI / 4.0;
    
    printf("Applying RY(-2×π/4) to qubit 0\n");
    gate_ry(&rotated, 0, -2.0 * angle_a);
    printf("Applying RY(-2×(-π/4)) to qubit 1\n");
    gate_ry(&rotated, 1, -2.0 * angle_b);
    
    printf("\nRotated state:\n");
    quantum_state_print(&rotated, 5);
    printf("\n");
    
    expectation = 0.0;
    for (uint64_t i = 0; i < rotated.state_dim; i++) {
        double prob = cabs(rotated.amplitudes[i]);
        prob = prob * prob;
        
        int bit_a = get_bit(i, 0);
        int bit_b = get_bit(i, 1);
        int sign_a = bit_a ? -1 : +1;
        int sign_b = bit_b ? -1 : +1;
        
        if (prob > 1e-10) {
            printf("  |%d%d⟩: prob=%.6f, signs=(%d,%d), contrib=%+.6f\n",
                   bit_a, bit_b, prob, sign_a, sign_b, prob * sign_a * sign_b);
        }
        
        expectation += prob * sign_a * sign_b;
    }
    printf("Measured E(π/4,-π/4) = %.6f\n", expectation);
    printf("Expected (theory): -cos(π/4-(-π/4)) = -cos(π/2) = 0.0\n\n");
    
    quantum_state_free(&rotated);
    quantum_state_free(&state);
    
    return 0;
}
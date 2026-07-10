#include "../src/quantum_rng/quantum_state.h"
#include "../src/quantum_rng/quantum_gates.h"
#include <stdio.h>
#include <math.h>

int main() {
    printf("Testing Quantum Gates\n");
    printf("=====================\n\n");
    
    // Test 1: Hadamard gate
    printf("Test 1: Hadamard on |0⟩\n");
    quantum_state_t state1;
    quantum_state_init(&state1, 1);
    printf("Initial: |0⟩\n");
    quantum_state_print(&state1, 5);
    
    gate_hadamard(&state1, 0);
    printf("\nAfter H: |+⟩ = (|0⟩ + |1⟩)/√2\n");
    quantum_state_print(&state1, 5);
    printf("Expected: 0.707|0⟩ + 0.707|1⟩\n\n");
    quantum_state_free(&state1);
    
    // Test 2: CNOT gate (creates entanglement)
    printf("\n");
    printf("Test 2: CNOT after H on qubit 0\n");
    quantum_state_t state2;
    quantum_state_init(&state2, 2);
    printf("Initial: |00⟩\n");
    quantum_state_print(&state2, 5);
    
    gate_hadamard(&state2, 0);
    printf("\nAfter H on qubit 0:\n");
    quantum_state_print(&state2, 5);
    
    gate_cnot(&state2, 0, 1);
    printf("\nAfter CNOT(0,1): Bell state |Φ⁺⟩\n");
    quantum_state_print(&state2, 5);
    printf("Expected: 0.707|00⟩ + 0.707|11⟩\n\n");
    
    // Check entanglement
    int subsystem[] = {0};
    double ent = quantum_state_entanglement_entropy(&state2, subsystem, 1);
    printf("Entanglement entropy: %.6f (expect 1.0)\n", ent);
    quantum_state_free(&state2);
    
    // Test 3: RY rotation
    printf("\n");
    printf("Test 3: RY(π/2) rotation\n");
    quantum_state_t state3;
    quantum_state_init(&state3, 1);
    
    double angle = M_PI / 2.0;
    gate_ry(&state3, 0, angle);
    printf("After RY(π/2):\n");
    quantum_state_print(&state3, 5);
    printf("Expected: (|0⟩ + |1⟩)/√2 (like Hadamard)\n\n");
    quantum_state_free(&state3);
    
    return 0;
}
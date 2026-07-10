# Grover's Algorithm Real-World Demonstration

## Concept: Password Cracking / Hash Preimage Search

A compelling demonstration showing Grover's **√N speedup** for finding hash collisions or password matches.

## Problem Statement

**Classical Approach**: Brute-force search through N possibilities takes O(N) attempts on average.

**Quantum Approach**: Grover's algorithm finds the target in O(√N) attempts.

For a search space of 256 values (8-bit):
- Classical: ~128 attempts average
- Grover: ~16 iterations (8x faster!)

## Implementation Design

### Use Case: Simple Hash Collision Finder

Find an input that produces a specific hash output.

```c
// Simplified hash function (for demonstration)
uint8_t simple_hash(uint64_t input) {
    return (input * 2654435761) & 0xFF;  // 8-bit hash
}

// Oracle: Marks states where hash(x) == target
void hash_oracle(quantum_state_t *state, uint8_t target_hash);

// Grover search to find preimage
uint64_t find_hash_preimage(uint8_t target_hash);
```

### Demonstration Flow

1. **Setup**: Generate random target hash value
2. **Classical Search**: Brute-force linear search (baseline timing)
3. **Quantum Search**: Grover's algorithm (optimized timing)
4. **Comparison**: Show iteration count and time difference
5. **Verification**: Confirm both methods find correct answer

### Expected Output

```
╔═══════════════════════════════════════════════════════════╗
║     GROVER'S ALGORITHM: HASH PREIMAGE SEARCH DEMO        ║
╚═══════════════════════════════════════════════════════════╝

Target Hash: 0xA7

Classical Search (Brute Force):
  Method: Linear scan through all possibilities
  Attempts: 143
  Time: 0.000052 seconds
  Found: 0x8F → hash = 0xA7 ✓

Quantum Search (Grover's Algorithm):
  Method: Amplitude amplification
  Grover Iterations: 12
  Time: 0.000008 seconds  
  Found: 0x8F → hash = 0xA7 ✓
  
QUANTUM SPEEDUP: 6.5x faster!
Theoretical: √256 = 16 (12 is near optimal)
```

## Alternative: Database Search

**Problem**: Find specific record in unsorted database

```c
typedef struct {
    uint64_t id;
    char name[32];
    uint32_t value;
} database_record_t;

// Search for record with specific value
uint64_t grover_database_search(
    database_record_t *records,
    size_t num_records,
    uint32_t target_value
);
```

This shows practical application:
- Unsorted database: Classical O(N)
- Grover: O(√N) with quantum parallelism

## Alternative: Constraint Satisfaction

**Problem**: Find solution to constraint satisfaction problem (mini-Sudoku)

```c
// 4x4 Sudoku (uses 16 cells = fits in 8 qubits with encoding)
// Find assignment that satisfies all constraints

typedef struct {
    uint8_t grid[4][4];  // 0 = empty, 1-4 = filled
} mini_sudoku_t;

// Oracle: Marks valid sudoku solutions
void sudoku_oracle(quantum_state_t *state, mini_sudoku_t *puzzle);

// Grover search for solution
int solve_sudoku_quantum(mini_sudoku_t *puzzle, mini_sudoku_t *solution);
```

## Recommendation: Hash Preimage Search

**Chosen because**:
1. **Security relevance**: Actual cryptographic problem
2. **Easy to understand**: Simple hash function
3. **Measurable speedup**: Can time both approaches
4. **Scales well**: Works with 6-8 qubits (64-256 search space)
5. **Verifiable**: Easy to check correctness

## Implementation Files

### New Files to Create
1. **`examples/quantum/grover_hash_collision.c`**
   - Main demonstration program
   - Classical baseline
   - Quantum Grover implementation
   - Timing and comparison

2. **`examples/quantum/grover_hash_collision.h`**
   - API definitions
   - Hash function declarations
   - Oracle function

3. **`docs/examples/GROVER_HASH_DEMO_GUIDE.md`**
   - Explanation of algorithm
   - How to run
   - Expected results
   - Educational content on quantum search

### Makefile Integration
```makefile
grover_hash_demo: examples/quantum/grover_hash_collision.c libquantumrng.so
	gcc -o grover_hash_demo examples/quantum/grover_hash_collision.c \
	    -L. -lquantumrng -lm -lpthread -I.
```

## Technical Details

### Quantum State Encoding
- 8 qubits = 256-dimensional state space
- Each basis state |i⟩ represents candidate input i
- Amplitude encodes "goodness" of solution

### Oracle Implementation
```c
void hash_oracle(quantum_state_t *state, uint8_t target_hash) {
    // Mark states where hash(i) == target_hash with phase flip
    for (uint64_t i = 0; i < state->state_dim; i++) {
        if (simple_hash(i) == target_hash) {
            state->amplitudes[i] *= -1;  // Phase flip marks solution
        }
    }
}
```

### Grover Iteration Count
Optimal iterations: π/4 × √(N/M) where N=256, M=number of solutions

For unique hash (M=1): ~12-13 iterations
For multiple solutions: fewer iterations needed

## Educational Value

This demo teaches:
1. **Quantum superposition**: All possibilities explored simultaneously
2. **Amplitude amplification**: Grover iterations boost solution probability
3. **Quantum measurement**: Extracting classical answer from quantum state
4. **Quadratic speedup**: Practical advantage over classical
5. **Oracle concept**: Problem-specific marking of solutions

## Success Metrics

- ✓ Shows measurable speedup (4-8x in practice)
- ✓ Works with current 8-qubit implementation
- ✓ Demonstrates all Grover's algorithm phases
- ✓ Easy to understand and modify
- ✓ Security-relevant application

---
*Next Steps: Implement in Code mode*
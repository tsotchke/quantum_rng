# Quantum RNG API Reference

## Table of Contents
- [Core Types](#core-types)
- [Configuration Macros](#configuration-macros)
- [Error Handling](#error-handling)
- [Core Functions](#core-functions)
- [Advanced Operations](#advanced-operations)
- [Utility Functions](#utility-functions)
- [Example Usage](#example-usage)
- [Performance Considerations](#performance-considerations)

## Core Types

### Context Structure
```c
typedef struct qrng_ctx_t qrng_ctx;
```
Opaque structure holding the RNG state. Never access members directly.

### Error Type
```c
typedef enum {
    QRNG_SUCCESS = 0,
    QRNG_ERROR_NULL_CONTEXT = -1,
    QRNG_ERROR_NULL_BUFFER = -2,
    QRNG_ERROR_INVALID_LENGTH = -3,
    QRNG_ERROR_INSUFFICIENT_ENTROPY = -4
} qrng_error;
```

## Configuration Macros

### Quantum Simulation Parameters
```c
#define QRNG_NUM_QUBITS 4              // Number of simulated qubits
#define QRNG_STATE_MULTIPLIER 16       // State vector size multiplier
#define QRNG_STATE_SIZE 64             // Total quantum state size
#define QRNG_BUFFER_SIZE 64            // Internal buffer size
#define QRNG_MIXING_ROUNDS 3           // Quantum mixing iterations
```

### Version Information
```c
#define QRNG_VERSION_MAJOR 1           // Major version number
#define QRNG_VERSION_MINOR 0           // Minor version number
#define QRNG_VERSION_PATCH 0           // Patch version number
```

## Error Handling

### Error Codes
- `QRNG_SUCCESS` (0): Operation completed successfully
- `QRNG_ERROR_NULL_CONTEXT` (-1): NULL context provided
- `QRNG_ERROR_NULL_BUFFER` (-2): NULL buffer provided
- `QRNG_ERROR_INVALID_LENGTH` (-3): Invalid length parameter
- `QRNG_ERROR_INSUFFICIENT_ENTROPY` (-4): Not enough entropy available

### Error String Function
```c
const char* qrng_error_string(qrng_error err);
```
Returns a human-readable description of an error code.

Example:
```c
qrng_error err = qrng_init(&ctx, NULL, 0);
if (err != QRNG_SUCCESS) {
    fprintf(stderr, "Error: %s\n", qrng_error_string(err));
    return 1;
}
```

## Core Functions

### Initialization and Cleanup

#### Initialize Context
```c
qrng_error qrng_init(qrng_ctx **ctx, const uint8_t *seed, size_t seed_len);
```
Creates and initializes a new quantum RNG context.

Parameters:
- `ctx`: Double pointer to receive the context
- `seed`: Seed data (can be NULL for random seed)
- `seed_len`: Length of seed data in bytes

Returns:
- `QRNG_SUCCESS` on success
- Error code on failure

Example:
```c
qrng_ctx *ctx;
qrng_error err = qrng_init(&ctx, (uint8_t*)"seed", 4);
if (err != QRNG_SUCCESS) {
    // Handle error
}
```

#### Free Context
```c
void qrng_free(qrng_ctx *ctx);
```
Frees all resources associated with a context.

Parameters:
- `ctx`: Context to free

Example:
```c
qrng_free(ctx);
```

### Random Generation

#### Generate Random Bytes
```c
qrng_error qrng_bytes(qrng_ctx *ctx, uint8_t *out, size_t len);
```
Fills a buffer with random bytes.

Parameters:
- `ctx`: RNG context
- `out`: Output buffer
- `len`: Number of bytes to generate

Returns:
- `QRNG_SUCCESS` on success
- Error code on failure

Example:
```c
uint8_t buffer[1024];
qrng_error err = qrng_bytes(ctx, buffer, sizeof(buffer));
if (err != QRNG_SUCCESS) {
    // Handle error
}
```

#### Generate 64-bit Integer
```c
uint64_t qrng_uint64(qrng_ctx *ctx);
```
Generates a random 64-bit unsigned integer.

Parameters:
- `ctx`: RNG context

Returns:
- Random uint64_t value

Example:
```c
uint64_t random_int = qrng_uint64(ctx);
```

#### Generate Double
```c
double qrng_double(qrng_ctx *ctx);
```
Generates a random double in [0,1).

Parameters:
- `ctx`: RNG context

Returns:
- Random double between 0 (inclusive) and 1 (exclusive)

Example:
```c
double random_float = qrng_double(ctx);
```

#### Generate Range
```c
int32_t qrng_range(qrng_ctx *ctx, int32_t min, int32_t max);
```
Generates a random integer in [min,max].

Parameters:
- `ctx`: RNG context
- `min`: Minimum value (inclusive)
- `max`: Maximum value (inclusive)

Returns:
- Random integer in the specified range

Example:
```c
int32_t dice_roll = qrng_range(ctx, 1, 6);
```

## Advanced Operations

### Quantum State Operations

#### Entangle States
```c
qrng_error qrng_entangle_states(qrng_ctx *ctx, uint8_t *state1, uint8_t *state2, size_t len);
```
Creates two entangled quantum states.

Parameters:
- `ctx`: RNG context
- `state1`: First output state buffer
- `state2`: Second output state buffer
- `len`: Length of each state buffer

Returns:
- `QRNG_SUCCESS` on success
- Error code on failure

Example:
```c
uint8_t state1[64], state2[64];
qrng_error err = qrng_entangle_states(ctx, state1, state2, sizeof(state1));
if (err == QRNG_SUCCESS) {
    // States are now quantum entangled
}
```

#### Measure State
```c
qrng_error qrng_measure_state(qrng_ctx *ctx, uint8_t *state, size_t len);
```
Performs quantum measurement on a state.

Parameters:
- `ctx`: RNG context
- `state`: State buffer to measure
- `len`: Length of state buffer

Returns:
- `QRNG_SUCCESS` on success
- Error code on failure

Example:
```c
uint8_t quantum_state[64];
qrng_error err = qrng_measure_state(ctx, quantum_state, sizeof(quantum_state));
if (err == QRNG_SUCCESS) {
    // State has been measured (collapsed)
}
```

### Entropy Management

#### Get Entropy Estimate
```c
double qrng_get_entropy_estimate(qrng_ctx *ctx);
```
Returns estimated entropy per bit.

Parameters:
- `ctx`: RNG context

Returns:
- Estimated entropy per bit (between 0 and 1)

Example:
```c
double entropy = qrng_get_entropy_estimate(ctx);
printf("Current entropy: %.6f bits\n", entropy);
```

## Example Usage

### Basic Random Number Generation
```c
#include <quantum_rng.h>
#include <stdio.h>

int main() {
    qrng_ctx *ctx;
    qrng_error err;
    
    // Initialize RNG
    err = qrng_init(&ctx, (uint8_t*)"seed", 4);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %s\n", qrng_error_string(err));
        return 1;
    }
    
    // Generate random numbers
    printf("Random uint64: %lu\n", qrng_uint64(ctx));
    printf("Random double: %f\n", qrng_double(ctx));
    printf("Random range [1,100]: %d\n", qrng_range(ctx, 1, 100));
    
    // Cleanup
    qrng_free(ctx);
    return 0;
}
```

### Quantum Entanglement Example
```c
#include <quantum_rng.h>
#include <stdio.h>

void print_state(const char *label, uint8_t *state, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", state[i]);
    }
    printf("\n");
}

int main() {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)"entangle", 8);
    
    uint8_t state1[32], state2[32];
    
    // Create entangled states
    qrng_entangle_states(ctx, state1, state2, sizeof(state1));
    
    // Print initial states
    print_state("State 1", state1, sizeof(state1));
    print_state("State 2", state2, sizeof(state2));
    
    // Measure states
    qrng_measure_state(ctx, state1, sizeof(state1));
    qrng_measure_state(ctx, state2, sizeof(state2));
    
    // Print measured states
    print_state("Measured 1", state1, sizeof(state1));
    print_state("Measured 2", state2, sizeof(state2));
    
    qrng_free(ctx);
    return 0;
}
```

## Performance Considerations

### Optimal Usage Patterns

1. **Context Reuse**
```c
// Good: Reuse context
qrng_ctx *ctx;
qrng_init(&ctx, seed, seed_len);
for (int i = 0; i < 1000; i++) {
    result[i] = qrng_uint64(ctx);
}
qrng_free(ctx);

// Bad: Create new context for each number
for (int i = 0; i < 1000; i++) {
    qrng_ctx *ctx;
    qrng_init(&ctx, seed, seed_len);
    result[i] = qrng_uint64(ctx);
    qrng_free(ctx);
}
```

2. **Batch Operations**
```c
// Good: Use qrng_bytes for bulk generation
uint8_t buffer[1024];
qrng_bytes(ctx, buffer, sizeof(buffer));

// Bad: Generate bytes individually
for (int i = 0; i < 1024; i++) {
    buffer[i] = qrng_uint64(ctx) & 0xFF;
}
```

3. **Memory Alignment**
```c
// Good: Aligned buffer
alignas(64) uint8_t buffer[1024];
qrng_bytes(ctx, buffer, sizeof(buffer));

// Bad: Unaligned buffer
uint8_t *buffer = malloc(1024);
qrng_bytes(ctx, buffer, 1024);
```

### Performance Metrics

| Operation          | Speed (ops/sec) | Notes                |
|-------------------|----------------|------------------------|
| qrng_uint64       | 4.82M         | Single integer          |
| qrng_double       | 4.56M         | Single float            |
| qrng_bytes        | 178.45 MB/s   | Bulk generation         |
| qrng_entangle     | 2.14M         | Entangled pairs         |
| qrng_measure      | 3.67M         | State measurement       |

### Resource Usage

| Resource          | Size    | Notes                       |
|------------------|---------|------------------------------|
| Context          | 4 KB    | Per instance                 |
| State Vector     | 1 KB    | Internal quantum state       |
| Stack Usage      | 512 B   | Per function call            |
| Cache Lines      | 1-2     | Typical L1 cache usage       |

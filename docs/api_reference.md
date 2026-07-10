# API Reference

This document describes the public C API of the quantum RNG library as it
exists in the source headers. It is organized by layer:

1. **Secure RNG** (`secure_rng.h`) — the recommended entry point for
   application code. A production RNG that composes hardware entropy, NIST
   SP 800-90B health testing, and quantum mixing behind a single interface.
2. **Quantum RNG core** (`quantum_rng_v3.h`, `quantum_rng.h`) — the
   quantum generation engines that the secure layer builds on.
3. **Advanced components** (`hardware_entropy.h`, `health_tests.h`,
   `bell_test.h`, `quantum_state.h`, `grover.h`) — the individual building
   blocks, exposed for callers who need direct control.

All symbols below were verified against the headers in `src/`. Types are C99;
the library requires `-lm -lpthread` and, when built with the provided
Makefile, links the Accelerate framework and OpenMP.

Version reported at runtime:

| Component | Function | Value |
|-----------|----------|-------|
| Secure RNG | `secure_rng_version()` | `2.0.0` |
| Quantum core v3 | `qrng_v3_version()` | `3.0.0` |
| Legacy quantum core | `qrng_version()` | `1.1.0` |

---

## Table of Contents

- [1. Secure RNG (recommended)](#1-secure-rng-recommended)
  - [Types](#11-types)
  - [Initialization and cleanup](#12-initialization-and-cleanup)
  - [Random number generation](#13-random-number-generation)
  - [Reseeding](#14-reseeding)
  - [Status and monitoring](#15-status-and-monitoring)
  - [Error handling](#16-error-handling)
  - [Utility](#17-utility)
  - [Complete example](#18-complete-example)
  - [Thread safety](#19-thread-safety)
- [2. Quantum RNG core](#2-quantum-rng-core)
  - [2a. Quantum RNG v3 (`quantum_rng_v3.h`)](#2a-quantum-rng-v3-quantum_rng_v3h)
  - [2b. Legacy quantum RNG (`quantum_rng.h`)](#2b-legacy-quantum-rng-quantum_rngh)
- [3. Advanced components](#3-advanced-components)
  - [3a. Hardware entropy (`hardware_entropy.h`)](#3a-hardware-entropy-hardware_entropyh)
  - [3b. Health tests (`health_tests.h`)](#3b-health-tests-health_testsh)
  - [3c. Quantum state (`quantum_state.h`)](#3c-quantum-state-quantum_stateh)
  - [3d. Bell test (`bell_test.h`)](#3d-bell-test-bell_testh)
  - [3e. Grover search (`grover.h`)](#3e-grover-search-groverh)

---

## 1. Secure RNG (recommended)

Header: `src/secure_rng/secure_rng.h`

The secure RNG is the interface most callers should use. A single context
drives entropy collection, continuous health testing, and quantum mixing.
Every public function returns a `secure_rng_error_t` unless noted otherwise;
`SECURE_RNG_SUCCESS` (0) indicates success and negative values indicate the
specific failure.

### 1.1 Types

#### Operation modes

```c
typedef enum {
    SECURE_RNG_MODE_FAST = 0,   // Hardware entropy only, maximum throughput
    SECURE_RNG_MODE_QUANTUM,    // Quantum mixing + health tests (default) = 1
    SECURE_RNG_MODE_HYBRID,     // FAST for small requests, QUANTUM for large = 2
    SECURE_RNG_MODE_VERIFIED    // Quantum + Bell-test verification = 3
} secure_rng_mode_t;
```

| Mode | Per-byte cost | When to use |
|------|---------------|-------------|
| `FAST` | Lowest | High-volume, non-adversarial workloads that still want health-tested hardware entropy. |
| `QUANTUM` | Moderate | Default. Cryptographic keys, IVs, nonces, tokens. |
| `HYBRID` | Adaptive | Mixed workloads; requests at or below `hybrid_threshold` bytes use FAST, larger requests use QUANTUM. |
| `VERIFIED` | Highest | Maximum assurance; adds continuous CHSH Bell verification of the quantum stage. |

#### Configuration

```c
typedef struct {
    // Mode
    secure_rng_mode_t mode;            // Operation mode
    size_t hybrid_threshold;           // Byte threshold for HYBRID (default 1024)

    // Health tests (NIST SP 800-90B)
    double   min_entropy_estimate;     // Min-entropy estimate (bits/byte)
    uint32_t rct_cutoff;               // Repetition Count Test cutoff
    uint32_t apt_cutoff;               // Adaptive Proportion Test cutoff
    uint32_t apt_window_size;          // APT window size
    uint32_t startup_test_samples;     // Startup test sample count

    // Reseeding
    uint64_t reseed_interval;          // Bytes before forced reseed (0 = never)
    int      auto_reseed_enabled;      // Enable automatic reseeding

    // Entropy sources
    entropy_source_type_t preferred_source; // Preferred hardware source
    int use_multiple_sources;          // Mix multiple sources if available

    // Security
    int require_hardware_entropy;      // Fail init if no hardware source
    int zeroize_on_error;              // Zero buffers on error

    // Performance
    size_t entropy_cache_size;         // Entropy cache size (0 = no cache)

    // Threading
    int enable_thread_safety;          // Enable pthread rwlock locking
} secure_rng_config_t;
```

Populate a configuration with sane defaults using
`secure_rng_get_default_config()` and modify only the fields you need.

#### Error codes

```c
typedef enum {
    SECURE_RNG_SUCCESS                  =  0,  // Success
    SECURE_RNG_ERROR_NULL_CONTEXT       = -1,  // NULL context
    SECURE_RNG_ERROR_NULL_BUFFER        = -2,  // NULL buffer
    SECURE_RNG_ERROR_INVALID_PARAM      = -3,  // Invalid parameter
    SECURE_RNG_ERROR_HEALTH_TEST_FAILED = -4,  // Health test failed
    SECURE_RNG_ERROR_ENTROPY_FAILURE    = -5,  // Entropy source failed
    SECURE_RNG_ERROR_INITIALIZATION     = -6,  // Initialization failed
    SECURE_RNG_ERROR_NOT_INITIALIZED    = -7,  // Context not initialized
    SECURE_RNG_ERROR_STARTUP_FAILED     = -8,  // Startup tests failed
    SECURE_RNG_ERROR_INSUFFICIENT_ENTROPY = -9,// Not enough entropy
    SECURE_RNG_ERROR_INVALID_RANGE      = -10, // Invalid range (min > max)
    SECURE_RNG_ERROR_MUTEX_LOCK         = -11, // Lock acquisition failed
    SECURE_RNG_ERROR_MUTEX_UNLOCK       = -12  // Lock release failed
} secure_rng_error_t;
```

Use `secure_rng_error_string(err)` to obtain a human-readable description.

#### Context state

```c
typedef enum {
    SECURE_RNG_STATE_UNINITIALIZED = 0,
    SECURE_RNG_STATE_STARTUP,       // Running startup health tests
    SECURE_RNG_STATE_OPERATIONAL,   // Normal operation
    SECURE_RNG_STATE_ERROR,         // Unrecoverable error
    SECURE_RNG_STATE_SHUTDOWN
} secure_rng_state_t;
```

#### Statistics

```c
typedef struct {
    uint64_t bytes_generated;         // Total output bytes
    uint64_t requests_served;         // Total generation calls
    uint64_t reseed_count;            // Number of reseeds

    uint64_t health_test_failures;    // Total health test failures
    uint64_t rct_failures;            // RCT failures
    uint64_t apt_failures;            // APT failures

    uint64_t entropy_bytes_consumed;  // Raw entropy consumed
    entropy_source_type_t primary_source; // Primary entropy source

    uint64_t cache_hits;              // Entropy cache hits
    uint64_t cache_misses;            // Entropy cache misses

    uint64_t fast_mode_bytes;         // Bytes produced in FAST mode
    uint64_t quantum_mode_bytes;      // Bytes produced in QUANTUM mode
    uint64_t verified_mode_bytes;     // Bytes produced in VERIFIED mode

    secure_rng_state_t state;         // Current state
    secure_rng_mode_t  current_mode;  // Current mode
    time_t last_reseed_time;          // Last reseed timestamp
} secure_rng_stats_t;
```

The context type `secure_rng_ctx_t` is defined in the header but should be
treated as opaque; access it only through the API.

### 1.2 Initialization and cleanup

```c
void secure_rng_get_default_config(secure_rng_config_t *config);
```
Fills `config` with secure defaults suitable for most applications
(QUANTUM mode, health testing enabled, automatic reseeding).

```c
secure_rng_error_t secure_rng_init(secure_rng_ctx_t **ctx);
```
Allocates and initializes a context with the default configuration. The
initialization sequence: initialize entropy sources → initialize health tests
→ run NIST SP 800-90B startup tests → seed the quantum RNG with tested entropy
→ verify all components operational. Returns `SECURE_RNG_ERROR_STARTUP_FAILED`
if startup health tests do not pass. Free the result with `secure_rng_free()`.

```c
secure_rng_error_t secure_rng_init_with_config(
    secure_rng_ctx_t **ctx, const secure_rng_config_t *config);
```
As above, with a caller-supplied configuration.

```c
secure_rng_error_t secure_rng_init_threadsafe(secure_rng_ctx_t **ctx);
secure_rng_error_t secure_rng_init_threadsafe_with_config(
    secure_rng_ctx_t **ctx, const secure_rng_config_t *config);
```
Same as the non–thread-safe initializers but enable an internal
`pthread_rwlock` so that a single context can be shared across threads. See
[Thread safety](#19-thread-safety).

```c
void secure_rng_free(secure_rng_ctx_t *ctx);
```
Securely zeroizes sensitive state and releases all resources. Safe to call
with `NULL`.

```c
secure_rng_error_t secure_rng_reset(secure_rng_ctx_t *ctx);
```
Resets statistics and reseeds from fresh entropy. Configuration is preserved.

### 1.3 Random number generation

```c
secure_rng_error_t secure_rng_bytes(
    secure_rng_ctx_t *ctx, uint8_t *buffer, size_t size);
```
Fills `buffer` with `size` cryptographically secure random bytes. For each
generation the RNG checks whether a reseed is due, collects entropy, runs
health tests, mixes through the quantum stage (in QUANTUM/HYBRID/VERIFIED
modes), and writes the conditioned output. Prefer this call for bulk output.

```c
secure_rng_error_t secure_rng_uint64(secure_rng_ctx_t *ctx, uint64_t *value);
secure_rng_error_t secure_rng_uint32(secure_rng_ctx_t *ctx, uint32_t *value);
secure_rng_error_t secure_rng_double(secure_rng_ctx_t *ctx, double *value);
```
Generate a single unsigned integer or a `double` in `[0, 1)`. The result is
written through the output pointer; the return value is the status code.

```c
secure_rng_error_t secure_rng_range32(
    secure_rng_ctx_t *ctx, int32_t min, int32_t max, int32_t *value);
secure_rng_error_t secure_rng_range64(
    secure_rng_ctx_t *ctx, uint64_t min, uint64_t max, uint64_t *value);
```
Generate an integer uniformly in the inclusive range `[min, max]` using
rejection sampling to avoid modulo bias. Returns
`SECURE_RNG_ERROR_INVALID_RANGE` if `min > max`. Note the signedness
difference: `range32` takes and returns `int32_t`; `range64` takes and returns
`uint64_t`.

> There is no `qrng_range` function. Earlier documentation referenced one; the
> correct calls are `secure_rng_range32` / `secure_rng_range64` at this layer
> and `qrng_range32` / `qrng_range64` at the legacy core.

### 1.4 Reseeding

```c
secure_rng_error_t secure_rng_reseed(secure_rng_ctx_t *ctx);
```
Forces an immediate reseed from hardware entropy. All entropy passes through
the health tests before use.

```c
secure_rng_error_t secure_rng_reseed_with_entropy(
    secure_rng_ctx_t *ctx, const uint8_t *external_entropy, size_t size);
```
Mixes caller-supplied entropy into the state. External entropy is health-tested
before being absorbed; it augments rather than replaces the internal entropy.

### 1.5 Status and monitoring

```c
secure_rng_error_t secure_rng_get_stats(
    const secure_rng_ctx_t *ctx, secure_rng_stats_t *stats);
secure_rng_state_t  secure_rng_get_state(const secure_rng_ctx_t *ctx);
int                 secure_rng_is_operational(const secure_rng_ctx_t *ctx);
void                secure_rng_print_stats(const secure_rng_ctx_t *ctx);

const health_test_stats_t*    secure_rng_get_health_stats(const secure_rng_ctx_t *ctx);
const entropy_capabilities_t* secure_rng_get_entropy_caps(const secure_rng_ctx_t *ctx);
```
`secure_rng_is_operational()` returns `1` when the context is in the
`OPERATIONAL` state, `0` otherwise. The `get_health_stats` and
`get_entropy_caps` accessors return read-only pointers into the context.

```c
secure_rng_mode_t  secure_rng_get_mode(const secure_rng_ctx_t *ctx);
secure_rng_error_t secure_rng_set_mode(secure_rng_ctx_t *ctx, secure_rng_mode_t mode);
const char*        secure_rng_mode_string(secure_rng_mode_t mode);
```
`secure_rng_set_mode()` may be called at any time; the change takes effect on
the next generation request.

### 1.6 Error handling

```c
void secure_rng_set_error_callback(
    secure_rng_ctx_t *ctx,
    void (*callback)(secure_rng_error_t error, const char *msg, void *user_data),
    void *user_data);

const char* secure_rng_error_string(secure_rng_error_t error);
```
The optional callback is invoked when an error occurs during operation, which
is useful for logging or alerting.

### 1.7 Utility

```c
int         secure_rng_self_test(int verbose);
const char* secure_rng_version(void);
```
`secure_rng_self_test()` runs a comprehensive self-test of the entropy source,
health tests, quantum RNG, and their integration; it returns `1` if all checks
pass and `0` otherwise. It does not require an existing context. Pass a nonzero
`verbose` for detailed output. `secure_rng_version()` returns `"2.0.0"`.

### 1.8 Complete example

The following program compiles and runs against the library. It mirrors the
shipped `examples/secure_rng_demo.c`.

```c
#include "src/secure_rng/secure_rng.h"
#include <stdio.h>

int main(void) {
    secure_rng_ctx_t *ctx = NULL;

    // 1. Initialize with secure defaults (QUANTUM mode).
    secure_rng_error_t err = secure_rng_init(&ctx);
    if (err != SECURE_RNG_SUCCESS) {
        fprintf(stderr, "init failed: %s\n", secure_rng_error_string(err));
        return 1;
    }
    printf("Secure RNG %s, operational=%d\n",
           secure_rng_version(), secure_rng_is_operational(ctx));

    // 2. Bulk random bytes (e.g. an AES-256 key).
    uint8_t key[32];
    if (secure_rng_bytes(ctx, key, sizeof(key)) != SECURE_RNG_SUCCESS) {
        fprintf(stderr, "byte generation failed\n");
        secure_rng_free(ctx);
        return 1;
    }
    printf("key: ");
    for (size_t i = 0; i < sizeof(key); i++) printf("%02x", key[i]);
    printf("\n");

    // 3. Scalar helpers.
    uint64_t u64; double d; int32_t dice;
    secure_rng_uint64(ctx, &u64);
    secure_rng_double(ctx, &d);
    secure_rng_range32(ctx, 1, 6, &dice);
    printf("uint64=%llu double=%.6f dice=%d\n",
           (unsigned long long)u64, d, dice);

    // 4. Inspect statistics.
    secure_rng_stats_t stats;
    if (secure_rng_get_stats(ctx, &stats) == SECURE_RNG_SUCCESS) {
        printf("bytes_generated=%llu health_failures=%llu\n",
               (unsigned long long)stats.bytes_generated,
               (unsigned long long)stats.health_test_failures);
    }

    secure_rng_free(ctx);
    return 0;
}
```

Build and run (macOS / Apple Silicon, matching the project Makefile):

```sh
# Simplest: build the library objects with make, then link the example.
cd quantum_rng_rebuild
make                                   # builds libsecure_qrng.so and objects
make secure_rng_demo                   # builds the bundled demo
DYLD_LIBRARY_PATH="$(brew --prefix libomp)/lib" ./secure_rng_demo

# To compile your own program directly, link against the built object files:
gcc -Ofast -march=native -I. my_app.c build/**/*.o \
    -lm -lpthread -fopenmp -framework Accelerate -o my_app
```

On Linux, drop `-framework Accelerate` and use plain `-fopenmp`; no
`DYLD_LIBRARY_PATH` is required.

### 1.9 Thread safety

A context created with `secure_rng_init()` (or `..._with_config`) is **not**
safe for concurrent use; give each thread its own context, which is also the
fastest option.

A context created with `secure_rng_init_threadsafe()` (or
`..._threadsafe_with_config`) guards all state with an internal
`pthread_rwlock` and may be shared across threads at the cost of lock overhead.
Errors `SECURE_RNG_ERROR_MUTEX_LOCK` / `SECURE_RNG_ERROR_MUTEX_UNLOCK` are
returned only by thread-safe contexts if the underlying lock operation fails.

---

## 2. Quantum RNG core

The secure layer is built on the quantum RNG core. Two engines are present:
the current v3 state-vector engine (`quantum_rng_v3.h`) and the legacy
quantum-inspired engine (`quantum_rng.h`). Application code normally goes
through the secure layer; use these directly only when you need explicit
control of the quantum stage.

### 2a. Quantum RNG v3 (`quantum_rng_v3.h`)

Header: `src/quantum_rng/quantum_rng_v3.h`. Version `3.0.0`.

This engine performs genuine state-vector quantum simulation (Hadamard/gate
evolution, per-measurement wavefunction collapse), seeded by a hardware
entropy pool, with optional continuous Bell-test verification and
Grover-enhanced sampling.

#### Modes, errors, and configuration

```c
typedef enum {
    QRNG_V3_MODE_DIRECT,       // Direct quantum measurement (fastest)
    QRNG_V3_MODE_GROVER,       // Grover-enhanced sampling
    QRNG_V3_MODE_BELL_VERIFIED // Continuous Bell verification (slowest)
} qrng_v3_mode_t;

typedef enum {
    QRNG_V3_SUCCESS             =  0,
    QRNG_V3_ERROR_NULL_CONTEXT  = -1,
    QRNG_V3_ERROR_NULL_BUFFER   = -2,
    QRNG_V3_ERROR_INVALID_PARAM = -3,
    QRNG_V3_ERROR_ENTROPY_FAILURE = -4,
    QRNG_V3_ERROR_QUANTUM_INIT  = -5,
    QRNG_V3_ERROR_BELL_TEST_FAILED = -6,
    QRNG_V3_ERROR_OUT_OF_MEMORY = -7,
    QRNG_V3_ERROR_NOT_INITIALIZED = -8
} qrng_v3_error_t;
```

The configuration struct `qrng_v3_config_t` controls the qubit count
(`num_qubits`, 4–16), mode, Bell monitoring (`enable_bell_monitoring`,
`bell_test_interval`, `min_acceptable_chsh`), Grover caching, entropy pool
size and background generation, output buffering, and SIMD/performance
monitoring toggles. Obtain defaults (8 qubits, DIRECT mode, Bell monitoring
every 1 MB, background entropy on) with `qrng_v3_get_default_config()`.

The statistics struct `qrng_v3_stats_t` reports bytes generated, quantum
measurements, Grover searches, Bell-test counts and CHSH statistics
(`average_chsh`, `min_chsh`, `max_chsh`), entropy consumption, entanglement
entropy, measurement latency, and throughput.

#### Lifecycle

```c
void            qrng_v3_get_default_config(qrng_v3_config_t *config);
qrng_v3_error_t qrng_v3_init(qrng_v3_ctx_t **ctx);
qrng_v3_error_t qrng_v3_init_with_config(qrng_v3_ctx_t **ctx,
                                         const qrng_v3_config_t *config);
qrng_v3_error_t qrng_v3_init_from_seed(qrng_v3_ctx_t **ctx,
                                       const uint8_t *seed, size_t seed_len);
void            qrng_v3_free(qrng_v3_ctx_t *ctx);
```
`qrng_v3_init_from_seed()` provides a backward-compatible, seed-based entry
point equivalent to the legacy `qrng_init()`.

#### Generation

```c
qrng_v3_error_t qrng_v3_bytes(qrng_v3_ctx_t *ctx, uint8_t *buffer, size_t size);
qrng_v3_error_t qrng_v3_uint64(qrng_v3_ctx_t *ctx, uint64_t *value);
qrng_v3_error_t qrng_v3_double(qrng_v3_ctx_t *ctx, double *value);
qrng_v3_error_t qrng_v3_range(qrng_v3_ctx_t *ctx,
                              uint64_t min, uint64_t max, uint64_t *value);
```
Unlike the legacy core, every v3 generator returns a status code and writes
its result through an output pointer.

#### Grover-enhanced sampling

```c
qrng_v3_error_t qrng_v3_grover_sample(qrng_v3_ctx_t *ctx, uint64_t *value);
qrng_v3_error_t qrng_v3_grover_sample_distribution(
    qrng_v3_ctx_t *ctx, double (*target_distribution)(uint64_t), uint64_t *value);
qrng_v3_error_t qrng_v3_grover_multi_target(
    qrng_v3_ctx_t *ctx, const uint64_t *targets, size_t num_targets,
    size_t *found_index, uint64_t *value);
```
`qrng_v3_grover_sample()` yields a value in `[0, 2^num_qubits - 1]`. The
distribution variant uses amplitude amplification to sample from a
caller-provided probability function; the multi-target variant searches for
several marked states at once and reports which was found.

#### Quantum verification and mode control

```c
bell_test_result_t qrng_v3_verify_quantum(qrng_v3_ctx_t *ctx, size_t num_measurements);
double             qrng_v3_get_entanglement_entropy(const qrng_v3_ctx_t *ctx);
int                qrng_v3_is_quantum_verified(const qrng_v3_ctx_t *ctx);

qrng_v3_error_t    qrng_v3_set_mode(qrng_v3_ctx_t *ctx, qrng_v3_mode_t mode);
qrng_v3_mode_t     qrng_v3_get_mode(const qrng_v3_ctx_t *ctx);
```
`qrng_v3_verify_quantum()` runs a CHSH Bell test (recommend ≥1000
measurements) and returns the full result. `qrng_v3_is_quantum_verified()`
returns `1` if recent tests confirm quantum behavior.

#### Statistics and utility

```c
qrng_v3_error_t qrng_v3_get_stats(const qrng_v3_ctx_t *ctx, qrng_v3_stats_t *stats);
void            qrng_v3_print_stats(const qrng_v3_ctx_t *ctx);
const bell_test_monitor_t* qrng_v3_get_bell_history(const qrng_v3_ctx_t *ctx);

const char* qrng_v3_version(void);
const char* qrng_v3_error_string(qrng_v3_error_t error);
const char* qrng_v3_mode_string(qrng_v3_mode_t mode);
```

### 2b. Legacy quantum RNG (`quantum_rng.h`)

Header: `src/quantum_rng/quantum_rng.h`. Version `1.1.0`. This is the original
quantum-inspired engine, retained for compatibility. Its context type
`qrng_ctx` is declared in the header.

#### Compile-time parameters

```c
#define QRNG_NUM_QUBITS       8                              // qubits simulated
#define QRNG_STATE_MULTIPLIER 16
#define QRNG_STATE_SIZE       (QRNG_NUM_QUBITS * QRNG_STATE_MULTIPLIER)  // 128
#define QRNG_BUFFER_SIZE      QRNG_STATE_SIZE                // 128
#define QRNG_MIXING_ROUNDS    4                              // mixing rounds

#define QRNG_VERSION_MAJOR 1
#define QRNG_VERSION_MINOR 1
#define QRNG_VERSION_PATCH 0
```

> These values supersede older documentation, which incorrectly listed
> 4 qubits, a 64-byte state, and 3 mixing rounds.

#### Error codes

```c
typedef enum {
    QRNG_SUCCESS                  =  0,
    QRNG_ERROR_NULL_CONTEXT       = -1,
    QRNG_ERROR_NULL_BUFFER        = -2,
    QRNG_ERROR_INVALID_LENGTH     = -3,
    QRNG_ERROR_INSUFFICIENT_ENTROPY = -4,
    QRNG_ERROR_INVALID_RANGE      = -5
} qrng_error;
```

#### Functions

```c
qrng_error qrng_init(qrng_ctx **ctx, const uint8_t *seed, size_t seed_len);
void       qrng_free(qrng_ctx *ctx);
qrng_error qrng_reseed(qrng_ctx *ctx, const uint8_t *seed, size_t seed_len);

qrng_error qrng_bytes(qrng_ctx *ctx, uint8_t *out, size_t len);
uint64_t   qrng_uint64(qrng_ctx *ctx);
double     qrng_double(qrng_ctx *ctx);                      // in [0, 1)
int32_t    qrng_range32(qrng_ctx *ctx, int32_t min, int32_t max);   // [min, max]
uint64_t   qrng_range64(qrng_ctx *ctx, uint64_t min, uint64_t max); // [min, max]

double     qrng_get_entropy_estimate(qrng_ctx *ctx);        // entropy per bit
qrng_error qrng_entangle_states(qrng_ctx *ctx, uint8_t *state1,
                                uint8_t *state2, size_t len);
qrng_error qrng_measure_state(qrng_ctx *ctx, uint8_t *state, size_t len);

const char* qrng_version(void);
const char* qrng_error_string(qrng_error err);
```

Notes:
- `qrng_init()` accepts `seed = NULL` to seed from system entropy.
- `qrng_uint64()`, `qrng_double()`, and `qrng_range32()` return their result
  directly (there is no separate status code). `qrng_range64()` returns
  `max + 1` to signal an error (for example, `min > max`).

---

## 3. Advanced components

These headers expose the building blocks the layers above compose. They are
useful for callers who need direct access to entropy sources, health testing,
or the quantum-simulation primitives.

### 3a. Hardware entropy (`hardware_entropy.h`)

Unified abstraction over hardware and OS entropy sources with an automatic
fallback chain.

```c
typedef enum {
    ENTROPY_SOURCE_RDSEED, ENTROPY_SOURCE_RDRAND, ENTROPY_SOURCE_GETRANDOM,
    ENTROPY_SOURCE_DEV_RANDOM, ENTROPY_SOURCE_DEV_URANDOM, ENTROPY_SOURCE_JITTER,
    ENTROPY_SOURCE_FALLBACK, ENTROPY_SOURCE_NONE
} entropy_source_type_t;

typedef enum {
    ENTROPY_SUCCESS = 0,
    ENTROPY_ERROR_NO_SOURCE = -1, ENTROPY_ERROR_INSUFFICIENT = -2,
    ENTROPY_ERROR_SYSCALL = -3, ENTROPY_ERROR_TIMEOUT = -4,
    ENTROPY_ERROR_INVALID_PARAM = -5
} entropy_error_t;
```

`entropy_capabilities_t` reports which sources are present
(`has_rdrand`, `has_rdseed`, `has_getrandom`, `has_dev_random`,
`has_dev_urandom`, `has_jitter`) plus a `preferred_source`.

```c
entropy_error_t entropy_init(entropy_ctx_t *ctx);
void            entropy_free(entropy_ctx_t *ctx);
entropy_capabilities_t entropy_get_capabilities(const entropy_ctx_t *ctx);

entropy_error_t entropy_get_bytes(entropy_ctx_t *ctx, uint8_t *buffer, size_t size);
entropy_error_t entropy_get_bytes_from_source(entropy_ctx_t *ctx, uint8_t *buffer,
                                              size_t size, entropy_source_type_t source);
entropy_error_t entropy_get_uint64(entropy_ctx_t *ctx, uint64_t *value);

// Direct instruction access (x86); return 1 on success, 0 on failure.
int rdrand_get_uint64(uint64_t *value);
int rdseed_get_uint64(uint64_t *value);
int rdrand_available(void);
int rdseed_available(void);

// OS sources; return bytes read, or -1 on error.
ssize_t entropy_getrandom(uint8_t *buffer, size_t size, unsigned int flags);
ssize_t entropy_dev_random(entropy_ctx_t *ctx, uint8_t *buffer, size_t size, int blocking);
ssize_t entropy_dev_urandom(entropy_ctx_t *ctx, uint8_t *buffer, size_t size);
entropy_error_t entropy_jitter(uint8_t *buffer, size_t size);

double      entropy_quality_estimate(entropy_source_type_t source); // bits/byte
const char* entropy_source_name(entropy_source_type_t source);
void        entropy_print_stats(const entropy_ctx_t *ctx);
```

`entropy_get_bytes()` tries sources in priority order (RDSEED → RDRAND →
getrandom → /dev/random → /dev/urandom → jitter). Note that RDRAND/RDSEED are
x86 instructions; on other platforms the abstraction falls back to the OS and
jitter sources.

### 3b. Health tests (`health_tests.h`)

NIST SP 800-90B continuous health tests: the Repetition Count Test (RCT), the
Adaptive Proportion Test (APT), and startup tests.

```c
typedef enum {
    HEALTH_SUCCESS = 0,
    HEALTH_ERROR_RCT_FAILURE = -1, HEALTH_ERROR_APT_FAILURE = -2,
    HEALTH_ERROR_STARTUP_FAILURE = -3, HEALTH_ERROR_INVALID_PARAM = -4,
    HEALTH_ERROR_NOT_INITIALIZED = -5
} health_error_t;
```

`health_test_config_t` holds `rct_cutoff`, `apt_cutoff`, `apt_window_size`,
`startup_test_samples`, and `min_entropy_estimate`. Defaults follow NIST
SP 800-90B Section 4 (for H_min ≥ 4 bits/sample: RCT cutoff 31, APT cutoff 354,
window 512).

```c
health_error_t health_tests_init(health_test_ctx_t *ctx);
health_error_t health_tests_init_custom(health_test_ctx_t *ctx,
                                         const health_test_config_t *config);
void           health_tests_free(health_test_ctx_t *ctx);
void           health_tests_reset(health_test_ctx_t *ctx);

health_error_t health_tests_startup(health_test_ctx_t *ctx,
                                    const uint8_t *samples, size_t num_samples);
health_error_t health_tests_run(health_test_ctx_t *ctx, uint8_t sample);
health_error_t health_tests_run_batch(health_test_ctx_t *ctx,
                                       const uint8_t *samples, size_t num_samples);

health_error_t health_test_rct(health_test_ctx_t *ctx, uint8_t sample);
health_error_t health_test_apt(health_test_ctx_t *ctx, uint8_t sample);

uint32_t health_calculate_rct_cutoff(double min_entropy);
uint32_t health_calculate_apt_cutoff(double min_entropy, uint32_t window_size);
void     health_get_recommended_config(double min_entropy, health_test_config_t *config);

health_test_stats_t health_tests_get_stats(const health_test_ctx_t *ctx);
int  health_tests_startup_complete(const health_test_ctx_t *ctx);
void health_tests_print_stats(const health_test_ctx_t *ctx);
void health_tests_set_callback(health_test_ctx_t *ctx,
                               void (*callback)(health_error_t, void *), void *user_data);
void health_tests_set_enabled(health_test_ctx_t *ctx, int enabled);

const char* health_error_string(health_error_t error);
int         health_validate_config(const health_test_config_t *config);
```

`health_tests_run()` is called per sample during operation; `health_tests_run_batch()`
tests an array and stops at the first failure. Startup tests must pass before
normal operation is permitted.

### 3c. Quantum state (`quantum_state.h`)

Full state-vector simulation used by the v3 engine, the Bell test, and Grover
search.

```c
typedef double _Complex complex_t;

#define MAX_QUBITS             32                 // 2^32 states
#define MAX_STATE_DIM          (1ULL << MAX_QUBITS)
#define RECOMMENDED_MAX_QUBITS 28                 // speed/memory sweet spot

typedef enum {
    QS_SUCCESS = 0,
    QS_ERROR_INVALID_QUBIT = -1, QS_ERROR_INVALID_STATE = -2,
    QS_ERROR_NOT_NORMALIZED = -3, QS_ERROR_OUT_OF_MEMORY = -4,
    QS_ERROR_INVALID_DIMENSION = -5
} qs_error_t;
```

`quantum_state_t` stores the complex amplitude vector and derived quantities
(global phase, entanglement entropy, purity, fidelity) plus measurement
history.

```c
qs_error_t quantum_state_init(quantum_state_t *state, size_t num_qubits);
void       quantum_state_free(quantum_state_t *state);
qs_error_t quantum_state_from_amplitudes(quantum_state_t *state,
                                         const complex_t *amplitudes, size_t dim);
qs_error_t quantum_state_clone(quantum_state_t *dest, const quantum_state_t *src);
void       quantum_state_reset(quantum_state_t *state);

int        quantum_state_is_normalized(const quantum_state_t *state, double tolerance);
qs_error_t quantum_state_normalize(quantum_state_t *state);
double     quantum_state_entropy(const quantum_state_t *state);   // von Neumann, bits
double     quantum_state_purity(const quantum_state_t *state);    // Tr(rho^2)
double     quantum_state_fidelity(const quantum_state_t *s1, const quantum_state_t *s2);
complex_t  quantum_state_get_amplitude(const quantum_state_t *state, uint64_t basis_index);
double     quantum_state_get_probability(const quantum_state_t *state, uint64_t basis_index);

double     quantum_state_entanglement_entropy(const quantum_state_t *state,
                                              const int *qubits_subsystem_a, size_t num_qubits_a);
qs_error_t quantum_state_partial_trace(const quantum_state_t *state,
                                       const int *qubits_to_trace, size_t num_traced,
                                       complex_t *reduced_density);

void   quantum_state_record_measurement(quantum_state_t *state, uint64_t outcome);
size_t quantum_state_get_measurement_history(const quantum_state_t *state,
                                             uint64_t *outcomes, size_t max_outcomes);
void   quantum_state_clear_measurements(quantum_state_t *state);

void quantum_state_print(const quantum_state_t *state, size_t max_terms);
void quantum_basis_state_string(uint64_t basis_index, size_t num_qubits,
                                char *buffer, size_t buffer_size);
```

Memory scales as `16 * 2^num_qubits` bytes for the amplitude vector: 28 qubits
is ≈4.3 GB, 32 qubits ≈68.7 GB. `RECOMMENDED_MAX_QUBITS` (28) is the
speed/memory sweet spot.

### 3d. Bell test (`bell_test.h`)

CHSH inequality testing to verify genuine quantum behavior. Classical systems
satisfy CHSH ≤ 2; quantum systems can reach the Tsirelson bound 2√2 ≈ 2.828.

```c
typedef struct {
    double angle_a1, angle_a2, angle_b1, angle_b2;  // measurement bases
} bell_measurement_settings_t;

typedef enum { BELL_PHI_PLUS, BELL_PHI_MINUS, BELL_PSI_PLUS, BELL_PSI_MINUS }
    bell_state_type_t;

// bell_test_result_t reports chsh_value, the four correlations, classical/quantum
// bounds, p_value, standard_error, measurement counts, and boolean verdicts
// (violates_classical, confirms_quantum, statistically_significant).
```

```c
qs_error_t create_bell_state_phi_plus (quantum_state_t *s, int q1, int q2);
qs_error_t create_bell_state_phi_minus(quantum_state_t *s, int q1, int q2);
qs_error_t create_bell_state_psi_plus (quantum_state_t *s, int q1, int q2);
qs_error_t create_bell_state_psi_minus(quantum_state_t *s, int q1, int q2);
qs_error_t create_bell_state(quantum_state_t *s, int q1, int q2, bell_state_type_t type);

double measure_correlation(quantum_state_t *state, int qubit_a, int qubit_b,
                           double angle_a, double angle_b, size_t num_samples,
                           quantum_entropy_ctx_t *entropy);
double calculate_chsh_parameter(const double correlations[4]);

bell_test_result_t bell_test_chsh(quantum_state_t *state, int qubit_a, int qubit_b,
                                  size_t num_measurements,
                                  const bell_measurement_settings_t *settings,
                                  quantum_entropy_ctx_t *entropy);
void   bell_get_optimal_settings(bell_measurement_settings_t *settings);
int    bell_test_confirms_quantum(const bell_test_result_t *result);
void   bell_test_print_results(const bell_test_result_t *result);
double bell_theoretical_chsh(bell_state_type_t state_type);

// Continuous monitoring
int  bell_monitor_init(bell_test_monitor_t *monitor, size_t capacity);
void bell_monitor_add_result(bell_test_monitor_t *monitor, const bell_test_result_t *result);
void bell_monitor_get_statistics(const bell_test_monitor_t *monitor);
void bell_monitor_free(bell_test_monitor_t *monitor);
```

`bell_get_optimal_settings()` returns the angles (a1=0, a2=π/2, b1=π/4,
b2=−π/4) that maximize the violation. Pass `settings = NULL` to `bell_test_chsh()`
to use them automatically. Measurements consume cryptographically secure
entropy through the supplied `quantum_entropy_ctx_t`.

### 3e. Grover search (`grover.h`)

Grover's algorithm — O(√N) unstructured search — used for quantum-enhanced
sampling.

```c
typedef struct {
    size_t   num_qubits;             // search space = 2^n
    uint64_t marked_state;           // state to find
    size_t   num_iterations;         // ~pi*sqrt(N)/4
    int      use_optimal_iterations; // auto-compute iterations
} grover_config_t;

// grover_result_t reports found_state, success_probability, oracle_calls,
// iterations_performed, fidelity, and found_marked_state.
```

```c
grover_result_t grover_search(quantum_state_t *state, const grover_config_t *config,
                              quantum_entropy_ctx_t *entropy);
size_t          grover_optimal_iterations(size_t num_qubits);
qs_error_t      grover_oracle(quantum_state_t *state, uint64_t marked_state);
qs_error_t      grover_diffusion(quantum_state_t *state);
qs_error_t      grover_iteration(quantum_state_t *state, uint64_t marked_state);

uint64_t   grover_random_sample(quantum_state_t *state, size_t num_qubits,
                                quantum_entropy_ctx_t *entropy);
qs_error_t grover_random_samples(quantum_state_t *state, size_t num_qubits,
                                 uint64_t *samples, size_t num_samples,
                                 quantum_entropy_ctx_t *entropy);

grover_analysis_t grover_analyze_performance(size_t num_qubits, size_t num_trials,
                                             quantum_entropy_ctx_t *entropy);
void grover_print_result(const grover_result_t *result, const grover_config_t *config);

// V3 enhancements
grover_result_t grover_adaptive_search(quantum_state_t *state, uint64_t marked_state,
                                       quantum_entropy_ctx_t *entropy);
qs_error_t grover_oracle_multi_phase(quantum_state_t *state, const uint64_t *marked_states,
                                     const double *phases, size_t num_marked);
qs_error_t grover_amplitude_amplification(quantum_state_t *state,
                                          const double *target_amplitudes, size_t num_iterations);
qs_error_t grover_importance_sampling(quantum_state_t *state,
                                      double (*importance_function)(uint64_t),
                                      size_t num_samples, uint64_t *samples,
                                      quantum_entropy_ctx_t *entropy);
uint64_t grover_mcmc_step(quantum_state_t *state, double (*target_distribution)(uint64_t),
                          uint64_t current_state, quantum_entropy_ctx_t *entropy);
```

All measuring functions take a `quantum_entropy_ctx_t *` so that the final
measurement uses cryptographically secure randomness rather than a predictable
source such as `rand()`.

---

The full quantum-simulation toolkit and the continued Bell-verified RNG live in
Moonlab (https://github.com/tsotchke/moonlab).
</content>

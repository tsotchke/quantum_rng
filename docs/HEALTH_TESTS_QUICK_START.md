# Health Tests Quick Start Guide

## Quick Reference

### Build and Test

```bash
# Build and run health tests
make test_health

# Should see:
# ✓ ALL TESTS PASSED - NIST SP 800-90B compliant
```

### Basic Usage (5 Minutes)

```c
#include "src/health/health_tests.h"

// 1. Initialize
health_test_ctx_t ctx;
health_tests_init(&ctx);

// 2. Run startup tests (required before use)
uint8_t startup_samples[2048];
// ... collect entropy into startup_samples ...
health_error_t err = health_tests_startup(&ctx, startup_samples, 2048);
if (err != HEALTH_SUCCESS) {
    fprintf(stderr, "Startup failed: %s\n", health_error_string(err));
    exit(1);
}

// 3. Test samples during operation
uint8_t sample;
// ... get entropy sample ...
err = health_tests_run(&ctx, sample);
if (err != HEALTH_SUCCESS) {
    fprintf(stderr, "Health test failed: %s\n", health_error_string(err));
    // Handle failure
}

// 4. Clean up
health_tests_free(&ctx);
```

### What Gets Tested

- **RCT (Repetition Count Test)**: Detects stuck-at faults
  - Default: Fails after 9 consecutive identical samples

- **APT (Adaptive Proportion Test)**: Detects entropy loss
  - Default: Fails if >71 occurrences in 512-sample window

### Configuration Defaults

```c
// Automatically set for H_min = 4.0 bits/sample
RCT cutoff:       9 repetitions
APT cutoff:       71 occurrences
APT window:       512 samples
Startup samples:  1024 minimum
```

### Custom Configuration

```c
health_test_config_t config = {
    .rct_cutoff = 31,              // More tolerant
    .apt_cutoff = 354,             // More tolerant
    .apt_window_size = 512,        // Standard
    .startup_test_samples = 1024,  // Standard
    .min_entropy_estimate = 6.0    // Higher entropy assumption
};
health_tests_init_custom(&ctx, &config);
```

### Error Handling

```c
health_error_t err = health_tests_run(&ctx, sample);

switch (err) {
    case HEALTH_SUCCESS:
        // All good
        break;

    case HEALTH_ERROR_RCT_FAILURE:
        // Repetitive samples detected
        // Possible stuck entropy source
        break;

    case HEALTH_ERROR_APT_FAILURE:
        // Biased samples detected
        // Entropy quality degraded
        break;

    case HEALTH_ERROR_STARTUP_FAILURE:
        // Startup tests failed
        // Cannot use RNG
        break;

    default:
        fprintf(stderr, "Error: %s\n", health_error_string(err));
        break;
}
```

### Monitoring

```c
// Get statistics
health_test_stats_t stats = health_tests_get_stats(&ctx);
printf("Samples: %lu, Failures: %lu\n",
       stats.samples_tested, stats.total_failures);

// Or print detailed report
health_tests_print_stats(&ctx);
```

### Failure Callbacks

```c
void on_failure(health_error_t error, void *user_data) {
    syslog(LOG_CRIT, "RNG health failure: %s", health_error_string(error));
    // Alert monitoring system, stop operations, etc.
}

health_tests_set_callback(&ctx, on_failure, NULL);
```

## When to Use

✅ **Use health tests when**:
- Building cryptographic applications
- Need FIPS 140-3 compliance
- Require high-reliability RNG
- Working with hardware entropy sources

❌ **Can skip health tests for**:
- Non-security applications (games, simulations)
- Performance-critical non-crypto code
- When using pre-tested entropy sources

## Common Patterns

### Pattern 1: One-time initialization

```c
health_test_ctx_t *ctx = malloc(sizeof(health_test_ctx_t));
health_tests_init(ctx);

// Run startup once
run_startup_tests(ctx);

// Use for entire program lifetime
// ...

health_tests_free(ctx);
free(ctx);
```

### Pattern 2: Per-thread testing

```c
// Thread-local storage
__thread health_test_ctx_t thread_health_ctx;

void thread_init(void) {
    health_tests_init(&thread_health_ctx);
    // Run startup for this thread
}

uint64_t thread_safe_random(void) {
    uint8_t entropy[8];
    get_entropy(entropy, 8);

    for (int i = 0; i < 8; i++) {
        health_tests_run(&thread_health_ctx, entropy[i]);
    }

    return bytes_to_uint64(entropy);
}
```

### Pattern 3: Batched testing

```c
// More efficient for bulk operations
uint8_t samples[1000];
get_entropy(samples, 1000);

if (health_tests_run_batch(&ctx, samples, 1000) == HEALTH_SUCCESS) {
    // All samples passed, safe to use
    process_samples(samples, 1000);
}
```

## Performance Tips

1. **Batch when possible**: Use `health_tests_run_batch()` for multiple samples
2. **Reuse context**: Initialize once, use many times
3. **Thread-local**: Use separate context per thread to avoid locks
4. **Monitor overhead**: Health tests add ~10-20 cycles per sample

## Troubleshooting

### "RCT failures"
→ Entropy source may be stuck or malfunctioning
→ Check hardware, verify entropy source initialization

### "APT failures"
→ Entropy source producing biased output
→ Verify entropy quality, check min-entropy estimate

### "Startup failures"
→ Entropy source not working at initialization
→ Check that entropy source is properly initialized before health tests

### "All zeros/ones"
→ Entropy source not initialized
→ Initialize entropy source before running health tests

## Next Steps

1. Read full documentation: `docs/HEALTH_TESTS.md`
2. Review test suite: `tests/health_tests_test.c`
3. Check NIST SP 800-90B: https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf
4. Integrate with your entropy source
5. Run entropy assessment to validate min-entropy estimate

## Support

- GitHub Issues: Report problems or ask questions
- Test Suite: Run `make test_health` to verify implementation
- Documentation: See `docs/HEALTH_TESTS.md` for complete guide

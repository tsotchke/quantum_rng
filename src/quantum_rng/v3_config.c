/*
 * v3_config.c - default configuration for Quantum RNG v3.0
 */
#include "quantum_rng_v3.h"

void qrng_v3_get_default_config(qrng_v3_config_t *config) {
    if (!config) return;
    
    // Quantum simulation defaults
    config->num_qubits = 8;  // 256-dimensional state space
    config->mode = QRNG_V3_MODE_DIRECT;
    
    // Bell test monitoring (disable for benchmarks, enable for production)
    config->enable_bell_monitoring = 0;  // Disabled by default for performance
    config->bell_test_interval = 1024 * 1024;  // Test every 1MB when enabled
    // Alert threshold for the monitored CHSH value. A healthy source produces
    // ~2.83 (Tsirelson) and the classical bound is 2.0, so 2.1 flags a genuine
    // collapse toward classical while leaving ample margin for the sampling
    // noise of a finite-measurement CHSH estimate (avoids spurious failures).
    config->min_acceptable_chsh = 2.1;
    
    // Grover optimization
    config->enable_grover_cache = 1;
    config->grover_cache_size = 256;
    
    // Entropy pool
    config->entropy_pool_size = 64 * 1024;  // 64KB pool
    config->enable_background_entropy = 1;
    
    // Output buffer (larger = fewer refills = better performance)
    config->output_buffer_size = 64 * 1024;  // 64KB buffer
    
    // Performance
    config->enable_simd = 1;
    config->enable_performance_monitoring = 0;  // Disabled by default (overhead)
}

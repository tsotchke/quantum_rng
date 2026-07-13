/*
 * v3_utils.c - utility functions and backward compatibility
 *
 * Version / error / mode string tables and the legacy seed-based init.
 */
#include "quantum_rng_v3.h"
#include "quantum_constants.h"
#include "../common/validation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

const char* qrng_v3_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             QUANTUM_RNG_V3_VERSION_MAJOR,
             QUANTUM_RNG_V3_VERSION_MINOR,
             QUANTUM_RNG_V3_VERSION_PATCH);
    return version;
}

const char* qrng_v3_error_string(qrng_v3_error_t error) {
    switch (error) {
        case QRNG_V3_SUCCESS:
            return "Success";
        case QRNG_V3_ERROR_NULL_CONTEXT:
            return "NULL context provided";
        case QRNG_V3_ERROR_NULL_BUFFER:
            return "NULL buffer provided";
        case QRNG_V3_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case QRNG_V3_ERROR_ENTROPY_FAILURE:
            return "Entropy generation failed";
        case QRNG_V3_ERROR_QUANTUM_INIT:
            return "Quantum state initialization failed";
        case QRNG_V3_ERROR_BELL_TEST_FAILED:
            return "Bell test verification failed - quantum behavior not confirmed";
        case QRNG_V3_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case QRNG_V3_ERROR_NOT_INITIALIZED:
            return "Context not initialized";
        default:
            return "Unknown error";
    }
}

const char* qrng_v3_mode_string(qrng_v3_mode_t mode) {
    switch (mode) {
        case QRNG_V3_MODE_DIRECT:
            return "DIRECT (quantum measurements)";
        case QRNG_V3_MODE_GROVER:
            return "GROVER (quantum search sampling)";
        case QRNG_V3_MODE_BELL_VERIFIED:
            return "BELL_VERIFIED (continuous verification)";
        default:
            return "Unknown mode";
    }
}

// ============================================================================
// BACKWARD COMPATIBILITY
// ============================================================================

qrng_v3_error_t qrng_v3_init_from_seed(
    qrng_v3_ctx_t **ctx,
    const uint8_t *seed,
    size_t seed_len
) {
    // Initialize with default config
    qrng_v3_error_t err = qrng_v3_init(ctx);
    if (err != QRNG_V3_SUCCESS) return err;
    
    // Use seed to initialize quantum state if provided
    if (seed && seed_len > 0) {
        // Apply seed-based gates to quantum state
        for (size_t i = 0; i < seed_len && i < (*ctx)->config.num_qubits; i++) {
            double angle = (seed[i] / 255.0) * QC_2PI;
            gate_ry((*ctx)->quantum_state, i % (*ctx)->config.num_qubits, angle);
        }
    }
    
    return QRNG_V3_SUCCESS;
}

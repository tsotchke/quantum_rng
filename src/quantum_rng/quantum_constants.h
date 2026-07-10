#ifndef QUANTUM_CONSTANTS_H
#define QUANTUM_CONSTANTS_H

#include <stdint.h>

/**
 * @file quantum_constants.h
 * @brief High-precision mathematical and physical constants
 * 
 * All constants in IEEE 754 double precision hexadecimal format
 * for maximum precision and reproducibility across platforms.
 */

// ============================================================================
// MATHEMATICAL CONSTANTS
// ============================================================================

// π and related
#define QC_PI_HEX              0x400921FB54442D18ULL  // π
#define QC_PI_2_HEX            0x3FF921FB54442D18ULL  // π/2
#define QC_PI_4_HEX            0x3FE921FB54442D18ULL  // π/4
#define QC_2PI_HEX             0x401921FB54442D18ULL  // 2π
#define QC_3PI_4_HEX           0x4002D97C7F3321D2ULL  // 3π/4

// √2 and related
#define QC_SQRT2_HEX           0x3FF6A09E667F3BCDULL  // √2
#define QC_SQRT2_INV_HEX       0x3FE6A09E667F3BCDULL  // 1/√2
#define QC_2SQRT2_HEX          0x4006A09E667F3BCDULL  // 2√2 ≈ 2.8284271 (Tsirelson bound)

// √3 and related
#define QC_SQRT3_HEX           0x3FFBB67AE8584CAAULL  // √3
#define QC_SQRT3_INV_HEX       0x3FE279A74590331CULL  // 1/√3

// e and related
#define QC_E_HEX               0x4005BF0A8B145769ULL  // e
#define QC_E_INV_HEX           0x3FD78B56362CEF38ULL  // 1/e
#define QC_LN2_HEX             0x3FE62E42FEFA39EFULL  // ln(2)

// ============================================================================
// QUANTUM PHYSICS CONSTANTS (in atomic units)
// ============================================================================

// Fine structure constant α ≈ 1/137
#define QC_FINE_STRUCTURE_HEX  0x3F747AE147AE147BULL  
// Planck constant ħ (in reduced units)
#define QC_PLANCK_HEX          0x3FF0000000000000ULL  
// Rydberg constant
#define QC_RYDBERG_HEX         0x9E3779B97F4A7C15ULL  
// Electron g-factor
#define QC_ELECTRON_G_HEX      0x2B992DDFA232945ULL   
// Golden ratio φ
#define QC_GOLDEN_RATIO_HEX    0x3FF9E3779B97F4A8ULL  

// Quantum mixing constants (derived from physical constants)
#define QC_HEISENBERG_HEX      0xC13FA9A902A6328FULL
#define QC_SCHRODINGER_HEX     0x91E10DA5C79E7B1DULL
#define QC_PAULI_X_HEX         0x4C957F2D8A1E6B3CULL
#define QC_PAULI_Y_HEX         0xD3E99E3B6C1A4F78ULL
#define QC_PAULI_Z_HEX         0x8F142FC07892A5B6ULL

// ============================================================================
// CONVERSION UTILITIES
// ============================================================================

/**
 * @brief Convert hexadecimal to double (strict aliasing safe)
 * @param hex 64-bit hexadecimal value
 * @return Double precision floating point value
 */
static inline double qc_hex_to_double(uint64_t hex) {
    union { uint64_t u; double d; } converter;
    converter.u = hex;
    return converter.d;
}

// ============================================================================
// CONSTANT ACCESSORS
// ============================================================================

// Mathematical constants
#define QC_PI           (qc_hex_to_double(QC_PI_HEX))
#define QC_PI_2         (qc_hex_to_double(QC_PI_2_HEX))
#define QC_PI_4         (qc_hex_to_double(QC_PI_4_HEX))
#define QC_2PI          (qc_hex_to_double(QC_2PI_HEX))
#define QC_3PI_4        (qc_hex_to_double(QC_3PI_4_HEX))

#define QC_SQRT2        (qc_hex_to_double(QC_SQRT2_HEX))
#define QC_SQRT2_INV    (qc_hex_to_double(QC_SQRT2_INV_HEX))
#define QC_2SQRT2       (qc_hex_to_double(QC_2SQRT2_HEX))

#define QC_SQRT3        (qc_hex_to_double(QC_SQRT3_HEX))
#define QC_SQRT3_INV    (qc_hex_to_double(QC_SQRT3_INV_HEX))

#define QC_E            (qc_hex_to_double(QC_E_HEX))
#define QC_E_INV        (qc_hex_to_double(QC_E_INV_HEX))
#define QC_LN2          (qc_hex_to_double(QC_LN2_HEX))

// Physical constants
#define QC_FINE_STRUCTURE  (qc_hex_to_double(QC_FINE_STRUCTURE_HEX))
#define QC_PLANCK          (qc_hex_to_double(QC_PLANCK_HEX))
#define QC_RYDBERG         (qc_hex_to_double(QC_RYDBERG_HEX))
#define QC_ELECTRON_G      (qc_hex_to_double(QC_ELECTRON_G_HEX))
#define QC_GOLDEN_RATIO    (qc_hex_to_double(QC_GOLDEN_RATIO_HEX))

#define QC_HEISENBERG      (qc_hex_to_double(QC_HEISENBERG_HEX))
#define QC_SCHRODINGER     (qc_hex_to_double(QC_SCHRODINGER_HEX))
#define QC_PAULI_X         (qc_hex_to_double(QC_PAULI_X_HEX))
#define QC_PAULI_Y         (qc_hex_to_double(QC_PAULI_Y_HEX))
#define QC_PAULI_Z         (qc_hex_to_double(QC_PAULI_Z_HEX))

// ============================================================================
// COMMON QUANTUM CIRCUIT CONSTANTS
// ============================================================================

// Common rotation angles
#define QC_PI_8         (QC_PI / 8.0)      // π/8 (T gate angle)
#define QC_PI_16        (QC_PI / 16.0)     // π/16

// Tsirelson bound for CHSH inequality
#define QC_TSIRELSON_BOUND  QC_2SQRT2      // 2√2 ≈ 2.828

// Maximum entanglement entropy (1 ebit)
#define QC_MAX_EBIT     1.0

#endif /* QUANTUM_CONSTANTS_H */
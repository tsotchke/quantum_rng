#ifndef QUANTUM_CONSTANTS_H
#define QUANTUM_CONSTANTS_H

// Physical constants in high-precision hex representation
#define QRNG_FINE_STRUCTURE   0x7297352743776A1BULL // α
#define QRNG_PLANCK          0x6955927086495225ULL  // ħ
#define QRNG_RYDBERG         0x9E3779B97F4A7C15ULL  // R∞
#define QRNG_ELECTRON_G      0x2B992DDFA232945ULL   // ge
#define QRNG_BOLTZMANN       0xB5A2003DA180BD03ULL  // kB

// Quantum circuit parameters
#define QRNG_MAX_QUBITS      32
#define QRNG_MIN_QUBITS      4
#define QRNG_DEFAULT_QUBITS  8

// Operation modes
#define QRNG_MODE_FAST       0x01
#define QRNG_MODE_SECURE     0x02
#define QRNG_MODE_BALANCED   0x03

// Statistical test thresholds
#define QRNG_MIN_ENTROPY     63.9
#define QRNG_CHI_THRESHOLD   255.0
#define QRNG_PVALUE_THRESH   0.01

#endif /* QUANTUM_CONSTANTS_H */
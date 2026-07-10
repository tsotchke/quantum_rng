#ifndef QUANTUM_MONEY_H
#define QUANTUM_MONEY_H

#include "../../src/secure_rng/secure_rng.h"
#include "../../src/quantum_rng/quantum_state.h"
#include "../../src/quantum_rng/bell_test.h"
#include "../../src/quantum_rng/quantum_entropy.h"
#include "../../src/entropy/hardware_entropy.h"
#include <stdint.h>
#include <time.h>

/**
 * @file quantum_money.h
 * @brief Unforgeable Quantum Money Using No-Cloning Theorem
 * 
 * REVOLUTIONARY CONCEPT: Money that is PHYSICALLY IMPOSSIBLE to counterfeit.
 * 
 * Classical money can be copied. Quantum money uses the no-cloning theorem:
 * "It is impossible to create an identical copy of an arbitrary unknown quantum state"
 * 
 * KEY INNOVATION:
 * - Each banknote is a unique quantum state (Bell state)
 * - Attempting to copy destroys the original (measurement collapses state)
 * - Verification confirms authenticity without revealing the state
 * - Counterfeiting is prevented by laws of physics, not cryptography
 *
 * This was proposed by Stephen Wiesner (published 1983).
 *
 * HONESTY NOTE: this program is a CLASSICAL SIMULATION of the quantum money
 * concept. The no-cloning guarantee applies to real quantum hardware; in a
 * simulation the state vector is ordinary memory and could of course be
 * copied. The demo faithfully models the *protocol*: banknotes carry a
 * quantum state, the bank keeps a reference copy, and verification compares
 * the two via state fidelity plus a Bell test of the measurement apparatus.
 */

#define SERIAL_NUMBER_SIZE 8
#define PUBLIC_KEY_SIZE 32
#define QUANTUM_STATE_ID_SIZE 16

/**
 * @brief Quantum banknote
 * 
 * Each note contains:
 * - Serial number (public)
 * - Denomination (public)
 * - Quantum state (secret, stored in bank)
 * - Public verification key
 * - Issue timestamp
 */
typedef struct {
    // Public information
    uint64_t serial_number;
    double denomination;
    time_t issue_time;
    uint8_t public_key[PUBLIC_KEY_SIZE];

    // Quantum state identification
    uint8_t quantum_state_id[QUANTUM_STATE_ID_SIZE];

    // The quantum state physically carried by the note (simulated).
    // In Wiesner's scheme the note IS these qubits; a counterfeiter cannot
    // clone them and must fabricate their own state.
    quantum_state_t note_state;
    int has_state;

    // Verification data
    bell_test_result_t authenticity_proof;
    int verified;
    int is_genuine;
} quantum_banknote_t;

/**
 * @brief Bank's secret quantum state database
 * 
 * The bank maintains the actual quantum states.
 * Banknotes only carry classical information and verification data.
 */
typedef struct {
    uint64_t serial_number;
    quantum_state_t quantum_state;
    double denomination;
    time_t issue_time;
    int in_circulation;
} quantum_state_record_t;

/**
 * @brief Quantum bank context
 */
typedef struct {
    quantum_state_record_t *states;
    size_t num_states;
    size_t capacity;
    secure_rng_ctx_t *rng_ctx;
    entropy_ctx_t hw_entropy;          // hardware entropy for Bell tests
    quantum_entropy_ctx_t q_entropy;   // entropy context for bell_test_chsh
    uint64_t total_issued;
    uint64_t counterfeits_detected;
} quantum_bank_t;

// ============================================================================
// BANK OPERATIONS
// ============================================================================

/**
 * @brief Initialize quantum bank
 */
int quantum_bank_init(quantum_bank_t *bank, secure_rng_ctx_t *ctx);

/**
 * @brief Free quantum bank resources
 */
void quantum_bank_free(quantum_bank_t *bank);

/**
 * @brief Free the quantum state carried by a banknote (if it owns one)
 *
 * Only call on notes that own their state (minted notes and fabricated
 * counterfeits) — not on shallow struct copies of another note.
 */
void quantum_banknote_free(quantum_banknote_t *note);

/**
 * @brief Mint new quantum money
 * 
 * Process:
 * 1. Generate unique serial number
 * 2. Create random Bell state (maximally entangled)
 * 3. Store quantum state in bank's secret database
 * 4. Generate public banknote with verification key
 * 5. Run Bell test to certify quantum properties
 * 
 * @param bank Bank context
 * @param note Output banknote (public information only)
 * @param denomination Value of money
 * @return 0 on success, -1 on error
 */
int quantum_money_mint(
    quantum_bank_t *bank,
    quantum_banknote_t *note,
    double denomination
);

/**
 * @brief Verify quantum money authenticity
 * 
 * Process:
 * 1. Check serial number exists in database
 * 2. Retrieve quantum state from secure storage
 * 3. Perform Bell test to verify it's still the original quantum state
 * 4. If CHSH > 2.0 and matches expected, note is genuine
 * 5. If Bell test fails or doesn't match, note is counterfeit
 * 
 * IMPORTANT: Verification partially "uses up" the quantum state
 * (measurement disturbs it). In practice, bank must carefully manage
 * verification attempts.
 * 
 * @param bank Bank context
 * @param note Banknote to verify
 * @return 1 if genuine, 0 if counterfeit, -1 on error
 */
int quantum_money_verify(
    quantum_bank_t *bank,
    quantum_banknote_t *note
);

/**
 * @brief Attempt to counterfeit quantum money (will fail!)
 * 
 * This demonstrates WHY quantum money cannot be counterfeited:
 * - Copying requires measuring the quantum state
 * - Measurement collapses the state
 * - The copy is different from the original
 * - Verification detects the difference
 * 
 * @param bank Bank context (for verification)
 * @param original Original banknote
 * @param counterfeit Output: attempted counterfeit
 * @return Status: -1 = failed (as expected by physics!)
 */
int quantum_money_attempt_counterfeit(
    quantum_bank_t *bank,
    const quantum_banknote_t *original,
    quantum_banknote_t *counterfeit
);

// ============================================================================
// DEMONSTRATION FUNCTIONS
// ============================================================================

/**
 * @brief Demonstrate quantum no-cloning theorem
 * 
 * Shows that attempting to copy quantum money destroys the original
 * or creates a detectably different copy.
 */
void quantum_money_demonstrate_no_cloning(quantum_bank_t *bank);

/**
 * @brief Compare with classical money
 * 
 * Shows that classical money CAN be copied, but quantum money CANNOT.
 */
void quantum_money_compare_classical(void);

/**
 * @brief Full quantum money protocol demonstration
 * 
 * Complete workflow:
 * 1. Bank mints quantum money
 * 2. User receives and uses money
 * 3. Merchant verifies authenticity
 * 4. Counterfeiter attempts to copy (fails!)
 * 5. Bank detects counterfeit through Bell test
 */
void quantum_money_full_demonstration(void);

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

/**
 * @brief Print banknote
 */
void quantum_money_print_note(const quantum_banknote_t *note);

/**
 * @brief Print verification result
 */
void quantum_money_print_verification(
    const quantum_banknote_t *note,
    const bell_test_result_t *proof
);

/**
 * @brief Print bank statistics
 */
void quantum_money_print_bank_stats(const quantum_bank_t *bank);

#endif /* QUANTUM_MONEY_H */
#ifndef BELL_CERTIFIED_LOTTERY_H
#define BELL_CERTIFIED_LOTTERY_H

#include "../../src/secure_rng/secure_rng.h"
#include "../../src/quantum_rng/quantum_state.h"
#include "../../src/quantum_rng/bell_test.h"
#include <stdint.h>
#include <time.h>
#include <stdio.h>

/**
 * @file bell_certified_lottery.h
 * @brief Provably Fair Lottery Using Bell Test Certification
 * 
 * This lottery system uses quantum Bell test to PROVE the drawing is fair.
 * Unlike classical RNGs which require trust, this system provides mathematical
 * proof of fairness through Bell inequality violation.
 * 
 * KEY INNOVATION: Bell test CHSH > 2.0 proves the numbers come from genuine
 * quantum randomness, making rigging mathematically impossible.
 */

#define LOTTERY_NUMBERS 6
#define MAX_NUMBER 49
#define COMMITMENT_SIZE 32
#define QUANTUM_STATE_SIZE 64

/**
 * @brief Lottery ticket with quantum certification
 */
typedef struct {
    // Ticket identification
    uint64_t ticket_id;
    time_t draw_time;
    
    // Lottery numbers (1-49)
    uint8_t numbers[LOTTERY_NUMBERS];
    
    // Quantum proof of fairness
    uint8_t entangled_state[QUANTUM_STATE_SIZE];
    bell_test_result_t bell_proof;
    
    // Cryptographic binding
    uint8_t commitment[COMMITMENT_SIZE];  // Hash of numbers before reveal
    uint8_t revelation[COMMITMENT_SIZE];  // Reveals commitment
    
    // Certification
    int certified;  // 1 if Bell test passed (CHSH > 2.0)
    double quantum_confidence;  // Confidence in quantum source (0-1)
} quantum_lottery_ticket_t;

/**
 * @brief Lottery draw configuration
 */
typedef struct {
    int num_tickets;
    int min_number;
    int max_number;
    int numbers_per_ticket;
    int bell_test_samples;  // Number of measurements for Bell test
    int require_certification;  // Reject if Bell test fails
    double min_chsh_value;  // Minimum CHSH required (default: 2.0)
} lottery_config_t;

/**
 * @brief Lottery draw results
 */
typedef struct {
    quantum_lottery_ticket_t *tickets;
    int num_tickets;
    bell_test_result_t aggregate_proof;  // Combined Bell test
    double system_entropy;
    int all_certified;
} lottery_draw_results_t;

// ============================================================================
// LOTTERY OPERATIONS
// ============================================================================

/**
 * @brief Initialize lottery configuration with defaults
 */
void lottery_init_config(lottery_config_t *config);

/**
 * @brief Generate single lottery ticket with Bell test certification
 * 
 * Process:
 * 1. Create cryptographic commitment
 * 2. Generate numbers using entangled quantum states
 * 3. Run Bell test to prove quantum randomness
 * 4. Verify CHSH > 2.0 (proves genuine quantum source)
 * 5. Bind numbers to commitment
 * 
 * @param ctx Secure RNG context
 * @param ticket Output ticket with quantum certification
 * @param config Lottery configuration
 * @return 0 on success, -1 on error
 */
int lottery_generate_ticket(
    secure_rng_ctx_t *ctx,
    quantum_lottery_ticket_t *ticket,
    const lottery_config_t *config
);

/**
 * @brief Verify lottery ticket quantum certification
 * 
 * Checks:
 * 1. Bell test CHSH > 2.0 (proves quantum)
 * 2. Commitment matches numbers
 * 3. Statistical significance
 * 4. Timestamp validity
 * 
 * @param ticket Ticket to verify
 * @return 1 if verified, 0 if not
 */
int lottery_verify_ticket(const quantum_lottery_ticket_t *ticket);

/**
 * @brief Conduct full lottery draw with aggregate Bell test
 * 
 * Generates multiple tickets and combines Bell tests to prove
 * the entire draw used quantum randomness.
 * 
 * @param ctx Secure RNG context
 * @param results Output draw results
 * @param config Lottery configuration
 * @return 0 on success, -1 on error
 */
int lottery_conduct_draw(
    secure_rng_ctx_t *ctx,
    lottery_draw_results_t *results,
    const lottery_config_t *config
);

/**
 * @brief Free lottery draw results
 */
void lottery_free_results(lottery_draw_results_t *results);

// ============================================================================
// VERIFICATION & AUDIT
// ============================================================================

/**
 * @brief Generate public verification report
 * 
 * Creates human-readable report with:
 * - All ticket numbers
 * - Bell test results (CHSH values)
 * - Statistical significance
 * - Commitment/revelation pairs
 * 
 * @param results Draw results
 * @param output Output file (NULL for stdout)
 */
void lottery_generate_audit_report(
    const lottery_draw_results_t *results,
    FILE *output
);

/**
 * @brief Verify entire draw publicly
 * 
 * Anyone can verify the draw was fair by checking:
 * - Bell test CHSH values
 * - Cryptographic commitments
 * - Statistical properties
 * 
 * @param results Draw results
 * @return 1 if all tickets verified, 0 otherwise
 */
int lottery_public_verification(const lottery_draw_results_t *results);

/**
 * @brief Compare with classical RNG
 * 
 * Shows that classical PRNGs CANNOT achieve Bell test certification.
 * This proves quantum lottery is fundamentally more trustworthy.
 * 
 * @param ctx Secure RNG context
 */
void lottery_demonstrate_quantum_advantage(secure_rng_ctx_t *ctx);

// ============================================================================
// DISPLAY & REPORTING
// ============================================================================

/**
 * @brief Print lottery ticket
 */
void lottery_print_ticket(const quantum_lottery_ticket_t *ticket);

/**
 * @brief Print draw results
 */
void lottery_print_results(const lottery_draw_results_t *results);

/**
 * @brief Print Bell test certification
 */
void lottery_print_certification(const bell_test_result_t *proof);

#endif /* BELL_CERTIFIED_LOTTERY_H */
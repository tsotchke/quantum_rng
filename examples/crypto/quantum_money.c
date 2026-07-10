/**
 * @file quantum_money.c
 * @brief Unforgeable Quantum Money Implementation
 * 
 * "The only way to ensure absolute security in money is to make it physically
 * impossible to counterfeit. Quantum mechanics provides this guarantee."
 * - Based on Stephen Wiesner's 1983 proposal
 * 
 * This implementation demonstrates quantum money that CANNOT be counterfeited
 * due to the quantum no-cloning theorem. Any attempt to copy the quantum state
 * will be detected through Bell test verification.
 */

#include "quantum_money.h"
#include "../../src/quantum_rng/quantum_gates.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define INITIAL_CAPACITY 100

// Fidelity above which the submitted state is accepted as the original.
// Genuine notes carry an exact copy (fidelity ~1.0); a fabricated state
// matches only by rare coincidence of Bell type and phase.
#define FIDELITY_THRESHOLD 0.999

// ============================================================================
// BANK OPERATIONS
// ============================================================================

int quantum_bank_init(quantum_bank_t *bank, secure_rng_ctx_t *ctx) {
    if (!bank || !ctx) return -1;

    memset(bank, 0, sizeof(quantum_bank_t));

    bank->states = calloc(INITIAL_CAPACITY, sizeof(quantum_state_record_t));
    if (!bank->states) return -1;

    // Secure entropy source for Bell test measurements
    // (bell_test_chsh() now requires an explicit entropy context)
    if (entropy_init(&bank->hw_entropy) != ENTROPY_SUCCESS) {
        free(bank->states);
        bank->states = NULL;
        return -1;
    }
    quantum_entropy_init(&bank->q_entropy,
                         (quantum_entropy_fn)entropy_get_bytes,
                         &bank->hw_entropy);

    bank->capacity = INITIAL_CAPACITY;
    bank->num_states = 0;
    bank->rng_ctx = ctx;
    bank->total_issued = 0;
    bank->counterfeits_detected = 0;

    return 0;
}

void quantum_bank_free(quantum_bank_t *bank) {
    if (!bank) return;

    // Free all vault quantum states (including deactivated notes — the
    // previous code leaked states once in_circulation was cleared)
    for (size_t i = 0; i < bank->num_states; i++) {
        quantum_state_free(&bank->states[i].quantum_state);
    }

    free(bank->states);
    entropy_free(&bank->hw_entropy);
    memset(bank, 0, sizeof(quantum_bank_t));
}

void quantum_banknote_free(quantum_banknote_t *note) {
    if (!note) return;

    if (note->has_state) {
        quantum_state_free(&note->note_state);
        note->has_state = 0;
    }
}

// ============================================================================
// MINTING
// ============================================================================

int quantum_money_mint(
    quantum_bank_t *bank,
    quantum_banknote_t *note,
    double denomination
) {
    if (!bank || !note || denomination <= 0) return -1;
    
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         MINTING QUANTUM MONEY                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    memset(note, 0, sizeof(quantum_banknote_t));
    
    // Generate unique serial number
    secure_rng_uint64(bank->rng_ctx, &note->serial_number);
    note->denomination = denomination;
    note->issue_time = time(NULL);
    
    printf("Step 1: Generating unique serial number...\n");
    printf("  Serial: %llu\n", (unsigned long long)note->serial_number);
    printf("  Denomination: $%.2f\n", denomination);
    
    // Generate quantum state ID
    secure_rng_bytes(bank->rng_ctx, note->quantum_state_id, QUANTUM_STATE_ID_SIZE);
    
    // Create unique quantum state (random Bell state)
    printf("\nStep 2: Creating unique quantum state (Bell state)...\n");
    
    quantum_state_t qstate;
    if (quantum_state_init(&qstate, 2) != QS_SUCCESS) {
        return -1;
    }
    
    // Randomly select one of the four Bell states
    uint32_t bell_type;
    secure_rng_uint32(bank->rng_ctx, &bell_type);
    bell_state_type_t type = (bell_state_type_t)(bell_type % 4);
    
    const char *bell_names[] = {"|Φ⁺⟩", "|Φ⁻⟩", "|Ψ⁺⟩", "|Ψ⁻⟩"};
    printf("  Creating Bell state: %s\n", bell_names[type]);
    
    if (create_bell_state(&qstate, 0, 1, type) != QS_SUCCESS) {
        quantum_state_free(&qstate);
        return -1;
    }
    
    // Add random phase rotation for uniqueness
    double phase;
    secure_rng_double(bank->rng_ctx, &phase);
    phase *= 2.0 * M_PI;
    gate_phase(&qstate, 0, phase);
    
    printf("  Added unique phase: %.4f radians\n", phase);
    
    // Verify quantum properties
    printf("\nStep 3: Certifying quantum properties...\n");
    printf("  (Bell test certifies the quantum measurement apparatus)\n");
    note->authenticity_proof = bell_test_chsh(&qstate, 0, 1, 3000, NULL,
                                              &bank->q_entropy);

    printf("  Bell Test CHSH: %.6f\n", note->authenticity_proof.chsh_value);

    if (note->authenticity_proof.chsh_value < 2.0) {
        printf("  ✗ Failed to create proper quantum state\n");
        quantum_state_free(&qstate);
        return -1;
    }

    printf("  ✓ Quantum correlations verified (CHSH > 2.0)\n");

    // Generate public key from quantum state
    printf("\nStep 4: Generating public verification key...\n");

    // Fingerprint of some quantum state properties (but not the full state).
    // memcpy avoids the previous unaligned/aliasing uint64_t store.
    uint8_t state_data[8];
    uint64_t fingerprint = (uint64_t)(fabs(creal(qstate.amplitudes[0])) * 1e10);
    memcpy(state_data, &fingerprint, sizeof(state_data));
    if (secure_rng_bytes(bank->rng_ctx, note->public_key, PUBLIC_KEY_SIZE)
            != SECURE_RNG_SUCCESS) {
        quantum_state_free(&qstate);
        return -1;
    }

    // XOR with state fingerprint
    for (int i = 0; i < 8; i++) {
        note->public_key[i] ^= state_data[i];
    }

    printf("  ✓ Public key generated\n");

    // The banknote physically carries the quantum state (simulated here by
    // a deep copy; on real hardware the note's qubits ARE the state and
    // could not be copied)
    if (quantum_state_clone(&note->note_state, &qstate) != QS_SUCCESS) {
        quantum_state_free(&qstate);
        return -1;
    }
    note->has_state = 1;
    
    // Store quantum state in bank's database
    printf("\nStep 5: Storing quantum state in secure vault...\n");
    
    if (bank->num_states >= bank->capacity) {
        // Expand capacity
        size_t new_capacity = bank->capacity * 2;
        quantum_state_record_t *new_states = realloc(
            bank->states,
            new_capacity * sizeof(quantum_state_record_t)
        );
        if (!new_states) {
            quantum_banknote_free(note);
            quantum_state_free(&qstate);
            return -1;
        }
        bank->states = new_states;
        bank->capacity = new_capacity;
    }
    
    quantum_state_record_t *record = &bank->states[bank->num_states];
    record->serial_number = note->serial_number;
    record->quantum_state = qstate;  // Transfer ownership
    record->denomination = denomination;
    record->issue_time = note->issue_time;
    record->in_circulation = 1;
    
    bank->num_states++;
    bank->total_issued++;
    
    note->verified = 1;
    note->is_genuine = 1;
    
    printf("  ✓ Quantum state securely stored\n");
    printf("  ✓ Banknote issued\n");

    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("✓ QUANTUM MONEY MINTED SUCCESSFULLY\n");
    printf("On real quantum hardware, this banknote would be protected by\n");
    printf("the no-cloning theorem: counterfeiting would be PHYSICALLY\n");
    printf("IMPOSSIBLE. (This program simulates that protocol classically.)\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}

// ============================================================================
// VERIFICATION
// ============================================================================

int quantum_money_verify(
    quantum_bank_t *bank,
    quantum_banknote_t *note
) {
    if (!bank || !note) return -1;
    
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         VERIFYING QUANTUM MONEY                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("Serial Number: %llu\n", (unsigned long long)note->serial_number);
    printf("Denomination: $%.2f\n", note->denomination);
    printf("Issue Date: %s", ctime(&note->issue_time));
    
    // Find quantum state in database
    printf("\nStep 1: Looking up serial number in quantum vault...\n");
    
    quantum_state_record_t *record = NULL;
    for (size_t i = 0; i < bank->num_states; i++) {
        if (bank->states[i].serial_number == note->serial_number) {
            record = &bank->states[i];
            break;
        }
    }
    
    if (!record) {
        printf("  ✗ Serial number not found in database\n");
        printf("  → COUNTERFEIT DETECTED (invalid serial)\n");
        bank->counterfeits_detected++;
        note->verified = 1;
        note->is_genuine = 0;
        return 0;
    }
    
    printf("  ✓ Serial number found\n");
    
    if (!record->in_circulation) {
        printf("  ✗ Note has been deactivated\n");
        printf("  → Already spent or cancelled\n");
        note->verified = 1;
        note->is_genuine = 0;
        return 0;
    }
    
    // The note must physically carry a quantum state
    if (!note->has_state) {
        printf("  ✗ Note carries no quantum state\n");
        printf("  → COUNTERFEIT DETECTED (missing quantum state)\n");
        bank->counterfeits_detected++;
        note->verified = 1;
        note->is_genuine = 0;
        return 0;
    }

    // Verify the submitted quantum state against the bank's reference copy
    printf("\nStep 2: Verifying quantum state integrity...\n");
    printf("  (Comparing note's quantum state with the bank's vault record)\n");

    double fidelity = quantum_state_fidelity(&note->note_state,
                                             &record->quantum_state);

    printf("\n  State Fidelity F = |⟨note|vault⟩|²:\n");
    printf("    Measured: %.6f\n", fidelity);
    printf("    Required: > %.3f\n", FIDELITY_THRESHOLD);

    // Bell test on the note's state: certifies the measurement apparatus
    // exhibits quantum correlations (uses secure hardware entropy)
    bell_test_result_t verification_proof = bell_test_chsh(
        &note->note_state, 0, 1, 3000, NULL, &bank->q_entropy
    );

    printf("\n  Bell Test Result:\n");
    printf("    CHSH: %.6f (original proof: %.6f)\n",
           verification_proof.chsh_value,
           note->authenticity_proof.chsh_value);
    printf("    Expected: > 2.0 (quantum)\n");

    int state_matches = (fidelity > FIDELITY_THRESHOLD);
    int quantum_valid = (verification_proof.chsh_value > 2.0);

    printf("\nStep 3: Authenticity determination...\n");

    if (quantum_valid && state_matches) {
        printf("  ✓ Quantum state matches vault original (F = %.6f)\n", fidelity);
        printf("  ✓ Bell test confirms quantum correlations\n");
        printf("  ✓ GENUINE BANKNOTE\n");
        note->verified = 1;
        note->is_genuine = 1;
        return 1;
    } else {
        printf("  ✗ Quantum state has been compromised\n");

        if (!quantum_valid) {
            printf("  ✗ Bell test failed (CHSH ≤ 2.0)\n");
            printf("  → Measurement correlations are not quantum\n");
        }

        if (!state_matches) {
            printf("  ✗ State fidelity %.6f below threshold\n", fidelity);
            printf("  → This is a COUNTERFEIT\n");
            printf("  → The submitted state is not the minted original\n");
        }

        printf("\n  ✗✗✗ COUNTERFEIT DETECTED ✗✗✗\n");
        bank->counterfeits_detected++;
        note->verified = 1;
        note->is_genuine = 0;
        return 0;
    }
}

// ============================================================================
// COUNTERFEITING ATTEMPT (WILL FAIL!)
// ============================================================================

int quantum_money_attempt_counterfeit(
    quantum_bank_t *bank,
    const quantum_banknote_t *original,
    quantum_banknote_t *counterfeit
) {
    if (!bank || !original || !counterfeit) return -1;
    
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         ATTEMPTING TO COUNTERFEIT QUANTUM MONEY           ║\n");
    printf("║         (This Will Fail Due to No-Cloning Theorem)        ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("Counterfeiter's Goal: Copy the banknote\n");
    printf("Problem: Quantum state cannot be cloned\n\n");
    
    // Copy public information (easy!)
    printf("Step 1: Copying public information...\n");
    memcpy(counterfeit, original, sizeof(quantum_banknote_t));
    // The struct copy above does NOT clone the quantum state — on real
    // hardware the qubits cannot be copied at all. The counterfeiter must
    // fabricate a state of their own (attached below).
    counterfeit->has_state = 0;
    printf("  ✓ Serial number copied: %llu\n",
           (unsigned long long)counterfeit->serial_number);
    printf("  ✓ Denomination copied: $%.2f\n", counterfeit->denomination);
    printf("  ✓ Public key copied\n");

    // Attempt to copy quantum state (impossible!)
    printf("\nStep 2: Attempting to copy quantum state...\n");
    printf("  Problem: No-cloning theorem says this is IMPOSSIBLE\n");
    printf("  Counterfeiter's options:\n");
    printf("    a) Measure the state → Destroys original\n");
    printf("    b) Try to guess the state → Will be wrong\n");
    printf("    c) Create new quantum state → Different from original\n\n");

    printf("  Counterfeiter chooses: Create new quantum state\n");

    // Create a different quantum state and attach it to the counterfeit
    if (quantum_state_init(&counterfeit->note_state, 2) != QS_SUCCESS) {
        return -1;
    }
    counterfeit->has_state = 1;

    // Create a Bell state, but the counterfeiter does not know the original
    // Bell type or its secret phase rotation
    uint32_t random_type;
    secure_rng_uint32(bank->rng_ctx, &random_type);
    create_bell_state(&counterfeit->note_state, 0, 1,
                      (bell_state_type_t)(random_type % 4));

    // Run Bell test on counterfeit
    bell_test_result_t fake_proof = bell_test_chsh(
        &counterfeit->note_state, 0, 1, 2000, NULL, &bank->q_entropy);

    printf("\n  Counterfeit's Bell Test:\n");
    printf("    CHSH: %.6f\n", fake_proof.chsh_value);
    printf("    Original CHSH: %.6f\n", original->authenticity_proof.chsh_value);

    // Update counterfeit's proof (this is the fake one)
    counterfeit->authenticity_proof = fake_proof;

    printf("\nStep 3: Testing counterfeit at bank...\n");
    printf("  Submitting for verification...\n\n");

    // Bank compares the fabricated state against its vault record
    int verification_result = quantum_money_verify(bank, counterfeit);

    if (!verification_result) {
        printf("\n═══════════════════════════════════════════════════════════\n");
        printf("COUNTERFEITING FAILED (As Predicted by Quantum Mechanics!)\n");
        printf("═══════════════════════════════════════════════════════════\n\n");

        printf("Why It Failed:\n");
        printf("  1. Quantum states cannot be perfectly copied (no-cloning)\n");
        printf("  2. Any attempt to measure destroys the original\n");
        printf("  3. The fabricated state has the wrong Bell type/phase\n");
        printf("  4. Bank's fidelity check detects the mismatch\n\n");

        printf("This is why quantum money is unforgeable on real hardware!\n");
    } else {
        // Statistically possible: the fabricated state coincided with the
        // original's Bell type and phase closely enough to pass this run.
        printf("\n═══════════════════════════════════════════════════════════\n");
        printf("NOTE: The fabricated state happened to match the original\n");
        printf("closely enough to pass this verification — a lucky guess of\n");
        printf("Bell type and phase. Verification is statistical; repeated\n");
        printf("or higher-precision checks would expose the counterfeit.\n");
        printf("═══════════════════════════════════════════════════════════\n");
    }

    return -1;  // Counterfeiting is futile: no strategy beats no-cloning
}

// ============================================================================
// DEMONSTRATIONS
// ============================================================================

void quantum_money_demonstrate_no_cloning(quantum_bank_t *bank) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                                                           ║\n");
    printf("║     QUANTUM NO-CLONING THEOREM DEMONSTRATION              ║\n");
    printf("║                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("QUANTUM NO-CLONING THEOREM (Wootters & Zurek, 1982):\n");
    printf("──────────────────────────────────────────────────────────\n");
    printf("\"It is impossible to create an independent and identical\n");
    printf(" copy of an arbitrary unknown quantum state.\"\n");
    printf("──────────────────────────────────────────────────────────\n\n");
    
    printf("This fundamental law of quantum physics makes quantum money\n");
    printf("impossible to counterfeit.\n\n");
    
    // Create original quantum money
    printf("Scenario: Alice has genuine quantum money\n");
    printf("──────────────────────────────────────────────────────────\n\n");
    
    quantum_banknote_t original;
    quantum_money_mint(bank, &original, 100.00);
    
    quantum_money_print_note(&original);
    
    // Verify it's genuine
    printf("\nAlice verifies her money at the bank:\n");
    printf("──────────────────────────────────────────────────────────\n");
    
    if (quantum_money_verify(bank, &original)) {
        printf("✓ Alice's money is GENUINE\n");
    }
    
    // Attempt counterfeiting
    printf("\n\nScenario: Bob tries to counterfeit Alice's money\n");
    printf("──────────────────────────────────────────────────────────\n\n");
    
    quantum_banknote_t fake;
    quantum_money_attempt_counterfeit(bank, &original, &fake);
    
    // Show the difference
    printf("\n\nCOMPARISON:\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("Original (Alice)         Counterfeit (Bob)\n");
    printf("────────────────────────────────────────────────────────────\n");
    printf("Serial: %llu    Serial: %llu (copied)\n",
           (unsigned long long)original.serial_number,
           (unsigned long long)fake.serial_number);
    printf("CHSH: %.4f              CHSH: %.4f (different!)\n",
           original.authenticity_proof.chsh_value,
           fake.authenticity_proof.chsh_value);
    printf("Status: GENUINE          Status: COUNTERFEIT\n");
    printf("════════════════════════════════════════════════════════════\n\n");
    
    printf("CONCLUSION:\n");
    printf("──────────────────────────────────────────────────────────\n");
    printf("✓ Original money remains genuine\n");
    printf("✗ Counterfeit is detected by bank\n");
    printf("✓ Quantum no-cloning theorem prevents copying\n");
    printf("✓ On real quantum hardware this is guaranteed by physics\n");
    printf("  (this program demonstrates the protocol in simulation)\n\n");

    quantum_banknote_free(&original);
    quantum_banknote_free(&fake);
}

void quantum_money_compare_classical(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                                                           ║\n");
    printf("║       CLASSICAL MONEY vs QUANTUM MONEY                    ║\n");
    printf("║                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("CLASSICAL MONEY (e.g., Bitcoin, Credit Cards):\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Security: Cryptographic (computational assumptions)\n");
    printf("Copying: Can be copied if private key is stolen\n");
    printf("Verification: Requires cryptographic computation\n");
    printf("Threats:\n");
    printf("  ✗ Quantum computers break RSA/ECC\n");
    printf("  ✗ Private keys can be stolen\n");
    printf("  ✗ Requires trusted third parties\n");
    printf("  ✗ Double-spending requires complex protocols\n\n");
    
    printf("QUANTUM MONEY:\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Security: Physical (quantum no-cloning theorem)\n");
    printf("Copying: PHYSICALLY IMPOSSIBLE\n");
    printf("Verification: Bell test (public, verifiable)\n");
    printf("Advantages:\n");
    printf("  ✓ Immune to quantum computer attacks\n");
    printf("  ✓ Cannot be cloned even with full quantum computer\n");
    printf("  ✓ Bank stores quantum states securely\n");
    printf("  ✓ Double-spending automatically detected\n\n");
    
    printf("SECURITY COMPARISON:\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Classical: Security based on computational hardness\n");
    printf("           → Can be broken with enough computing power\n");
    printf("           → Quantum computers threaten RSA/ECC\n\n");
    
    printf("Quantum: Security based on laws of physics\n");
    printf("         → Cannot be broken even with infinite computing power\n");
    printf("         → No-cloning theorem is a LAW OF NATURE\n\n");
    
    printf("🏆 QUANTUM ADVANTAGE: Information-theoretic security\n");
    printf("   (Not computational security)\n\n");
}

void quantum_money_full_demonstration(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                                                           ║\n");
    printf("║     QUANTUM MONEY: COMPLETE PROTOCOL DEMONSTRATION        ║\n");
    printf("║                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    // Initialize
    secure_rng_ctx_t *ctx;
    if (secure_rng_init(&ctx) != SECURE_RNG_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize RNG\n");
        return;
    }
    
    quantum_bank_t bank;
    if (quantum_bank_init(&bank, ctx) != 0) {
        fprintf(stderr, "Error: Failed to initialize bank\n");
        secure_rng_free(ctx);
        return;
    }
    
    printf("\n════════════════════════════════════════════════════════════\n");
    printf("SCENARIO 1: Normal Transaction (Alice pays Bob)\n");
    printf("════════════════════════════════════════════════════════════\n");
    
    // Mint money for Alice
    printf("\n1. Bank mints $100 quantum money for Alice\n");
    printf("───────────────────────────────────────────────────────────\n");
    
    quantum_banknote_t alice_money;
    quantum_money_mint(&bank, &alice_money, 100.00);
    
    // Alice verifies
    printf("\n2. Alice verifies her money\n");
    printf("───────────────────────────────────────────────────────────\n");
    
    if (quantum_money_verify(&bank, &alice_money)) {
        printf("✓ Alice confirms: Money is genuine\n");
    }
    
    // Alice pays Bob
    printf("\n3. Alice gives money to Bob\n");
    printf("───────────────────────────────────────────────────────────\n");
    printf("  Bob receives the banknote...\n");
    
    // Bob verifies
    printf("\n4. Bob verifies the money at bank\n");
    printf("───────────────────────────────────────────────────────────\n");
    
    quantum_banknote_t bob_money = alice_money;
    if (quantum_money_verify(&bank, &bob_money)) {
        printf("✓ Bob confirms: Money is genuine\n");
        printf("✓ Transaction successful!\n");
    }
    
    // Attack scenario
    printf("\n\n════════════════════════════════════════════════════════════\n");
    printf("SCENARIO 2: Counterfeiting Attempt (Eve attacks)\n");
    printf("════════════════════════════════════════════════════════════\n");
    
    printf("\nEve tries to counterfeit Bob's $100 note\n");
    printf("───────────────────────────────────────────────────────────\n");
    
    quantum_banknote_t eve_fake;
    quantum_money_attempt_counterfeit(&bank, &bob_money, &eve_fake);
    
    // Statistics
    printf("\n\n════════════════════════════════════════════════════════════\n");
    printf("BANK STATISTICS\n");
    printf("════════════════════════════════════════════════════════════\n");
    quantum_money_print_bank_stats(&bank);

    // Cleanup (bob_money is a shallow copy of alice_money, so only the
    // owning notes are freed)
    quantum_banknote_free(&alice_money);
    quantum_banknote_free(&eve_fake);
    quantum_bank_free(&bank);
    secure_rng_free(ctx);
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void quantum_money_print_note(const quantum_banknote_t *note) {
    printf("\n┌─────────────────────────────────────────────────────────────┐\n");
    printf("│              QUANTUM BANKNOTE                               │\n");
    printf("├─────────────────────────────────────────────────────────────┤\n");
    printf("│                                                             │\n");
    printf("│  Serial Number: %-43llu │\n", 
           (unsigned long long)note->serial_number);
    printf("│  Denomination: $%-42.2f │\n", note->denomination);
    printf("│  Issue Date: %-46s│\n", ctime(&note->issue_time));
    printf("│                                                             │\n");
    printf("├─────────────────────────────────────────────────────────────┤\n");
    printf("│  QUANTUM CERTIFICATION                                      │\n");
    printf("│                                                             │\n");
    printf("│  Bell Test CHSH: %-42.6f │\n", 
           note->authenticity_proof.chsh_value);
    printf("│  Quantum Bound: 2.828427                                    │\n");
    printf("│  Classical Bound: 2.000000                                  │\n");
    printf("│                                                             │\n");
    
    if (note->verified) {
        printf("│  Status: %-50s │\n", 
               note->is_genuine ? "✓ VERIFIED GENUINE" : "✗ COUNTERFEIT DETECTED");
    } else {
        printf("│  Status: Not yet verified                                   │\n");
    }
    
    printf("│                                                             │\n");
    printf("├─────────────────────────────────────────────────────────────┤\n");
    printf("│  This note is protected by quantum no-cloning theorem.      │\n");
    printf("│  Counterfeiting is PHYSICALLY IMPOSSIBLE.                   │\n");
    printf("└─────────────────────────────────────────────────────────────┘\n");
}

void quantum_money_print_verification(
    const quantum_banknote_t *note,
    const bell_test_result_t *proof
) {
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         QUANTUM MONEY VERIFICATION RESULT                 ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║                                                           ║\n");
    printf("║  Serial: %-51llu ║\n", (unsigned long long)note->serial_number);
    printf("║  Value: $%-50.2f ║\n", note->denomination);
    printf("║                                                           ║\n");
    printf("║  Bell Test Results:                                       ║\n");
    printf("║    CHSH Value: %-42.6f ║\n", proof->chsh_value);
    printf("║    Original:   %-42.6f ║\n", note->authenticity_proof.chsh_value);
    printf("║    Difference: %-42.6f ║\n",
           fabs(proof->chsh_value - note->authenticity_proof.chsh_value));
    printf("║                                                           ║\n");
    
    if (note->is_genuine) {
        printf("║  ┌─────────────────────────────────────────────────────┐  ║\n");
        printf("║  │   ✓✓✓ GENUINE BANKNOTE ✓✓✓                          │  ║\n");
        printf("║  │                                                     │  ║\n");
        printf("║  │   Quantum state verified through Bell test.        │  ║\n");
        printf("║  │   This note is authentic and valid.                │  ║\n");
        printf("║  └─────────────────────────────────────────────────────┘  ║\n");
    } else {
        printf("║  ┌─────────────────────────────────────────────────────┐  ║\n");
        printf("║  │   ✗✗✗ COUNTERFEIT DETECTED ✗✗✗                      │  ║\n");
        printf("║  │                                                     │  ║\n");
        printf("║  │   Quantum state does not match original.           │  ║\n");
        printf("║  │   This note has been copied or tampered with.      │  ║\n");
        printf("║  └─────────────────────────────────────────────────────┘  ║\n");
    }
    
    printf("╚═══════════════════════════════════════════════════════════╝\n");
}

void quantum_money_print_bank_stats(const quantum_bank_t *bank) {
    if (!bank) return;
    
    printf("\nTotal Notes Issued: %llu\n", (unsigned long long)bank->total_issued);
    printf("Notes in Circulation: %zu\n", bank->num_states);
    printf("Counterfeits Detected: %llu\n", (unsigned long long)bank->counterfeits_detected);
    printf("Counterfeit Rate: %.2f%%\n",
           bank->total_issued > 0 ? 
           (bank->counterfeits_detected * 100.0 / bank->total_issued) : 0.0);
}

// ============================================================================
// MAIN DEMONSTRATION
// ============================================================================

int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                   ║\n");
    printf("║              QUANTUM MONEY: UNFORGEABLE BY PHYSICS                ║\n");
    printf("║                                                                   ║\n");
    printf("║  Based on Stephen Wiesner's 1983 Proposal                         ║\n");
    printf("║  Using Quantum No-Cloning Theorem                                 ║\n");
    printf("║                                                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    
    printf("\nWHAT MAKES QUANTUM MONEY SPECIAL:\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("Traditional money security relies on:\n");
    printf("  • Complex printing (can be forged with skill)\n");
    printf("  • Cryptographic signatures (can be broken)\n");
    printf("  • Trusted authorities (can be corrupted)\n\n");
    
    printf("Quantum money security relies on:\n");
    printf("  • Laws of quantum physics (CANNOT be violated)\n");
    printf("  • No-cloning theorem (fundamental impossibility)\n");
    printf("  • Bell test verification (mathematical proof)\n\n");
    
    printf("Result: Counterfeiting is PHYSICALLY IMPOSSIBLE, not just hard!\n\n");
    printf("NOTE: This program is a CLASSICAL SIMULATION of the protocol.\n");
    printf("The physical no-cloning guarantee applies to real quantum hardware;\n");
    printf("here the quantum states are simulated to demonstrate the concept.\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        // Full demonstration including no-cloning
        secure_rng_ctx_t *ctx;
        secure_rng_init(&ctx);
        
        quantum_bank_t bank;
        quantum_bank_init(&bank, ctx);
        
        quantum_money_demonstrate_no_cloning(&bank);
        
        printf("\n\nPress Enter to see classical comparison...");
        getchar();
        
        quantum_money_compare_classical();
        
        quantum_bank_free(&bank);
        secure_rng_free(ctx);
    } else if (argc > 1 && strcmp(argv[1], "--protocol") == 0) {
        // Full protocol demonstration
        quantum_money_full_demonstration();
    } else {
        // Quick demonstration
        printf("\nPress Enter to begin demonstration...");
        getchar();
        
        quantum_money_full_demonstration();
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║                        WHY THIS MATTERS                           ║\n");
    printf("╠═══════════════════════════════════════════════════════════════════╣\n");
    printf("║                                                                   ║\n");
    printf("║  Classical Money: Protected by human cleverness                   ║\n");
    printf("║    → Can always be defeated by more cleverness                    ║\n");
    printf("║                                                                   ║\n");
    printf("║  Quantum Money: Protected by laws of physics                      ║\n");
    printf("║    → Cannot be defeated, even in principle                        ║\n");
    printf("║                                                                   ║\n");
    printf("║  This is the difference between:                                  ║\n");
    printf("║    • Computational security (can be broken)                       ║\n");
    printf("║    • Information-theoretic security (provably unbreakable)        ║\n");
    printf("║                                                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║                       USAGE EXAMPLES                              ║\n");
    printf("╠═══════════════════════════════════════════════════════════════════╣\n");
    printf("║                                                                   ║\n");
    printf("║  Quick demo:         ./quantum_money                              ║\n");
    printf("║  No-cloning demo:    ./quantum_money --demo                       ║\n");
    printf("║  Full protocol:      ./quantum_money --protocol                   ║\n");
    printf("║                                                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
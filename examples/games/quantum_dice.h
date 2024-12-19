#ifndef QUANTUM_DICE_H
#define QUANTUM_DICE_H

#include "../../src/quantum_rng/quantum_rng.h"

// Forward declaration of the opaque type
struct quantum_dice_t;
typedef struct quantum_dice_t quantum_dice_t;

/**
 * Create a new quantum dice with the specified number of sides
 * 
 * @param ctx QRNG context to use for random number generation
 * @param sides Number of sides (e.g., 6 for d6)
 * @return Pointer to new quantum_dice_t or NULL on error
 */
quantum_dice_t* quantum_dice_create(qrng_ctx *ctx, int sides);

/**
 * Roll the quantum dice
 * 
 * @param dice Quantum dice instance
 * @return Random number between 1 and number of sides, inclusive
 */
int quantum_dice_roll(quantum_dice_t *dice);

/**
 * Free resources associated with a quantum dice
 * 
 * @param dice Quantum dice instance to free
 */
void quantum_dice_free(quantum_dice_t *dice);

/**
 * Get the number of sides for a quantum dice
 * 
 * @param dice Quantum dice instance
 * @return Number of sides
 */
int quantum_dice_sides(const quantum_dice_t *dice);

/**
 * Perform multiple rolls in a batch operation
 * 
 * @param dice Quantum dice instance
 * @param results Array to store results
 * @param count Number of rolls to perform
 * @return 0 on success, non-zero on error
 */
int quantum_dice_batch_roll(quantum_dice_t *dice, int *results, int count);

/**
 * Reset the quantum dice state
 * 
 * @param dice Quantum dice instance
 * @return 0 on success, non-zero on error
 */
int quantum_dice_reset(quantum_dice_t *dice);

#endif /* QUANTUM_DICE_H */

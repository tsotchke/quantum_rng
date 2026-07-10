#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "quantum_dice.h"
#include "../../src/quantum_rng/quantum_rng.h"

// Internal constants
#define QUANTUM_MIXING_ROUNDS 3
#define MAX_BATCH_SIZE 1000

struct quantum_dice_t {
    qrng_ctx *qrng;        // Quantum RNG context
    int sides;             // Number of sides
    double *state_buffer;  // Buffer for batch operations
};

// Helper function to validate dice parameters
static int validate_sides(int sides) {
    // Support common RPG dice sizes
    switch (sides) {
        case 4:  // d4
        case 6:  // d6
        case 8:  // d8
        case 10: // d10
        case 12: // d12
        case 20: // d20
        case 100: // d100
            return 1;
        default:
            return 0;
    }
}

quantum_dice_t* quantum_dice_create(qrng_ctx *ctx, int sides) {
    if (!ctx || !validate_sides(sides)) {
        return NULL;
    }
    
    quantum_dice_t *dice = malloc(sizeof(quantum_dice_t));
    if (!dice) {
        return NULL;
    }
    
    dice->qrng = ctx;
    dice->sides = sides;
    dice->state_buffer = malloc(MAX_BATCH_SIZE * sizeof(double));
    
    if (!dice->state_buffer) {
        free(dice);
        return NULL;
    }
    
    return dice;
}

void quantum_dice_free(quantum_dice_t *dice) {
    if (dice) {
        free(dice->state_buffer);
        free(dice);
    }
}

int quantum_dice_roll(quantum_dice_t *dice) {
    if (!dice) {
        return -1;
    }

    // Unbiased rejection sampling over [1, sides].
    //
    // A 64-bit draw mapped with a plain modulo is slightly biased whenever
    // `sides` does not divide 2^64 evenly: the low `2^64 mod sides` outcomes
    // are one draw more likely than the rest. We eliminate that bias exactly
    // by rejecting draws that fall in that unfair tail, so every face is
    // equally likely with zero residual bias -- not merely below a detection
    // threshold.
    const uint64_t sides = (uint64_t)dice->sides;
    const uint64_t threshold = (0ULL - sides) % sides;  // == 2^64 mod sides
    uint64_t r;
    do {
        r = qrng_uint64(dice->qrng);
    } while (r < threshold);

    return (int)(r % sides) + 1;
}

int quantum_dice_sides(const quantum_dice_t *dice) {
    return dice ? dice->sides : -1;
}

int quantum_dice_batch_roll(quantum_dice_t *dice, int *results, int count) {
    if (!dice || !results || count <= 0 || count > MAX_BATCH_SIZE) {
        return -1;
    }
    
    // Each roll is an independent, exactly-unbiased draw over [1, sides],
    // using the same rejection-sampling scheme as the single-roll path.
    // (An earlier version averaged several uniforms per roll, which produced
    // a bell-shaped Irwin-Hall distribution biased toward the middle faces.)
    const uint64_t sides = (uint64_t)dice->sides;
    const uint64_t threshold = (0ULL - sides) % sides;  // == 2^64 mod sides
    for (int i = 0; i < count; i++) {
        uint64_t r;
        do {
            r = qrng_uint64(dice->qrng);
        } while (r < threshold);
        results[i] = (int)(r % sides) + 1;
    }

    return 0;
}

int quantum_dice_reset(quantum_dice_t *dice) {
    if (!dice) {
        return -1;
    }
    
    // Clear the batch buffer so stale variates cannot be reused
    memset(dice->state_buffer, 0, MAX_BATCH_SIZE * sizeof(double));

    // Advance the underlying RNG stream a few steps. This does not
    // "re-randomize" anything (the stream is already unpredictable);
    // it simply discards the next few outputs.
    for (int i = 0; i < QUANTUM_MIXING_ROUNDS; i++) {
        (void)qrng_double(dice->qrng);
    }

    return 0;
}

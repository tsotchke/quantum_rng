#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "quantum_dice.h"
#include "../../src/quantum_rng/quantum_rng.h"

// Internal constants
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
    
    // Use quantum RNG to generate a high-precision double
    double raw;
    uint32_t scaled;
    
    // Use rejection sampling to ensure perfect uniformity
    do {
        raw = qrng_double(dice->qrng);
        // Scale to range [0, 2^32)
        scaled = (uint32_t)(raw * 4294967296.0);
    } while (scaled >= ((4294967296U / dice->sides) * dice->sides));
    
    // Map to [1, sides] with perfect uniformity
    return (scaled % dice->sides) + 1;
}

int quantum_dice_sides(const quantum_dice_t *dice) {
    return dice ? dice->sides : -1;
}

int quantum_dice_batch_roll(quantum_dice_t *dice, int *results, int count) {
    if (!dice || !results || count <= 0 || count > MAX_BATCH_SIZE) {
        return -1;
    }
    
    // Use same algorithm as single roll for perfect uniformity
    for (int i = 0; i < count; i++) {
        double raw;
        uint32_t scaled;
        
        do {
            raw = qrng_double(dice->qrng);
            scaled = (uint32_t)(raw * 4294967296.0);
        } while (scaled >= ((4294967296U / dice->sides) * dice->sides));
        
        results[i] = (scaled % dice->sides) + 1;
    }
    
    return 0;
}

int quantum_dice_reset(quantum_dice_t *dice) {
    if (!dice) {
        return -1;
    }
    
    // Clear state buffer
    memset(dice->state_buffer, 0, MAX_BATCH_SIZE * sizeof(double));
    
    // Single quantum sample to reset state
    qrng_double(dice->qrng);
    
    return 0;
}

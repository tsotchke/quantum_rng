#include <stdio.h>
#include <stdlib.h>
#include "quantum_dice.h"

// Demo program showing various uses of quantum dice
int main() {
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, NULL, 0);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize QRNG: %s\n", qrng_error_string(err));
        return 1;
    }
    
    printf("=== Quantum Dice Demo ===\n\n");
    
    // Create different types of dice
    quantum_dice_t *d6 = quantum_dice_create(ctx, 6);
    quantum_dice_t *d20 = quantum_dice_create(ctx, 20);
    
    if (!d6 || !d20) {
        fprintf(stderr, "Failed to create dice\n");
        qrng_free(ctx);
        return 1;
    }
    
    // Single roll demonstration
    printf("Rolling d6: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", quantum_dice_roll(d6));
    }
    printf("\n");
    
    printf("Rolling d20: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", quantum_dice_roll(d20));
    }
    printf("\n\n");
    
    // Batch roll demonstration
    printf("Batch rolling d6 (10 times):\n");
    int results[10];
    if (quantum_dice_batch_roll(d6, results, 10) == 0) {
        for (int i = 0; i < 10; i++) {
            printf("%d ", results[i]);
        }
        printf("\n");
    }
    
    // Distribution demonstration
    printf("\nD6 Distribution Test (1000 rolls):\n");
    int counts[6] = {0};
    for (int i = 0; i < 1000; i++) {
        int roll = quantum_dice_roll(d6);
        counts[roll-1]++;
    }
    
    for (int i = 0; i < 6; i++) {
        printf("Face %d: %d times (%.1f%%)\n", 
               i+1, counts[i], counts[i]/10.0);
    }
    
    // Cleanup
    quantum_dice_free(d6);
    quantum_dice_free(d20);
    qrng_free(ctx);
    
    printf("\nDemo completed successfully!\n");
    return 0;
}

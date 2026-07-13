/* wubu_rng_qrng.c — adapter proving INTEGRATION.md §3b compiles + runs.
 * Wires quantum_rng's SEEDED mode as WuBuMath-style reproducible RNG.
 * Build:
 *   gcc -std=c11 -O2 -I../src -I../src/quantum_rng wubu_rng_qrng.c \
 *       ../src/quantum_rng/quantum_rng.c -o /tmp/wubu_rng_qrng -lm
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "quantum_rng.h"

static qrng_ctx *g;

void wubu_rng_seed(const uint8_t *s, size_t n) {
    qrng_init(&g, s, n);          /* seeded mode -> reproducible stream */
}
double wubu_rng_uniform(void) {
    return qrng_double(g);        /* in [0,1) */
}

int main(void) {
    uint8_t seed[4] = {0xDE,0xAD,0xBE,0xEF};
    wubu_rng_seed(seed, sizeof seed);
    printf("seeded uniform draws: %.6f %.6f %.6f\n",
           wubu_rng_uniform(), wubu_rng_uniform(), wubu_rng_uniform());

    /* reproducibility: re-seed, expect identical sequence */
    wubu_rng_seed(seed, sizeof seed);
    printf("after re-seed:       %.6f %.6f %.6f\n",
           wubu_rng_uniform(), wubu_rng_uniform(), wubu_rng_uniform());
    return 0;
}

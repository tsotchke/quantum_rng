/*
 * determinism_test.c -- Devil's-advocate check for tsotchke/quantum_rng.
 *
 * Upstream historically claimed "verified non-deterministic output" and the
 * audit's FIRST pass asserted the seed was decorative (the stream was keyed by
 * wall-clock time + PID). That claim is STALE against the current code: qrng_init
 * now honors a real seeded-PRNG contract (see quantum_rng.h:17-22 and
 * quantum_rng.c:287-303):
 *
 *   - seeded   (seed != NULL): system_entropy = absorb_seed(seed); the stream is
 *     a PURE, REPRODUCIBLE function of the seed. No time/PID/rdtsc is mixed in.
 *   - unseeded (seed == NULL): draws gettimeofday/getpid -> non-deterministic.
 *
 * To actually PROVE which mode we are in, this test runs the generator and prints
 * the raw stream to STDOUT only; all run metadata (PID, mode, seed) goes to
 * STDERR. That way `diff run1.txt run2.txt` of stdout is byte-identical for the
 * seeded mode across different PIDs and wall-clock times, proving the seed -- not
 * the clock -- governs the output.
 *
 * Build (the test self-contains the missing <sys/types.h> / M_PI / M_E the
 * upstream build omits when compiling the core standalone):
 *   gcc -std=c11 -O2 determinism_test.c -o determinism_test -lm
 *   ./determinism_test              # seeded run (default seed) -> stdout stream
 *   ./determinism_test 0xNN         # optional: supply one seed byte to vary it
 *   ./determinism_test --unseeded   # unseeded run -> non-deterministic stream
 *
 * Expected (current code):
 *   - seeded (same args): byte-identical stream every run  -> diff is empty.
 *   - different seed byte: different stream.
 *   - --unseeded: stream differs run-to-run (non-deterministic).
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif
#include "../src/quantum_rng/quantum_rng.c"

int main(int argc, char **argv) {
    int unseeded = 0;
    uint8_t seed[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    int have_seed = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--unseeded") == 0) { unseeded = 1; }
        else { seed[0] = (uint8_t) strtol(argv[i], NULL, 0); have_seed = 1; }
    }

    qrng_ctx *a = NULL;
    if (unseeded) {
        qrng_init(&a, NULL, 0);          /* no seed -> draws gettimeofday/getpid */
    } else {
        qrng_init(&a, seed, 4);          /* seeded -> pure function of seed */
    }

    /* Metadata on stderr so stdout stays a clean, diffable stream. */
    fprintf(stderr, "PID=%d mode=%s seed=%s\n",
            (int) getpid(),
            unseeded ? "unseeded" : "seeded",
            unseeded ? "(none)" : (have_seed ? argv[1] : "default"));

    for (int i = 0; i < 8; i++)
        printf("%016llx\n", (unsigned long long) qrng_uint64(a));
    return 0;
}

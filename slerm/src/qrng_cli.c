#include "qrng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "usage: %s [options]\n"
        "  -n N        generate N 64-bit values (default 8)\n"
        "  -s SEED     seed with the string SEED (deterministic)\n"
        "  -e BYTES    estimate entropy over BYTES of output (default 1 MiB)\n"
        "  -h          this help\n", prog);
}

int main(int argc, char **argv) {
    long n = 8;
    const char *seed = NULL;
    size_t ebytes = 1024 * 1024;
    int show_entropy = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) n = strtol(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) seed = argv[++i];
        else if (!strcmp(argv[i], "-e") && i + 1 < argc) { ebytes = (size_t)strtoull(argv[++i], NULL, 10); show_entropy = 1; }
        else if (!strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }

    int from_os = 0;
    qrng_ctx *ctx = qrng_create(seed, seed ? strlen(seed) : 0, &from_os);
    if (!ctx) { fprintf(stderr, "qrng_create failed\n"); return 1; }

    printf("# seed source: %s\n", seed ? "explicit string (deterministic)"
                                       : (from_os ? "OS entropy" : "time+pid (fallback)"));

    if (!show_entropy) {
        for (long i = 0; i < n; i++)
            printf("%016llx\n", (unsigned long long)qrng_next_u64(ctx));
    } else {
        unsigned char *buf = (unsigned char *)malloc(ebytes ? ebytes : 1);
        if (!buf) { fprintf(stderr, "alloc failed\n"); qrng_free(ctx); return 1; }
        qrng_fill(ctx, buf, ebytes);
        qrng_entropy_report r = qrng_entropy_estimate(buf, ebytes);
        printf("# measured over %zu bytes (%zu bits)\n", ebytes, ebytes * 8);
        printf("shannon_bits_per_byte = %.6f  (max 8.0)\n", r.shannon_bits_per_byte);
        printf("chi_square            = %.3f  (df=255)\n", r.chi_square);
        printf("chi_square_p          = %.4f  (0.01..0.99 is unremarkable)\n", r.chi_square_p);
        printf("runs_total            = %ld\n", r.runs_total);
        printf("runs_expected         = %.0f\n", r.runs_expected);
        printf("heuristic_pass        = %s\n", r.ok ? "yes" : "no");
        printf("# NOTE: this is a MEASUREMENT of the output stream, not a\n"
               "# claim of true entropy. A seeded PRNG has ~0 true entropy;\n"
               "# reproducible output is a feature for testing.\n");
        free(buf);
    }

    qrng_free(ctx);
    return 0;
}

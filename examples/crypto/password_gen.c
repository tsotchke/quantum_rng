/**
 * @file password_gen.c
 * @brief Quantum-random password generation demo.
 *
 * All random choices come from the quantum RNG via rejection sampling, so
 * character selection is uniform (no modulo bias).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../src/quantum_rng/quantum_rng.h"
#include "password_gen.h"

#define DEFAULT_MIN_LENGTH 12
#define DEFAULT_MAX_LENGTH 16
#define DEFAULT_SPECIAL_CHARS "!@#$%^&*()-_=+[]{}|;:,.<>?"
#define MAX_GENERATION_ATTEMPTS 16

// Lazily-initialized quantum RNG shared by this module.
// (The previous code called quantum_rng_generate(), which no longer exists
// in the v3 API.)
static int quantum_random_bytes(unsigned char* buffer, size_t len) {
    static qrng_ctx* g_ctx = NULL;

    if (!g_ctx) {
        if (qrng_init(&g_ctx, NULL, 0) != QRNG_SUCCESS) {
            return -1;
        }
    }

    return (qrng_bytes(g_ctx, buffer, len) == QRNG_SUCCESS) ? 0 : -1;
}

// Uniform random value in [0, range) using rejection sampling.
// BUG FIX: the previous code used rand_byte % range, which biases toward
// small values whenever 256 is not a multiple of range.
static int quantum_uniform(size_t range, size_t* out) {
    if (range == 0 || range > 256 || !out) return -1;

    unsigned int limit = 256u - (256u % (unsigned int)range);
    for (;;) {
        unsigned char byte;
        if (quantum_random_bytes(&byte, 1) != 0) return -1;
        if (byte < limit) {
            *out = byte % range;
            return 0;
        }
    }
}

void init_password_config(PasswordConfig* config) {
    config->min_length = DEFAULT_MIN_LENGTH;
    config->max_length = DEFAULT_MAX_LENGTH;
    config->require_uppercase = 1;
    config->require_lowercase = 1;
    config->require_numbers = 1;
    config->require_special = 1;
    config->allowed_special = DEFAULT_SPECIAL_CHARS;
}

static int get_random_char(char type, const char* special_chars) {
    size_t idx;

    switch (type) {
        case 'u': // uppercase
            if (quantum_uniform(26, &idx) != 0) return -1;
            return 'A' + (int)idx;
        case 'l': // lowercase
            if (quantum_uniform(26, &idx) != 0) return -1;
            return 'a' + (int)idx;
        case 'n': // number
            if (quantum_uniform(10, &idx) != 0) return -1;
            return '0' + (int)idx;
        case 's': { // special
            size_t n = special_chars ? strlen(special_chars) : 0;
            if (n == 0 || quantum_uniform(n, &idx) != 0) return -1;
            return special_chars[idx];
        }
        default:
            return -1;
    }
}

int generate_password(const PasswordConfig* config, char* password, size_t buffer_size) {
    if (!config || !password ||
        config->min_length <= 0 || config->max_length < config->min_length ||
        buffer_size < (size_t)config->min_length + 1) {
        return -1;
    }

    int required[4] = {
        config->require_uppercase,
        config->require_lowercase,
        config->require_numbers,
        config->require_special
    };
    int num_required = required[0] + required[1] + required[2] + required[3];
    if (config->min_length < num_required) {
        return -1;  // cannot satisfy all required character classes
    }
    if (required[3] && (!config->allowed_special || config->allowed_special[0] == '\0')) {
        return -1;  // special characters required but none allowed
    }

    // The character pool is exactly the set of selected (required) classes, so
    // a policy actually excludes the classes it does not ask for — e.g. an
    // "alphanumeric" policy never emits special characters.
    static const char class_type[4] = {'u', 'l', 'n', 's'};
    int    active_class[4];   // active slot -> class index (0..3)
    size_t num_active = 0;
    for (int j = 0; j < 4; j++) {
        if (required[j]) active_class[num_active++] = j;
    }
    // num_active == num_required >= 1 (guarded above).

    for (int attempt = 0; attempt < MAX_GENERATION_ATTEMPTS; attempt++) {
        // Get random password length within range
        size_t span;
        if (quantum_uniform((size_t)(config->max_length - config->min_length + 1), &span) != 0) {
            return -3;
        }
        size_t length = (size_t)config->min_length + span;

        if (buffer_size < length + 1) {
            return -2;
        }

        int type_counts[4] = {0};   // indexed by class (0..3)

        // Generate password, drawing only from the selected classes
        for (size_t i = 0; i < length; i++) {
            size_t sel;
            if (quantum_uniform(num_active, &sel) != 0) {
                return -3;
            }

            // Reserve the final slots so every selected class appears at least
            // once, even when length equals the number of required classes.
            if (i + num_active >= length) {
                for (size_t j = 0; j < num_active; j++) {
                    if (type_counts[active_class[j]] == 0) {
                        sel = j;
                        break;
                    }
                }
            }

            int cls = active_class[sel];
            int c = get_random_char(class_type[cls], config->allowed_special);
            if (c < 0) return -3;

            password[i] = (char)c;
            type_counts[cls]++;
        }

        password[length] = '\0';

        // Verify requirements are met; retry with a bounded loop instead of
        // unbounded recursion
        if ((config->require_uppercase && type_counts[0] == 0) ||
            (config->require_lowercase && type_counts[1] == 0) ||
            (config->require_numbers && type_counts[2] == 0) ||
            (config->require_special && type_counts[3] == 0)) {
            continue;
        }

        return 0;
    }

    return -4;
}

int validate_password(const char* password, const PasswordConfig* config) {
    if (!password || !config) return -1;

    size_t len = strlen(password);
    if (len < (size_t)config->min_length || len > (size_t)config->max_length) {
        return -2;
    }

    int has_upper = 0, has_lower = 0, has_number = 0, has_special = 0;

    for (size_t i = 0; i < len; i++) {
        char c = password[i];
        if (c >= 'A' && c <= 'Z') has_upper = 1;
        else if (c >= 'a' && c <= 'z') has_lower = 1;
        else if (c >= '0' && c <= '9') has_number = 1;
        else if (config->allowed_special && strchr(config->allowed_special, c)) has_special = 1;
        else return -3; // Invalid character
    }

    if ((config->require_uppercase && !has_upper) ||
        (config->require_lowercase && !has_lower) ||
        (config->require_numbers && !has_number) ||
        (config->require_special && !has_special)) {
        return -4;
    }

    return 0;
}

double calculate_password_entropy(const char* password) {
    if (!password) return 0.0;

    size_t len = strlen(password);
    if (len == 0) return 0.0;

    int charset_size = 0;
    int has_upper = 0, has_lower = 0, has_number = 0, has_special = 0;

    for (size_t i = 0; i < len; i++) {
        char c = password[i];
        if (c >= 'A' && c <= 'Z') has_upper = 1;
        else if (c >= 'a' && c <= 'z') has_lower = 1;
        else if (c >= '0' && c <= '9') has_number = 1;
        else has_special = 1;
    }

    charset_size += has_upper ? 26 : 0;
    charset_size += has_lower ? 26 : 0;
    charset_size += has_number ? 10 : 0;
    charset_size += has_special ? (int)strlen(DEFAULT_SPECIAL_CHARS) : 0;

    if (charset_size == 0) return 0.0;  // guard log2(0)

    return (double)len * log2((double)charset_size);
}

int generate_multiple_passwords(const PasswordConfig* config,
                              char** passwords,
                              size_t num_passwords,
                              size_t buffer_size) {
    if (!config || !passwords || num_passwords == 0) {
        return -1;
    }

    for (size_t i = 0; i < num_passwords; i++) {
        if (!passwords[i]) {
            return -2;
        }

        int result = generate_password(config, passwords[i], buffer_size);
        if (result != 0) {
            return result;
        }

        // Ensure uniqueness
        for (size_t j = 0; j < i; j++) {
            if (strcmp(passwords[i], passwords[j]) == 0) {
                i--; // Retry this password
                break;
            }
        }
    }

    return (int)num_passwords;
}

// ============================================================================
// DEMONSTRATION DRIVER
// ============================================================================
//
// Generates passwords under several policies, validates each, and reports the
// measured character-class distribution and mean entropy estimate over the
// batch. A small getopt front-end drives a custom policy.

#ifndef PASSWORD_GEN_NO_MAIN

#include <getopt.h>

#define DEMO_BUFFER_SIZE 128

static void classify_password(const char* pw, size_t counts[4]) {
    // counts: [0]=upper [1]=lower [2]=digit [3]=special (anything else).
    // The generator only emits characters from the configured classes, so the
    // final bucket is exactly the "special" class here.
    for (const char* p = pw; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c >= 'A' && c <= 'Z')       counts[0]++;
        else if (c >= 'a' && c <= 'z')  counts[1]++;
        else if (c >= '0' && c <= '9')  counts[2]++;
        else                            counts[3]++;
    }
}

// Generate `count` unique passwords under `cfg`, validate them, and print a
// measured character-class distribution plus the mean entropy estimate.
static int run_policy(const char* title, const PasswordConfig* cfg,
                      size_t count, int show_samples) {
    printf("%s\n", title);
    printf("   policy: length %d-%d  classes:%s%s%s%s\n",
           cfg->min_length, cfg->max_length,
           cfg->require_uppercase ? " upper" : "",
           cfg->require_lowercase ? " lower" : "",
           cfg->require_numbers   ? " digit" : "",
           cfg->require_special   ? " special" : "");

    char** pws = (char**)calloc(count, sizeof(char*));
    if (!pws) { fprintf(stderr, "   out of memory\n"); return -1; }
    for (size_t i = 0; i < count; i++) {
        pws[i] = (char*)malloc(DEMO_BUFFER_SIZE);
        if (!pws[i]) {
            for (size_t j = 0; j < i; j++) free(pws[j]);
            free(pws);
            fprintf(stderr, "   out of memory\n");
            return -1;
        }
    }

    int made = generate_multiple_passwords(cfg, pws, count, DEMO_BUFFER_SIZE);
    if (made != (int)count) {
        fprintf(stderr, "   generation failed (%d)\n", made);
        for (size_t i = 0; i < count; i++) free(pws[i]);
        free(pws);
        return -1;
    }

    size_t counts[4] = {0, 0, 0, 0};
    size_t total_chars = 0;
    size_t valid = 0;
    double entropy_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        classify_password(pws[i], counts);
        total_chars += strlen(pws[i]);
        entropy_sum += calculate_password_entropy(pws[i]);
        if (validate_password(pws[i], cfg) == 0) valid++;
    }

    if (show_samples) {
        size_t n = count < 3 ? count : 3;
        for (size_t i = 0; i < n; i++)
            printf("   sample: %s  (%.1f bits)\n",
                   pws[i], calculate_password_entropy(pws[i]));
    }

    const char* names[4] = { "uppercase", "lowercase", "digits", "special" };
    printf("   generated %zu, unique+valid %zu/%zu, %zu chars sampled\n",
           count, valid, count, total_chars);
    for (int i = 0; i < 4; i++) {
        double pct = total_chars ? 100.0 * (double)counts[i] / (double)total_chars : 0.0;
        printf("     %-10s %8zu   %5.1f%%\n", names[i], counts[i], pct);
    }
    printf("   mean entropy estimate: %.1f bits/password\n\n",
           count ? entropy_sum / (double)count : 0.0);

    for (size_t i = 0; i < count; i++) free(pws[i]);
    free(pws);
    return (valid == count) ? 0 : -1;
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  -l <length>   fixed password length for the custom policy (default 16)\n");
    printf("  -n <count>    passwords to generate per policy (default 32)\n");
    printf("  -c <classes>  required character classes for the custom policy,\n");
    printf("                any of: u=upper l=lower n=number s=special (default ulns)\n");
    printf("  -h            show this help and exit\n");
    printf("\nWith no options the demo runs a set of preset policies.\n");
}

int main(int argc, char** argv) {
    int  custom_length  = 16;
    size_t count        = 32;
    const char* classes = "ulns";
    int  have_custom    = 0;

    int opt;
    while ((opt = getopt(argc, argv, "l:n:c:h")) != -1) {
        switch (opt) {
            case 'l': {
                long v = strtol(optarg, NULL, 10);
                if (v <= 0 || v >= DEMO_BUFFER_SIZE) {
                    fprintf(stderr, "length must be in 1..%d\n", DEMO_BUFFER_SIZE - 1);
                    return 1;
                }
                custom_length = (int)v;
                have_custom = 1;
                break;
            }
            case 'n': {
                long v = strtol(optarg, NULL, 10);
                if (v <= 0) { fprintf(stderr, "count must be > 0\n"); return 1; }
                count = (size_t)v;
                break;
            }
            case 'c':
                classes = optarg;
                have_custom = 1;
                break;
            case 'h': print_usage(argv[0]); return 0;
            default:  print_usage(argv[0]); return 1;
        }
    }

    printf("=== Quantum Password Generator Demo ===\n\n");

    int failures = 0;

    if (have_custom) {
        // Build a single custom policy from -l / -c.
        PasswordConfig cfg;
        init_password_config(&cfg);
        cfg.min_length = custom_length;
        cfg.max_length = custom_length;
        cfg.require_uppercase = (strchr(classes, 'u') != NULL);
        cfg.require_lowercase = (strchr(classes, 'l') != NULL);
        cfg.require_numbers   = (strchr(classes, 'n') != NULL);
        cfg.require_special   = (strchr(classes, 's') != NULL);

        int required = cfg.require_uppercase + cfg.require_lowercase +
                       cfg.require_numbers + cfg.require_special;
        if (required == 0) {
            fprintf(stderr, "no valid classes in '%s' (use u/l/n/s)\n", classes);
            return 1;
        }
        if (custom_length < required) {
            fprintf(stderr, "length %d too short for %d required classes\n",
                    custom_length, required);
            return 1;
        }

        if (run_policy("Custom policy", &cfg, count, 1) != 0) failures++;
    } else {
        // Preset policies spanning lengths and character-class combinations.
        PasswordConfig p;

        init_password_config(&p);
        p.min_length = 8; p.max_length = 8;
        p.require_special = 0;               // alnum only
        if (run_policy("Policy A: 8 chars, alphanumeric", &p, count, 1) != 0) failures++;

        init_password_config(&p);
        p.min_length = 12; p.max_length = 16; // default: all classes, variable length
        if (run_policy("Policy B: 12-16 chars, all classes", &p, count, 1) != 0) failures++;

        init_password_config(&p);
        p.min_length = 24; p.max_length = 24;
        if (run_policy("Policy C: 24 chars, all classes", &p, count, 1) != 0) failures++;

        init_password_config(&p);
        p.min_length = 20; p.max_length = 20;
        p.require_uppercase = 0; p.require_special = 0;  // lowercase + digits
        if (run_policy("Policy D: 20 chars, lowercase+digits", &p, count, 1) != 0) failures++;
    }

    if (failures) {
        fprintf(stderr, "%d policy(ies) produced an invalid password\n", failures);
        return 1;
    }

    printf("Done.\n");
    return 0;
}

#endif // PASSWORD_GEN_NO_MAIN

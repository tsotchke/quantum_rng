#include "quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#define DEFAULT_COUNT 10
#define DEFAULT_MIN 0
#define DEFAULT_MAX UINT64_MAX
#define BUFFER_SIZE 1024

typedef enum {
    FORMAT_DECIMAL,
    FORMAT_HEX,
    FORMAT_BINARY
} output_format_t;

typedef struct {
    bool is_signed;
    union {
        struct {
            uint64_t min;
            uint64_t max;
        } u;
        struct {
            int64_t min;
            int64_t max;
        } s;
    } range;
    uint64_t count;
    output_format_t format;
    bool analyze;
    const char *output_file;
} options_t;

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -c, --count COUNT     Number of random numbers to generate (default: %d)\n", DEFAULT_COUNT);
    printf("  -m, --min MIN         Minimum value (default: 0)\n");
    printf("  -M, --max MAX         Maximum value (default: UINT64_MAX)\n");
    printf("  -f, --format FORMAT   Output format: dec, hex, bin (default: dec)\n");
    printf("  -a, --analyze         Perform statistical analysis\n");
    printf("  -o, --output FILE     Write output to file instead of stdout\n");
    printf("  -h, --help           Display this help message\n\n");
    printf("Examples:\n");
    printf("  Generate 5 random numbers:\n");
    printf("    %s -c 5\n\n", program_name);
    printf("  Generate numbers between 1 and 100:\n");
    printf("    %s -m 1 -M 100\n\n", program_name);
    printf("  Generate hex values with analysis:\n");
    printf("    %s -f hex -a\n", program_name);
}

static int calculate_required_bits(uint64_t min, uint64_t max) {
    uint64_t range = max - min + 1;
    int required_bits = 64;
    for (int i = 63; i >= 0; i--) {
        if (range & (1ULL << i)) {
            required_bits = i + 1;
            break;
        }
    }
    return required_bits;
}

static void analyze_numbers(uint64_t *numbers, size_t count, const options_t *opts) {
    if (count == 0) return;
    
    if (opts->is_signed) {
        int64_t s_min = INT64_MAX;
        int64_t s_max = INT64_MIN;
        double sum = 0;
        double sum_squares = 0;
        
        for (size_t i = 0; i < count; i++) {
            int64_t value = (int64_t)numbers[i];
            if (value < s_min) s_min = value;
            if (value > s_max) s_max = value;
            sum += value;
            sum_squares += ((double)value * value);
        }
        
        double mean = sum / count;
        double variance = (sum_squares / count) - (mean * mean);
        double std_dev = sqrt(variance);
        
        printf("\nStatistical Analysis:\n");
        printf("Minimum:        %lld\n", s_min);
        printf("Maximum:        %lld\n", s_max);
        printf("Mean:           %.2f\n", mean);
        printf("Std Deviation:  %.2f\n", std_dev);
        
        int required_bits = calculate_required_bits(
            (uint64_t)(opts->range.s.max - opts->range.s.min),
            (uint64_t)(opts->range.s.max - opts->range.s.min));
        
        printf("\nBit Distribution (%d bits needed for range %lld to %lld):\n",
               required_bits + 1, opts->range.s.min, opts->range.s.max);
        
        // Calculate bit distribution
        uint64_t *bit_counts = calloc(required_bits + 1, sizeof(uint64_t));
        if (!bit_counts) {
            fprintf(stderr, "Error: Could not allocate memory for bit analysis\n");
            return;
        }
        
        for (size_t i = 0; i < count; i++) {
            int64_t value = (int64_t)numbers[i];
            uint64_t unsigned_value = (uint64_t)(value - opts->range.s.min);
            for (int j = 0; j <= required_bits; j++) {
                if (unsigned_value & (1ULL << j)) {
                    bit_counts[j]++;
                }
            }
        }
        
        // Print bit positions header
        printf("Bit: ");
        for (int i = required_bits; i >= 0; i--) {
            printf("%d", i % 10);
            if (i > 0) printf(" ");
        }
        printf("\n");
        
        // Print separator
        printf("     ");
        for (int i = required_bits; i >= 0; i--) {
            printf("-");
            if (i > 0) printf(" ");
        }
        printf("\n");
        
        // Print distribution
        printf("Freq: ");
        for (int i = required_bits; i >= 0; i--) {
            printf("%d", (int)((double)bit_counts[i] / count * 9.0));
            if (i > 0) printf(" ");
        }
        printf("\n");
        
        free(bit_counts);
    } else {
        uint64_t u_min = numbers[0];
        uint64_t u_max = numbers[0];
        double sum = 0;
        double sum_squares = 0;
        
        for (size_t i = 0; i < count; i++) {
            uint64_t value = numbers[i];
            if (value < u_min) u_min = value;
            if (value > u_max) u_max = value;
            sum += value;
            sum_squares += ((double)value * value);
        }
        
        double mean = sum / count;
        double variance = (sum_squares / count) - (mean * mean);
        double std_dev = sqrt(variance);
        
        printf("\nStatistical Analysis:\n");
        printf("Minimum:        %llu\n", u_min);
        printf("Maximum:        %llu\n", u_max);
        printf("Mean:           %.2f\n", mean);
        printf("Std Deviation:  %.2f\n", std_dev);
        
        int required_bits = calculate_required_bits(opts->range.u.min, opts->range.u.max);
        
        printf("\nBit Distribution (%d bits needed for range %llu to %llu):\n",
               required_bits, opts->range.u.min, opts->range.u.max);
        
        // Calculate bit distribution
        uint64_t *bit_counts = calloc(required_bits, sizeof(uint64_t));
        if (!bit_counts) {
            fprintf(stderr, "Error: Could not allocate memory for bit analysis\n");
            return;
        }
        
        for (size_t i = 0; i < count; i++) {
            uint64_t value = numbers[i] - opts->range.u.min;
            for (int j = 0; j < required_bits; j++) {
                if (value & (1ULL << j)) {
                    bit_counts[j]++;
                }
            }
        }
        
        // Print bit positions header
        printf("Bit: ");
        for (int i = required_bits - 1; i >= 0; i--) {
            printf("%d", i % 10);
            if (i > 0) printf(" ");
        }
        printf("\n");
        
        // Print separator
        printf("     ");
        for (int i = required_bits - 1; i >= 0; i--) {
            printf("-");
            if (i > 0) printf(" ");
        }
        printf("\n");
        
        // Print distribution
        printf("Freq: ");
        for (int i = required_bits - 1; i >= 0; i--) {
            printf("%d", (int)((double)bit_counts[i] / count * 9.0));
            if (i > 0) printf(" ");
        }
        printf("\n");
        
        free(bit_counts);
    }
}

static int parse_options(int argc, char *argv[], options_t *opts) {
    static struct option long_options[] = {
        {"count",   required_argument, 0, 'c'},
        {"min",     required_argument, 0, 'm'},
        {"max",     required_argument, 0, 'M'},
        {"format",  required_argument, 0, 'f'},
        {"analyze", no_argument,       0, 'a'},
        {"output",  required_argument, 0, 'o'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Set defaults
    opts->is_signed = false;
    opts->range.u.min = DEFAULT_MIN;
    opts->range.u.max = DEFAULT_MAX;
    opts->count = DEFAULT_COUNT;
    opts->format = FORMAT_DECIMAL;
    opts->analyze = false;
    opts->output_file = NULL;
    
    int opt;
    int option_index = 0;
    char *endptr;
    
    while ((opt = getopt_long(argc, argv, "c:m:M:f:ao:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                opts->count = strtoull(optarg, &endptr, 0);
                if (*endptr != '\0' || opts->count == 0) {
                    fprintf(stderr, "Error: Invalid count value\n");
                    return -1;
                }
                break;
            case 'm': {
                // Try signed parse first
                int64_t s_val = strtoll(optarg, &endptr, 0);
                if (*endptr == '\0' && s_val < 0) {
                    opts->is_signed = true;
                    opts->range.s.min = s_val;
                } else {
                    // Try unsigned parse
                    opts->range.u.min = strtoull(optarg, &endptr, 0);
                    if (*endptr != '\0') {
                        fprintf(stderr, "Error: Invalid minimum value\n");
                        return -1;
                    }
                }
                break;
            }
            case 'M': {
                if (opts->is_signed) {
                    opts->range.s.max = strtoll(optarg, &endptr, 0);
                } else {
                    opts->range.u.max = strtoull(optarg, &endptr, 0);
                }
                if (*endptr != '\0') {
                    fprintf(stderr, "Error: Invalid maximum value\n");
                    return -1;
                }
                break;
            }
            case 'f':
                if (strcmp(optarg, "hex") == 0) opts->format = FORMAT_HEX;
                else if (strcmp(optarg, "bin") == 0) opts->format = FORMAT_BINARY;
                else if (strcmp(optarg, "dec") == 0) opts->format = FORMAT_DECIMAL;
                else {
                    fprintf(stderr, "Error: Unknown format '%s'\n", optarg);
                    return -1;
                }
                break;
            case 'a':
                opts->analyze = true;
                break;
            case 'o':
                opts->output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 1;
            case '?':
                return -1;
            default:
                fprintf(stderr, "Error: Unknown option %c\n", opt);
                return -1;
        }
    }
    
    if (opts->is_signed) {
        if (opts->range.s.min > opts->range.s.max) {
            fprintf(stderr, "Error: Min value cannot be greater than max value\n");
            return -1;
        }
    } else {
        if (opts->range.u.min > opts->range.u.max) {
            fprintf(stderr, "Error: Min value cannot be greater than max value\n");
            return -1;
        }
    }
    
    return 0;
}

static void print_binary(FILE *output, uint64_t value, const options_t *opts) {
    int required_bits;
    uint64_t normalized_value;
    
    if (opts->is_signed) {
        required_bits = calculate_required_bits(
            (uint64_t)(opts->range.s.max - opts->range.s.min),
            (uint64_t)(opts->range.s.max - opts->range.s.min)) + 1;
        normalized_value = (uint64_t)((int64_t)value - opts->range.s.min);
    } else {
        required_bits = calculate_required_bits(opts->range.u.min, opts->range.u.max);
        normalized_value = value - opts->range.u.min;
    }
    
    // Print bits in groups of 4
    int bits_printed = 0;
    for (int i = required_bits - 1; i >= 0; i--) {
        fprintf(output, "%llu", (normalized_value >> i) & 1);
        bits_printed++;
        // Add space after every 4 bits except at the end
        if (i > 0 && bits_printed % 4 == 0) {
            fprintf(output, " ");
        }
    }
    fprintf(output, " (%d bits)\n", required_bits);
}

int main(int argc, char *argv[]) {
    options_t opts;
    int result = parse_options(argc, argv, &opts);
    if (result != 0) {
        return result > 0 ? 0 : 1;
    }
    
    // Initialize QRNG
    qrng_ctx *ctx;
    uint8_t seed[32];
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f || fread(seed, 1, sizeof(seed), f) != sizeof(seed)) {
        fprintf(stderr, "Error: Could not read random seed\n");
        if (f) fclose(f);
        return 1;
    }
    fclose(f);
    
    if (qrng_init(&ctx, seed, sizeof(seed)) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: Could not initialize QRNG\n");
        return 1;
    }
    
    // Open output file if specified
    FILE *output = stdout;
    if (opts.output_file) {
        output = fopen(opts.output_file, "w");
        if (!output) {
            fprintf(stderr, "Error: Could not open output file '%s'\n", opts.output_file);
            qrng_free(ctx);
            return 1;
        }
    }
    
    // Generate and output random numbers
    uint64_t *numbers = NULL;
    if (opts.analyze) {
        numbers = (uint64_t *)malloc(opts.count * sizeof(uint64_t));
        if (!numbers) {
            fprintf(stderr, "Error: Could not allocate memory for analysis\n");
            if (output != stdout) fclose(output);
            qrng_free(ctx);
            return 1;
        }
    }
    
    for (uint64_t i = 0; i < opts.count; i++) {
        uint64_t value;
        if (opts.is_signed) {
            value = (uint64_t)qrng_range32(ctx, (int32_t)opts.range.s.min, (int32_t)opts.range.s.max);
        } else {
            value = qrng_range64(ctx, opts.range.u.min, opts.range.u.max);
        }
        
        if ((opts.is_signed && (int64_t)value > opts.range.s.max) ||
            (!opts.is_signed && value > opts.range.u.max)) {
            fprintf(stderr, "Error: QRNG range function failed\n");
            if (opts.analyze) free(numbers);
            if (output != stdout) fclose(output);
            qrng_free(ctx);
            return 1;
        }
        
        if (opts.analyze) {
            numbers[i] = value;
        }
        
        switch (opts.format) {
            case FORMAT_DECIMAL:
                if (opts.is_signed) {
                    fprintf(output, "%lld\n", (int64_t)value);
                } else {
                    fprintf(output, "%llu\n", value);
                }
                break;
            case FORMAT_HEX:
                fprintf(output, "0x%016llx\n", value);
                break;
            case FORMAT_BINARY:
                print_binary(output, value, &opts);
                break;
        }
    }
    
    // Perform analysis if requested
    if (opts.analyze) {
        analyze_numbers(numbers, opts.count, &opts);
        free(numbers);
    }
    
    // Cleanup
    if (output != stdout) {
        fclose(output);
    }
    qrng_free(ctx);
    
    return 0;
}

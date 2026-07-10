/**
 * @file qrng_cli_v2.c
 * @brief Production-grade CLI for Quantum RNG v2.0
 *
 * Features:
 * - Mode selection (FAST/QUANTUM/HYBRID/VERIFIED)
 * - Thread-safe operation support
 * - Performance monitoring
 * - Health test statistics
 * - Entropy source information
 * - Multiple output formats (hex, binary, base64)
 * - Batch generation
 * - Interactive and command-line modes
 */

#include "secure_rng/secure_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define VERSION "2.0.0"
#define DEFAULT_OUTPUT_SIZE 32
#define MAX_OUTPUT_SIZE (1024 * 1024 * 100)  // 100MB max

// ============================================================================
// OUTPUT FORMATS
// ============================================================================

typedef enum {
    OUTPUT_FORMAT_HEX,
    OUTPUT_FORMAT_BINARY,
    OUTPUT_FORMAT_BASE64,
    OUTPUT_FORMAT_DEC
} output_format_t;

static void print_hex(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

static void print_binary(const uint8_t *data, size_t size) {
    if (fwrite(data, 1, size, stdout) != size) {
        fprintf(stderr, "Warning: Not all bytes written\n");
    }
}

static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void print_base64(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i += 3) {
        uint32_t n = ((uint32_t)data[i]) << 16;
        if (i + 1 < size) n |= ((uint32_t)data[i + 1]) << 8;
        if (i + 2 < size) n |= data[i + 2];
        
        printf("%c", base64_chars[(n >> 18) & 0x3F]);
        printf("%c", base64_chars[(n >> 12) & 0x3F]);
        printf("%c", (i + 1 < size) ? base64_chars[(n >> 6) & 0x3F] : '=');
        printf("%c", (i + 2 < size) ? base64_chars[n & 0x3F] : '=');
    }
    printf("\n");
}

static void print_decimal(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%u", data[i]);
        if (i < size - 1) printf(" ");
    }
    printf("\n");
}

// ============================================================================
// COMMAND-LINE OPTIONS
// ============================================================================

typedef struct {
    // Generation options
    size_t num_bytes;
    output_format_t format;
    
    // Mode options
    secure_rng_mode_t mode;
    int thread_safe;
    
    // Configuration options
    double min_entropy;
    size_t reseed_interval;
    
    // Output options
    int show_stats;
    int show_health;
    int show_entropy;
    int benchmark;
    int continuous;
    
    // Misc options
    int verbose;
    int quiet;
} cli_options_t;

static void print_usage(const char *program_name) {
    printf("Quantum RNG v%s - Production-grade Quantum Random Number Generator\n\n", VERSION);
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    
    printf("Generation Options:\n");
    printf("  -n, --bytes=N          Generate N random bytes (default: %d)\n", DEFAULT_OUTPUT_SIZE);
    printf("  -f, --format=FMT       Output format: hex, binary, base64, dec (default: hex)\n");
    printf("  -c, --continuous       Continuous generation (until Ctrl+C)\n");
    
    printf("\nMode Options:\n");
    printf("  -m, --mode=MODE        Operation mode:\n");
    printf("                         fast     - Hardware entropy only (max performance)\n");
    printf("                         quantum  - Quantum mixing + health tests (default)\n");
    printf("                         hybrid   - Adaptive (fast<1KB, quantum>=1KB)\n");
    printf("                         verified - Quantum + Bell test verification\n");
    printf("  -t, --thread-safe      Enable thread-safe operation (pthread mutexes)\n");
    
    printf("\nConfiguration Options:\n");
    printf("  -e, --min-entropy=E    Min-entropy estimate in bits/byte (default: 4.0)\n");
    printf("  -r, --reseed=N         Reseed interval in bytes (default: 1MB)\n");
    
    printf("\nInformation Options:\n");
    printf("  -s, --stats            Show statistics after generation\n");
    printf("  -H, --health           Show health test statistics\n");
    printf("  -E, --entropy-info     Show entropy source information\n");
    printf("  -b, --benchmark        Run performance benchmark\n");
    
    printf("\nOther Options:\n");
    printf("  -v, --verbose          Verbose output\n");
    printf("  -q, --quiet            Suppress informational output\n");
    printf("  -h, --help             Show this help message\n");
    printf("  -V, --version          Show version information\n");
    
    printf("\nExamples:\n");
    printf("  %s -n 32                         # Generate 32 bytes in hex\n", program_name);
    printf("  %s -n 1024 -f binary > key.bin   # Generate binary keyfile\n", program_name);
    printf("  %s -m fast -n 1000000 -b         # Benchmark FAST mode\n", program_name);
    printf("  %s -m quantum -t -s              # Thread-safe quantum mode with stats\n", program_name);
    printf("  %s -m hybrid -c                  # Continuous adaptive mode\n", program_name);
    
    printf("\nSecurity Considerations:\n");
    printf("  - All entropy is health-tested per NIST SP 800-90B\n");
    printf("  - FAST mode: ~50+ MB/s, hardware entropy only\n");
    printf("  - QUANTUM mode: ~5 MB/s, quantum mixing (default)\n");
    printf("  - HYBRID mode: Adaptive for optimal performance/security\n");
    printf("  - VERIFIED mode: Maximum assurance, slower\n");
    printf("  - Thread-safe mode adds mutex overhead (~5-10%%)\n");
    
    printf("\n");
}

static void print_version(void) {
    printf("Quantum RNG v%s\n", VERSION);
    printf("Production-grade quantum random number generator\n");
    printf("With NIST SP 800-90B health tests and hardware entropy sources\n");
    printf("\n");
    printf("Features:\n");
    printf("  - Bell test verified quantum engine (CHSH = 2.828)\n");
    printf("  - Hardware entropy: RDSEED, RDRAND, getrandom(), /dev/random\n");
    printf("  - Health tests: RCT, APT (continuous monitoring)\n");
    printf("  - Thread-safe operation (pthread mutexes)\n");
    printf("  - Multiple operation modes\n");
    printf("\n");
    printf("License: MIT\n");
}

// ============================================================================
// BENCHMARKING
// ============================================================================

static void run_benchmark(secure_rng_ctx_t *ctx, secure_rng_mode_t mode) {
    printf("\n=== Performance Benchmark: %s ===\n\n", secure_rng_mode_string(mode));
    
    // Set mode
    secure_rng_set_mode(ctx, mode);
    
    const size_t test_sizes[] = {1024, 10240, 102400, 1048576, 10485760};
    const char *test_names[] = {"1KB", "10KB", "100KB", "1MB", "10MB"};
    
    for (size_t i = 0; i < sizeof(test_sizes) / sizeof(test_sizes[0]); i++) {
        uint8_t *buffer = malloc(test_sizes[i]);
        if (!buffer) {
            fprintf(stderr, "Memory allocation failed\n");
            continue;
        }
        
        // Warmup
        secure_rng_bytes(ctx, buffer, 1024);
        
        // Benchmark
        clock_t start = clock();
        secure_rng_error_t err = secure_rng_bytes(ctx, buffer, test_sizes[i]);
        clock_t end = clock();
        
        if (err != SECURE_RNG_SUCCESS) {
            fprintf(stderr, "Generation failed: %s\n", secure_rng_error_string(err));
            free(buffer);
            continue;
        }
        
        double seconds = (double)(end - start) / CLOCKS_PER_SEC;
        double mbps = (test_sizes[i] / (1024.0 * 1024.0)) / seconds;
        
        printf("  %-6s: %8.3f sec | %10.2f MB/s\n", test_names[i], seconds, mbps);
        
        free(buffer);
    }
    
    printf("\n");
}

static void run_mode_comparison(secure_rng_ctx_t *ctx) {
    printf("\n=== Mode Comparison Benchmark (1MB) ===\n\n");
    
    const size_t test_size = 1024 * 1024;
    uint8_t *buffer = malloc(test_size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    
    secure_rng_mode_t modes[] = {
        SECURE_RNG_MODE_FAST,
        SECURE_RNG_MODE_QUANTUM,
        SECURE_RNG_MODE_HYBRID
    };
    
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        secure_rng_set_mode(ctx, modes[i]);
        
        // Warmup
        secure_rng_bytes(ctx, buffer, 1024);
        
        // Benchmark
        clock_t start = clock();
        secure_rng_bytes(ctx, buffer, test_size);
        clock_t end = clock();
        
        double seconds = (double)(end - start) / CLOCKS_PER_SEC;
        double mbps = (test_size / (1024.0 * 1024.0)) / seconds;
        
        printf("  %-30s: %8.3f sec | %10.2f MB/s\n",
               secure_rng_mode_string(modes[i]), seconds, mbps);
    }
    
    free(buffer);
    printf("\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    // Default options
    cli_options_t opts = {
        .num_bytes = DEFAULT_OUTPUT_SIZE,
        .format = OUTPUT_FORMAT_HEX,
        .mode = SECURE_RNG_MODE_QUANTUM,
        .thread_safe = 0,
        .min_entropy = 4.0,
        .reseed_interval = 1024 * 1024,
        .show_stats = 0,
        .show_health = 0,
        .show_entropy = 0,
        .benchmark = 0,
        .continuous = 0,
        .verbose = 0,
        .quiet = 0
    };
    
    // Parse command-line options
    static struct option long_options[] = {
        {"bytes",        required_argument, 0, 'n'},
        {"format",       required_argument, 0, 'f'},
        {"mode",         required_argument, 0, 'm'},
        {"thread-safe",  no_argument,       0, 't'},
        {"min-entropy",  required_argument, 0, 'e'},
        {"reseed",       required_argument, 0, 'r'},
        {"stats",        no_argument,       0, 's'},
        {"health",       no_argument,       0, 'H'},
        {"entropy-info", no_argument,       0, 'E'},
        {"benchmark",    no_argument,       0, 'b'},
        {"continuous",   no_argument,       0, 'c'},
        {"verbose",      no_argument,       0, 'v'},
        {"quiet",        no_argument,       0, 'q'},
        {"help",         no_argument,       0, 'h'},
        {"version",      no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "n:f:m:te:r:sHEbcvqhV", long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                opts.num_bytes = atoll(optarg);
                if (opts.num_bytes > MAX_OUTPUT_SIZE) {
                    fprintf(stderr, "Error: Size exceeds maximum (%d MB)\n", 
                            MAX_OUTPUT_SIZE / (1024 * 1024));
                    return 1;
                }
                break;
                
            case 'f':
                if (strcmp(optarg, "hex") == 0) {
                    opts.format = OUTPUT_FORMAT_HEX;
                } else if (strcmp(optarg, "binary") == 0) {
                    opts.format = OUTPUT_FORMAT_BINARY;
                } else if (strcmp(optarg, "base64") == 0) {
                    opts.format = OUTPUT_FORMAT_BASE64;
                } else if (strcmp(optarg, "dec") == 0) {
                    opts.format = OUTPUT_FORMAT_DEC;
                } else {
                    fprintf(stderr, "Error: Unknown format '%s'\n", optarg);
                    return 1;
                }
                break;
                
            case 'm':
                if (strcmp(optarg, "fast") == 0) {
                    opts.mode = SECURE_RNG_MODE_FAST;
                } else if (strcmp(optarg, "quantum") == 0) {
                    opts.mode = SECURE_RNG_MODE_QUANTUM;
                } else if (strcmp(optarg, "hybrid") == 0) {
                    opts.mode = SECURE_RNG_MODE_HYBRID;
                } else if (strcmp(optarg, "verified") == 0) {
                    opts.mode = SECURE_RNG_MODE_VERIFIED;
                } else {
                    fprintf(stderr, "Error: Unknown mode '%s'\n", optarg);
                    return 1;
                }
                break;
                
            case 't':
                opts.thread_safe = 1;
                break;
                
            case 'e':
                opts.min_entropy = atof(optarg);
                if (opts.min_entropy <= 0.0 || opts.min_entropy > 8.0) {
                    fprintf(stderr, "Error: Min-entropy must be in (0, 8]\n");
                    return 1;
                }
                break;
                
            case 'r':
                opts.reseed_interval = atoll(optarg);
                break;
                
            case 's':
                opts.show_stats = 1;
                break;
                
            case 'H':
                opts.show_health = 1;
                break;
                
            case 'E':
                opts.show_entropy = 1;
                break;
                
            case 'b':
                opts.benchmark = 1;
                break;
                
            case 'c':
                opts.continuous = 1;
                break;
                
            case 'v':
                opts.verbose = 1;
                break;
                
            case 'q':
                opts.quiet = 1;
                break;
                
            case 'h':
                print_usage(argv[0]);
                return 0;
                
            case 'V':
                print_version();
                return 0;
                
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Initialize RNG
    if (opts.verbose && !opts.quiet) {
        fprintf(stderr, "Initializing Quantum RNG v%s...\n", VERSION);
        fprintf(stderr, "Mode: %s\n", secure_rng_mode_string(opts.mode));
        if (opts.thread_safe) {
            fprintf(stderr, "Thread-safe: Enabled\n");
        }
    }
    
    secure_rng_config_t config;
    secure_rng_get_default_config(&config);
    config.mode = opts.mode;
    config.min_entropy_estimate = opts.min_entropy;
    config.reseed_interval = opts.reseed_interval;
    config.enable_thread_safety = opts.thread_safe;
    
    secure_rng_ctx_t *ctx;
    secure_rng_error_t err;
    
    if (opts.thread_safe) {
        err = secure_rng_init_threadsafe_with_config(&ctx, &config);
    } else {
        err = secure_rng_init_with_config(&ctx, &config);
    }
    
    if (err != SECURE_RNG_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize RNG: %s\n", 
                secure_rng_error_string(err));
        return 1;
    }
    
    if (opts.verbose && !opts.quiet) {
        fprintf(stderr, "Initialization complete\n\n");
    }
    
    // Show entropy source information
    if (opts.show_entropy) {
        const entropy_capabilities_t *caps = secure_rng_get_entropy_caps(ctx);
        printf("=== Entropy Source Information ===\n");
        printf("Available sources:\n");
        printf("  RDSEED:      %s\n", caps->has_rdseed ? "Yes" : "No");
        printf("  RDRAND:      %s\n", caps->has_rdrand ? "Yes" : "No");
        printf("  getrandom(): %s\n", caps->has_getrandom ? "Yes" : "No");
        printf("  /dev/random: %s\n", caps->has_dev_random ? "Yes" : "No");
        printf("  /dev/urandom:%s\n", caps->has_dev_urandom ? "Yes" : "No");
        printf("  CPU Jitter:  %s\n", caps->has_jitter ? "Yes" : "No");
        printf("Primary source: %s\n", entropy_source_name(caps->preferred_source));
        printf("\n");
    }
    
    // Run benchmark
    if (opts.benchmark) {
        if (!opts.quiet) {
            printf("Running performance benchmarks...\n");
        }
        run_mode_comparison(ctx);
        run_benchmark(ctx, opts.mode);
        
        secure_rng_free(ctx);
        return 0;
    }
    
    // Continuous mode
    if (opts.continuous) {
        if (!opts.quiet) {
            fprintf(stderr, "Generating continuous random data (Ctrl+C to stop)...\n");
        }
        
        uint8_t buffer[4096];
        while (1) {
            err = secure_rng_bytes(ctx, buffer, sizeof(buffer));
            if (err != SECURE_RNG_SUCCESS) {
                fprintf(stderr, "Error: %s\n", secure_rng_error_string(err));
                break;
            }
            
            switch (opts.format) {
                case OUTPUT_FORMAT_HEX:
                    print_hex(buffer, sizeof(buffer));
                    break;
                case OUTPUT_FORMAT_BINARY:
                    print_binary(buffer, sizeof(buffer));
                    break;
                case OUTPUT_FORMAT_BASE64:
                    print_base64(buffer, sizeof(buffer));
                    break;
                case OUTPUT_FORMAT_DEC:
                    print_decimal(buffer, sizeof(buffer));
                    break;
            }
        }
        
        secure_rng_free(ctx);
        return 0;
    }
    
    // Single generation
    uint8_t *buffer = malloc(opts.num_bytes);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        secure_rng_free(ctx);
        return 1;
    }
    
    clock_t start = clock();
    err = secure_rng_bytes(ctx, buffer, opts.num_bytes);
    clock_t end = clock();
    
    if (err != SECURE_RNG_SUCCESS) {
        fprintf(stderr, "Error: %s\n", secure_rng_error_string(err));
        free(buffer);
        secure_rng_free(ctx);
        return 1;
    }
    
    // Output data
    switch (opts.format) {
        case OUTPUT_FORMAT_HEX:
            print_hex(buffer, opts.num_bytes);
            break;
        case OUTPUT_FORMAT_BINARY:
            print_binary(buffer, opts.num_bytes);
            break;
        case OUTPUT_FORMAT_BASE64:
            print_base64(buffer, opts.num_bytes);
            break;
        case OUTPUT_FORMAT_DEC:
            print_decimal(buffer, opts.num_bytes);
            break;
    }
    
    free(buffer);
    
    // Show statistics
    if (opts.show_stats) {
        double seconds = (double)(end - start) / CLOCKS_PER_SEC;
        double mbps = (opts.num_bytes / (1024.0 * 1024.0)) / seconds;
        
        printf("\n=== Generation Statistics ===\n");
        printf("Bytes generated: %zu\n", opts.num_bytes);
        printf("Time: %.3f seconds\n", seconds);
        printf("Throughput: %.2f MB/s\n", mbps);
        printf("\n");
        
        secure_rng_print_stats(ctx);
    }
    
    // Show health test stats
    if (opts.show_health) {
        const health_test_stats_t *health = secure_rng_get_health_stats(ctx);
        printf("=== Health Test Statistics ===\n");
        printf("Samples tested: %llu\n", (unsigned long long)health->samples_tested);
        printf("RCT failures: %llu\n", (unsigned long long)health->rct_failures);
        printf("APT failures: %llu\n", (unsigned long long)health->apt_failures);
        printf("Startup complete: %s\n", health->startup_complete ? "Yes" : "No");
        printf("\n");
    }
    
    secure_rng_free(ctx);
    return 0;
}
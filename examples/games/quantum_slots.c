#include "quantum_slots.h"
#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>

// Global symbol information
const symbol_info_t SYMBOLS[NUM_SYMBOLS] = {
    {SYMBOL_CHERRY,  WEIGHT_CHERRY,  PAYOUT_CHERRY},
    {SYMBOL_LEMON,   WEIGHT_LEMON,   PAYOUT_LEMON},
    {SYMBOL_ORANGE,  WEIGHT_ORANGE,  PAYOUT_ORANGE},
    {SYMBOL_GRAPES,  WEIGHT_GRAPES,  PAYOUT_GRAPES},
    {SYMBOL_STAR,    WEIGHT_STAR,    PAYOUT_STAR},
    {SYMBOL_DIAMOND, WEIGHT_DIAMOND, PAYOUT_DIAMOND},
    {SYMBOL_SEVEN,   WEIGHT_SEVEN,   PAYOUT_SEVEN}
};

void init_slots_config(slots_config_t *config) {
    config->starting_credits = DEFAULT_CREDITS;
    config->bet_amount = DEFAULT_BET;
    strncpy(config->seed, DEFAULT_SEED, sizeof(config->seed) - 1);
    config->seed_length = DEFAULT_SEED_LENGTH;
    config->output_mode = OUTPUT_NORMAL;
    config->verify_fairness = 0;
    config->show_animation = 1;
    config->interactive = 0;
    config->output_file[0] = '\0';
    config->auto_spins = 0;
}

void print_slots_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -c, --credits N     Starting credits (default: %d)\n", DEFAULT_CREDITS);
    printf("  -b, --bet N         Bet amount (%d-%d, default: %d)\n", 
           MIN_BET, MAX_BET, DEFAULT_BET);
    printf("  -s, --seed STRING   Random seed (default: \"%s\")\n", DEFAULT_SEED);
    printf("  -q, --quiet         Quiet mode (only output numbers)\n");
    printf("  -v, --verbose       Verbose mode (show additional statistics)\n");
    printf("  -j, --json          Output results in JSON format\n");
    printf("  -a, --no-animation  Disable spin animation\n");
    printf("  -f, --fairness      Verify fairness with statistical tests\n");
    printf("  -i, --interactive   Run in interactive mode\n");
    printf("  -n, --num-spins N   Number of automatic spins (0 for interactive)\n");
    printf("  -o, --output FILE   Write output to file\n");
    printf("  -h, --help          Show this help message\n");
}

void parse_slots_args(int argc, char *argv[], slots_config_t *config) {
    static struct option long_options[] = {
        {"credits",     required_argument, 0, 'c'},
        {"bet",         required_argument, 0, 'b'},
        {"seed",        required_argument, 0, 's'},
        {"quiet",       no_argument,       0, 'q'},
        {"verbose",     no_argument,       0, 'v'},
        {"json",        no_argument,       0, 'j'},
        {"no-animation",no_argument,       0, 'a'},
        {"fairness",    no_argument,       0, 'f'},
        {"interactive", no_argument,       0, 'i'},
        {"num-spins",   required_argument, 0, 'n'},
        {"output",      required_argument, 0, 'o'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "c:b:s:qvjafin:o:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                config->starting_credits = atoi(optarg);
                if (config->starting_credits <= 0) {
                    fprintf(stderr, "Error: Starting credits must be positive\n");
                    exit(1);
                }
                break;

            case 'b':
                config->bet_amount = atoi(optarg);
                if (config->bet_amount < MIN_BET || config->bet_amount > MAX_BET) {
                    fprintf(stderr, "Error: Bet amount must be between %d and %d\n",
                            MIN_BET, MAX_BET);
                    exit(1);
                }
                break;

            case 's':
                strncpy(config->seed, optarg, sizeof(config->seed) - 1);
                config->seed_length = strlen(config->seed);
                break;

            case 'q':
                config->output_mode = OUTPUT_QUIET;
                break;

            case 'v':
                config->output_mode = OUTPUT_VERBOSE;
                break;

            case 'j':
                config->output_mode = OUTPUT_JSON;
                break;

            case 'a':
                config->show_animation = 0;
                break;

            case 'f':
                config->verify_fairness = 1;
                break;

            case 'i':
                config->interactive = 1;
                break;

            case 'n':
                config->auto_spins = atoi(optarg);
                if (config->auto_spins < 0) {
                    fprintf(stderr, "Error: Number of spins must be non-negative\n");
                    exit(1);
                }
                break;

            case 'o':
                strncpy(config->output_file, optarg, sizeof(config->output_file) - 1);
                break;

            case 'h':
                print_slots_usage(argv[0]);
                exit(0);

            case '?':
                exit(1);

            default:
                fprintf(stderr, "Error: Unknown option %c\n", c);
                exit(1);
        }
    }
}

int select_symbol(qrng_ctx *ctx) {
    int total_weight = 0;
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        total_weight += SYMBOLS[i].weight;
    }

    int rand_val = qrng_range32(ctx, 0, total_weight - 1);
    int cumulative = 0;

    for (int i = 0; i < NUM_SYMBOLS; i++) {
        cumulative += SYMBOLS[i].weight;
        if (rand_val < cumulative) {
            return i;
        }
    }

    return NUM_SYMBOLS - 1;  // Fallback
}

void animate_spin(const slots_config_t *config, const int *results) {
    if (!config->show_animation) return;

    printf("\033[2J\033[H");  // Clear screen
    printf("🎰 Quantum Slot Machine 🎰\n");
    printf("========================\n\n");

    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)config->seed, config->seed_length);

    for (int frame = 0; frame < SPIN_FRAMES; frame++) {
        printf("\r[");
        for (int i = 0; i < NUM_REELS; i++) {
            if (frame >= SPIN_FRAMES - NUM_REELS + i) {
                printf(" %s ", SYMBOLS[results[i]].symbol);
            } else {
                int symbol = qrng_range32(ctx, 0, NUM_SYMBOLS - 1);
                printf(" %s ", SYMBOLS[symbol].symbol);
            }
        }
        printf("]");
        fflush(stdout);
        usleep(FRAME_DELAY_MS * 1000);
    }
    printf("\n");

    qrng_free(ctx);
}

int calculate_payout(const int *results) {
    // Check for three matching symbols
    if (results[0] == results[1] && results[1] == results[2]) {
        return SYMBOLS[results[0]].payout;
    }
    return 0;
}

void verify_slots_fairness(const slots_config_t *config) {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)config->seed, config->seed_length);

    printf("\nVerifying Fairness (sampling %d spins):\n", MAX_SPINS);
    printf("======================================\n");

    int *symbol_counts = calloc(NUM_SYMBOLS, sizeof(int));
    int total_wins = 0;
    int total_payout = 0;

    // Run simulation
    for (int i = 0; i < MAX_SPINS; i++) {
        int results[NUM_REELS];
        for (int j = 0; j < NUM_REELS; j++) {
            results[j] = select_symbol(ctx);
            symbol_counts[results[j]]++;
        }

        int payout = calculate_payout(results);
        if (payout > 0) {
            total_wins++;
            total_payout += payout;
        }
    }

    // Print distribution for each symbol
    printf("\nSymbol Distribution:\n");
    printf("Symbol    | Count     | Expected  | Deviation\n");
    printf("----------|-----------|-----------|----------\n");

    int total_weight = 0;
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        total_weight += SYMBOLS[i].weight;
    }

    for (int i = 0; i < NUM_SYMBOLS; i++) {
        double expected = (double)MAX_SPINS * NUM_REELS * SYMBOLS[i].weight / total_weight;
        double deviation = (symbol_counts[i] - expected) / sqrt(expected);

        printf("%-9s | %9d | %9.1f | %+.2f σ\n",
               SYMBOLS[i].symbol,
               symbol_counts[i],
               expected,
               deviation);
    }

    // Print win statistics
    printf("\nWin Statistics:\n");
    printf("Total spins:     %d\n", MAX_SPINS);
    printf("Winning spins:   %d (%.2f%%)\n", 
           total_wins, 100.0 * total_wins / MAX_SPINS);
    printf("Average payout:  %.2f x bet\n", 
           (double)total_payout / MAX_SPINS);
    printf("Return to player: %.2f%%\n", 
           100.0 * total_payout / (MAX_SPINS * config->bet_amount));

    free(symbol_counts);
    qrng_free(ctx);
}

void output_results_json(const int *results, int spins, const slots_config_t *config) {
    printf("{\n");
    printf("  \"config\": {\n");
    printf("    \"bet\": %d,\n", config->bet_amount);
    printf("    \"spins\": %d,\n", spins);
    printf("    \"seed\": \"%s\"\n", config->seed);
    printf("  },\n");
    printf("  \"results\": [\n");

    for (int i = 0; i < NUM_REELS; i++) {
        printf("    {\n");
        printf("      \"symbol\": \"%s\",\n", SYMBOLS[results[i]].symbol);
        printf("      \"position\": %d\n", i + 1);
        printf("    }%s\n", i < NUM_REELS - 1 ? "," : "");
    }

    printf("  ],\n");
    printf("  \"payout\": %d\n", calculate_payout(results));
    printf("}\n");
}

void run_slots(const slots_config_t *config) {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)config->seed, config->seed_length);

    FILE *output = config->output_file[0] ? fopen(config->output_file, "w") : stdout;
    if (!output) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", config->output_file);
        qrng_free(ctx);
        return;
    }

    int credits = config->starting_credits;
    int spins = 0;
    int total_won = 0;

    while (credits >= config->bet_amount && 
           (config->auto_spins == 0 || spins < config->auto_spins)) {
        int results[NUM_REELS];
        
        // Spin reels
        for (int i = 0; i < NUM_REELS; i++) {
            results[i] = select_symbol(ctx);
        }

        credits -= config->bet_amount;
        spins++;

        // Show animation
        animate_spin(config, results);

        // Calculate and apply payout
        int payout = calculate_payout(results) * config->bet_amount;
        credits += payout;
        total_won += payout;

        // Output results based on mode
        switch (config->output_mode) {
            case OUTPUT_QUIET:
                fprintf(output, "%d,%d,%d,%d\n", 
                        results[0], results[1], results[2], payout);
                break;

            case OUTPUT_JSON:
                output_results_json(results, spins, config);
                break;

            case OUTPUT_VERBOSE:
            case OUTPUT_NORMAL:
                if (payout > 0) {
                    fprintf(output, "\n🎉 Winner! %d credits!\n", payout);
                } else {
                    fprintf(output, "\nTry again!\n");
                }
                fprintf(output, "Credits: %d\n", credits);
                
                if (config->output_mode == OUTPUT_VERBOSE) {
                    fprintf(output, "Total won: %d\n", total_won);
                    fprintf(output, "Spins: %d\n", spins);
                }
                break;
        }
    }

    if (config->verify_fairness) {
        verify_slots_fairness(config);
    }

    if (output != stdout) {
        fclose(output);
    }
    qrng_free(ctx);
}

void run_interactive_mode(const slots_config_t *config) {
    slots_config_t interactive_config = *config;
    char input[256];

    printf("Quantum Slots Interactive Mode\n");
    printf("============================\n");

    while (1) {
        printf("\nCredits: %d | Bet: %d\n", 
               interactive_config.starting_credits, 
               interactive_config.bet_amount);
        printf("Press Enter to spin (or 'q' to quit): ");
        
        fgets(input, sizeof(input), stdin);
        if (input[0] == 'q' || input[0] == 'Q') break;

        run_slots(&interactive_config);
    }
}

int main(int argc, char *argv[]) {
    slots_config_t config;
    init_slots_config(&config);
    parse_slots_args(argc, argv, &config);

    if (config.interactive) {
        run_interactive_mode(&config);
    } else {
        run_slots(&config);
    }

    return 0;
}

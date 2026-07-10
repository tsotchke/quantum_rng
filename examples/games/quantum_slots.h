#ifndef QUANTUM_SLOTS_H
#define QUANTUM_SLOTS_H

// Game configuration
#define NUM_REELS 3
#define NUM_SYMBOLS 7
#define MAX_SPINS 1000000
#define DEFAULT_CREDITS 100
#define DEFAULT_BET 1
#define MIN_BET 1
#define MAX_BET 100
#define DEFAULT_SEED "quantum_slots"
#define DEFAULT_SEED_LENGTH 12

// Symbol definitions
#define SYMBOL_CHERRY  "🍒"
#define SYMBOL_LEMON   "🍋"
#define SYMBOL_ORANGE  "🍊"
#define SYMBOL_GRAPES  "🍇"
#define SYMBOL_STAR    "⭐"
#define SYMBOL_DIAMOND "💎"
#define SYMBOL_SEVEN   "7️⃣"

// Payout multipliers
#define PAYOUT_SEVEN   100
#define PAYOUT_DIAMOND 50
#define PAYOUT_STAR    25
#define PAYOUT_GRAPES  15
#define PAYOUT_ORANGE  10
#define PAYOUT_LEMON   5
#define PAYOUT_CHERRY  3

// Symbol weights (higher = more common)
#define WEIGHT_CHERRY  32
#define WEIGHT_LEMON   24
#define WEIGHT_ORANGE  16
#define WEIGHT_GRAPES  12
#define WEIGHT_STAR    8
#define WEIGHT_DIAMOND 4
#define WEIGHT_SEVEN   2

// Animation settings
#define SPIN_FRAMES 20
#define FRAME_DELAY_MS 100

// Output modes
typedef enum {
    OUTPUT_NORMAL,
    OUTPUT_QUIET,
    OUTPUT_VERBOSE,
    OUTPUT_JSON
} output_mode_t;

// Slot machine configuration
typedef struct {
    int starting_credits;
    int bet_amount;
    char seed[256];
    int seed_length;
    output_mode_t output_mode;
    int verify_fairness;
    int show_animation;
    int interactive;
    char output_file[1024];
    int auto_spins;
} slots_config_t;

// Symbol information
typedef struct {
    const char *symbol;
    int weight;
    int payout;
} symbol_info_t;

// Function declarations
void init_slots_config(slots_config_t *config);
void parse_slots_args(int argc, char *argv[], slots_config_t *config);
void print_slots_usage(const char *program_name);
void run_slots(const slots_config_t *config);
void verify_slots_fairness(const slots_config_t *config);
void run_interactive_mode(const slots_config_t *config);
void output_results_json(const int *results, int spins, const slots_config_t *config);
void animate_spin(const slots_config_t *config, const int *results);
int calculate_payout(const int *results);

// Global symbol information
extern const symbol_info_t SYMBOLS[NUM_SYMBOLS];

#endif /* QUANTUM_SLOTS_H */

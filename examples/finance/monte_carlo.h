#ifndef MONTE_CARLO_H
#define MONTE_CARLO_H

#include <stdio.h>

// Simulation parameters
#define DEFAULT_NUM_SIMULATIONS 100000
#define MIN_SIMULATIONS 1000
#define MAX_SIMULATIONS 10000000
#define DEFAULT_TRADING_DAYS 252  // Standard trading days in a year

// Default asset parameters
#define DEFAULT_INITIAL_PRICE 100.0
#define DEFAULT_VOLATILITY 0.2     // 20% annual volatility
#define DEFAULT_RISK_FREE_RATE 0.05  // 5% annual risk-free rate
#define DEFAULT_DIVIDEND_YIELD 0.02  // 2% annual dividend yield

// Confidence levels
#define CONFIDENCE_95 1.96  // 95% confidence interval z-score
#define CONFIDENCE_99 2.576 // 99% confidence interval z-score

// Output modes
typedef enum {
    OUTPUT_NORMAL,
    OUTPUT_QUIET,
    OUTPUT_VERBOSE,
    OUTPUT_JSON,
    OUTPUT_CSV
} output_mode_t;

// Asset parameters structure
typedef struct {
    double initial_price;
    double volatility;
    double risk_free_rate;
    double dividend_yield;
} asset_params_t;

// Simulation configuration
typedef struct {
    int num_simulations;
    int trading_days;
    asset_params_t asset;
    char seed[256];
    int seed_length;
    output_mode_t output_mode;
    int show_progress;
    char output_file[1024];
    double confidence_level;
} simulation_config_t;

// Simulation results
typedef struct {
    double mean_price;
    double std_dev;
    double min_price;
    double max_price;
    double confidence_lower;
    double confidence_upper;
    double *prices;  // Array of final prices for detailed analysis
} simulation_results_t;

// Function declarations
void init_simulation_config(simulation_config_t *config);
void parse_simulation_args(int argc, char *argv[], simulation_config_t *config);
void print_simulation_usage(const char *program_name);
simulation_results_t run_simulation(const simulation_config_t *config);
void print_results(FILE *output, const simulation_results_t *results, const simulation_config_t *config);
void output_results_json(FILE *output, const simulation_results_t *results, const simulation_config_t *config);
void output_results_csv(FILE *output, const simulation_results_t *results, const simulation_config_t *config);
void free_simulation_results(simulation_results_t *results);

#endif /* MONTE_CARLO_H */

#ifndef OPTIONS_PRICING_H
#define OPTIONS_PRICING_H

#include <stdio.h>

// Simulation parameters
#define DEFAULT_NUM_PATHS 10000  // Reduced from 100000
#define MIN_PATHS 1000
#define MAX_PATHS 1000000       // Reduced from 10000000
#define DEFAULT_TIME_STEPS 252   // Daily steps for a year
#define GREEK_PATHS_MULTIPLIER 0.2  // Use 20% of paths for Greeks

// Default option parameters
#define DEFAULT_SPOT_PRICE 100.0
#define DEFAULT_STRIKE_PRICE 100.0
#define DEFAULT_TIME_TO_MATURITY 1.0  // 1 year
#define DEFAULT_VOLATILITY 0.2        // 20% annual volatility
#define DEFAULT_RISK_FREE_RATE 0.05   // 5% annual risk-free rate
#define DEFAULT_DIVIDEND_YIELD 0.02   // 2% annual dividend yield

// Default Heston model parameters
#define DEFAULT_KAPPA 2.0
#define DEFAULT_THETA 0.04
#define DEFAULT_SIGMA 0.3
#define DEFAULT_RHO -0.7
#define DEFAULT_V0 0.04

// Greeks flags
#define GREEK_NONE  0x00
#define GREEK_DELTA 0x01
#define GREEK_GAMMA 0x02
#define GREEK_THETA 0x04
#define GREEK_VEGA  0x08
#define GREEK_RHO   0x10
#define GREEK_ALL   0x1F

// Option types
typedef enum {
    OPTION_CALL,
    OPTION_PUT,
    OPTION_BINARY_CALL,
    OPTION_BINARY_PUT,
    OPTION_ASIAN_CALL,
    OPTION_ASIAN_PUT,
    OPTION_LOOKBACK_CALL,
    OPTION_LOOKBACK_PUT
} option_type_t;

// Volatility model types
typedef enum {
    VOL_CONSTANT,    // Black-Scholes constant volatility
    VOL_HESTON       // Heston stochastic volatility
} volatility_model_t;

// Output modes
typedef enum {
    OUTPUT_NORMAL,
    OUTPUT_QUIET,
    OUTPUT_VERBOSE,
    OUTPUT_JSON,
    OUTPUT_CSV
} output_mode_t;

// Heston model parameters
typedef struct {
    double kappa;        // Mean reversion rate
    double theta;        // Long-term variance
    double sigma;        // Volatility of variance
    double rho;         // Correlation between asset and variance
    double v0;          // Initial variance
} heston_params_t;

// Option parameters structure
typedef struct {
    double spot_price;
    double strike_price;
    double time_to_maturity;
    double volatility;
    double risk_free_rate;
    double dividend_yield;
    option_type_t type;
} option_params_t;

// Greeks structure
typedef struct {
    double delta;  // Price sensitivity to underlying
    double gamma;  // Delta sensitivity to underlying
    double theta;  // Price sensitivity to time
    double vega;   // Price sensitivity to volatility
    double rho;    // Price sensitivity to interest rate
} greeks_t;

// Simulation configuration
typedef struct {
    int num_paths;
    int time_steps;
    option_params_t option;
    char seed[256];
    int seed_length;
    output_mode_t output_mode;
    int show_progress;
    char output_file[1024];
    int greek_flags;      // Bitmask of Greeks to calculate
    int show_paths;       // Show individual price paths
    volatility_model_t vol_model;  // Volatility model selection
    heston_params_t heston;        // Heston model parameters (if used)
} pricing_config_t;

// Simulation results
typedef struct {
    double price;
    double std_error;
    double confidence_lower;
    double confidence_upper;
    greeks_t greeks;
    double *paths;  // Array of price paths for analysis
    int paths_size;
} pricing_results_t;

// Function declarations
void init_pricing_config(pricing_config_t *config);
pricing_results_t run_pricing_simulation(const pricing_config_t *config);
double black_scholes_price(const option_params_t *params);
void black_scholes_greeks(const option_params_t *params, greeks_t *greeks);
void calculate_greeks(pricing_results_t *results, const pricing_config_t *config);
void print_results(FILE *output, const pricing_results_t *results, const pricing_config_t *config);
void output_results_json(FILE *output, const pricing_results_t *results, const pricing_config_t *config);
void output_results_csv(FILE *output, const pricing_results_t *results, const pricing_config_t *config);
void free_pricing_results(pricing_results_t *results);
const char* get_option_type_name(option_type_t type);
double calculate_payoff(double spot, double strike, option_type_t type);

#endif /* OPTIONS_PRICING_H */

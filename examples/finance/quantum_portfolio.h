#ifndef QUANTUM_PORTFOLIO_H
#define QUANTUM_PORTFOLIO_H

#include <stdio.h>

// Portfolio parameters
#define MAX_ASSETS 100
#define DEFAULT_NUM_ASSETS 10
// The quantum RNG produces ~0.6M doubles/second; each simulation consumes
// time_horizon * num_assets normal draws, so the default keeps a full run
// in the tens of seconds rather than minutes.
#define DEFAULT_NUM_SIMULATIONS 2000
#define MIN_SIMULATIONS 1000
#define MAX_SIMULATIONS 10000000
#define DEFAULT_TIME_HORIZON 252  // Trading days in a year

// Default asset parameters
#define DEFAULT_RISK_FREE_RATE 0.05  // 5% annual risk-free rate
#define DEFAULT_TARGET_RETURN 0.15   // 15% target annual return
#define DEFAULT_RISK_TOLERANCE 0.5   // Risk tolerance parameter

// Optimization parameters
#define MAX_ITERATIONS 1000
#define FRONTIER_ITERATIONS 200   // GA generations per efficient-frontier point
#define CONVERGENCE_THRESHOLD 1e-6
#define POPULATION_SIZE 100
#define MUTATION_RATE 0.1

// Output modes
typedef enum {
    OUTPUT_NORMAL,
    OUTPUT_QUIET,
    OUTPUT_VERBOSE,
    OUTPUT_JSON,
    OUTPUT_CSV
} output_mode_t;

// Asset information
typedef struct {
    char name[64];
    double expected_return;
    double volatility;
    double *correlations;  // Correlation with other assets
    double weight;         // Current portfolio weight (pre-optimization)
} asset_t;

// Portfolio configuration
typedef struct {
    int num_assets;
    int num_simulations;
    int time_horizon;
    double risk_free_rate;
    double target_return;
    double risk_tolerance;
    asset_t *assets;
    char seed[256];
    int seed_length;
    output_mode_t output_mode;
    int show_progress;
    char output_file[1024];
    int show_efficient_frontier;
    int show_rebalancing;
} portfolio_config_t;

// Portfolio metrics
typedef struct {
    double expected_return;
    double volatility;
    double sharpe_ratio;
    double var_95;         // 95% Value at Risk
    double cvar_95;        // Conditional VaR
    double max_drawdown;   // Expected (mean) max drawdown across simulated paths
    double *efficient_frontier_returns;
    double *efficient_frontier_risks;
    int frontier_points;
} portfolio_metrics_t;

// Portfolio results
typedef struct {
    double *weights;
    portfolio_metrics_t metrics;
    double *simulated_returns;
    int num_simulations;
} portfolio_results_t;

// Function declarations
void init_portfolio_config(portfolio_config_t *config);
void free_portfolio_config(portfolio_config_t *config);
void parse_portfolio_args(int argc, char *argv[], portfolio_config_t *config);
void print_portfolio_usage(const char *program_name);
portfolio_results_t optimize_portfolio(const portfolio_config_t *config);
void calculate_portfolio_metrics(portfolio_metrics_t *metrics, const double *weights,
                                 const portfolio_config_t *config);
void print_results(FILE *output, const portfolio_results_t *results,
                   const portfolio_config_t *config);
void output_results_json(FILE *output, const portfolio_results_t *results,
                         const portfolio_config_t *config);
void output_results_csv(FILE *output, const portfolio_results_t *results,
                        const portfolio_config_t *config);
void free_portfolio_results(portfolio_results_t *results);
void generate_efficient_frontier(portfolio_results_t *results, const portfolio_config_t *config);
void calculate_rebalancing(FILE *output, const portfolio_results_t *results,
                           const portfolio_config_t *config);
int compare_doubles(const void *a, const void *b);

#endif /* QUANTUM_PORTFOLIO_H */

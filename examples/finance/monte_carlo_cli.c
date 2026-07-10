/*
 * monte_carlo_cli.c - Command-line front end for the quantum-RNG Monte Carlo
 * asset-price simulator (monte_carlo.c).
 *
 * This file only provides main(), argument parsing and dispatch to the
 * library.  All numerical work is done by the functions declared in
 * monte_carlo.h:
 *
 *   init_simulation_config()   - fill a config with sensible defaults
 *   run_simulation()           - geometric Brownian motion over trading days
 *   print_results() / output_results_json() / output_results_csv()
 *
 * The simulator evolves the underlying under the risk-neutral measure, so the
 * mean terminal price should converge to  S0 * exp((r - q) * T)  with
 * T = trading_days / 252 (one simulated year of `trading_days` steps).
 *
 * Build (see the finance README / Makefile target `monte_carlo`):
 *   links against monte_carlo.o and the quantum RNG lib.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

#include "monte_carlo.h"

static void usage(const char *prog) {
    printf(
"Usage: %s [options]\n"
"\n"
"Simulates terminal asset prices under geometric Brownian motion using the\n"
"quantum RNG, then reports the mean, dispersion and a confidence interval.\n"
"\n"
"  -n <sims>     Number of Monte Carlo simulations      (default %d)\n"
"  -d <days>     Trading days per simulated year         (default %d)\n"
"  -p <price>    Initial (spot) price                    (default %.2f)\n"
"  -v <vol>      Volatility, e.g. 0.2 = 20%%             (default %.2f)\n"
"  -r <rate>     Risk-free rate, e.g. 0.05 = 5%%         (default %.2f)\n"
"  -y <yield>    Continuous dividend yield               (default %.2f)\n"
"  -o <mode>     Output: normal, json or csv             (default normal)\n"
"  -s <seed>     Seed string for the RNG (omit for hardware entropy)\n"
"  -h            Show this help and exit\n",
        prog,
        DEFAULT_NUM_SIMULATIONS, DEFAULT_TRADING_DAYS, DEFAULT_INITIAL_PRICE,
        DEFAULT_VOLATILITY, DEFAULT_RISK_FREE_RATE, DEFAULT_DIVIDEND_YIELD);
}

/* Map an output-mode name to the enum; returns -1 on an unknown name. */
static int parse_output_mode(const char *s, output_mode_t *out) {
    if      (strcmp(s, "normal") == 0) *out = OUTPUT_NORMAL;
    else if (strcmp(s, "json") == 0)   *out = OUTPUT_JSON;
    else if (strcmp(s, "csv") == 0)    *out = OUTPUT_CSV;
    else return -1;
    return 0;
}

int main(int argc, char *argv[]) {
    simulation_config_t config;
    init_simulation_config(&config);

    int opt;
    while ((opt = getopt(argc, argv, "n:d:p:v:r:y:o:s:h")) != -1) {
        switch (opt) {
            case 'n': config.num_simulations = atoi(optarg); break;
            case 'd': config.trading_days = atoi(optarg); break;
            case 'p': config.asset.initial_price = atof(optarg); break;
            case 'v': config.asset.volatility = atof(optarg); break;
            case 'r': config.asset.risk_free_rate = atof(optarg); break;
            case 'y': config.asset.dividend_yield = atof(optarg); break;
            case 'o':
                if (parse_output_mode(optarg, &config.output_mode) != 0) {
                    fprintf(stderr, "Unknown output mode '%s' (use normal, json or csv)\n",
                            optarg);
                    return 1;
                }
                break;
            case 's':
                strncpy(config.seed, optarg, sizeof(config.seed) - 1);
                config.seed[sizeof(config.seed) - 1] = '\0';
                config.seed_length = (int)strlen(config.seed);
                break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    /* Validate before touching the engine so errors are actionable. */
    if (config.num_simulations < MIN_SIMULATIONS ||
        config.num_simulations > MAX_SIMULATIONS) {
        fprintf(stderr, "Error: simulations (-n) must be between %d and %d.\n",
                MIN_SIMULATIONS, MAX_SIMULATIONS);
        return 1;
    }
    if (config.trading_days < 1) {
        fprintf(stderr, "Error: trading days (-d) must be at least 1.\n");
        return 1;
    }
    if (config.asset.initial_price <= 0.0) {
        fprintf(stderr, "Error: initial price (-p) must be positive.\n");
        return 1;
    }
    if (config.asset.volatility < 0.0) {
        fprintf(stderr, "Error: volatility (-v) must be non-negative.\n");
        return 1;
    }

    /* The progress bar goes to stderr, but keep it off for machine output. */
    if (config.output_mode == OUTPUT_JSON || config.output_mode == OUTPUT_CSV) {
        config.show_progress = 0;
    }

    simulation_results_t results = run_simulation(&config);
    if (!results.prices) {
        fprintf(stderr, "Error: simulation failed (check parameters).\n");
        return 1;
    }

    switch (config.output_mode) {
        case OUTPUT_JSON: output_results_json(stdout, &results, &config); break;
        case OUTPUT_CSV:  output_results_csv(stdout, &results, &config);  break;
        default:          print_results(stdout, &results, &config);       break;
    }

    /* In the human-readable report, show the risk-neutral expectation so the
       user can see the Monte Carlo mean converging toward it. */
    if (config.output_mode == OUTPUT_NORMAL) {
        double T = (double)config.trading_days / (double)DEFAULT_TRADING_DAYS;
        double expected = config.asset.initial_price *
                          exp((config.asset.risk_free_rate -
                               config.asset.dividend_yield) * T);
        printf("Theoretical Mean (S0*e^((r-q)T)): %.2f\n", expected);
    }

    free_simulation_results(&results);
    return 0;
}

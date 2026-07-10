#include "quantum_portfolio.h"
#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>

// Sample asset data for demonstration
const char *DEFAULT_ASSET_NAMES[] = {
    "AAPL", "GOOGL", "MSFT", "AMZN", "FB",
    "TSLA", "NVDA", "JPM", "V", "WMT"
};
#define NUM_DEFAULT_ASSETS 10

const double DEFAULT_RETURNS[] = {
    0.20, 0.18, 0.15, 0.25, 0.16,
    0.30, 0.22, 0.12, 0.14, 0.10
};

const double DEFAULT_VOLATILITIES[] = {
    0.25, 0.22, 0.20, 0.28, 0.24,
    0.35, 0.30, 0.18, 0.16, 0.15
};

/*
 * Draw a standard normal variate N(0,1) using the Box-Muller transform.
 * Two uniforms produce two independent normals; the second is cached so
 * consecutive calls consume one uniform each on average.
 */
static double generate_normal(qrng_ctx *ctx, double *cache, int *has_cache) {
    if (*has_cache) {
        *has_cache = 0;
        return *cache;
    }

    double u1;
    do {
        u1 = qrng_double(ctx);   /* qrng_double is in [0,1); reject 0 for log() */
    } while (u1 <= 0.0);
    double u2 = qrng_double(ctx);

    double r = sqrt(-2.0 * log(u1));
    *cache = r * sin(2.0 * M_PI * u2);
    *has_cache = 1;
    return r * cos(2.0 * M_PI * u2);
}

void init_portfolio_config(portfolio_config_t *config) {
    memset(config, 0, sizeof(*config));
    config->num_assets = DEFAULT_NUM_ASSETS;
    config->num_simulations = DEFAULT_NUM_SIMULATIONS;
    config->time_horizon = DEFAULT_TIME_HORIZON;
    config->risk_free_rate = DEFAULT_RISK_FREE_RATE;
    config->target_return = DEFAULT_TARGET_RETURN;
    config->risk_tolerance = DEFAULT_RISK_TOLERANCE;

    // Initialize every slot up to MAX_ASSETS so that a later -n larger
    // than the default never touches uninitialized entries (the extra
    // slots cycle through the sample data).
    config->assets = malloc(MAX_ASSETS * sizeof(asset_t));
    if (!config->assets) {
        fprintf(stderr, "Error: out of memory allocating assets\n");
        exit(1);
    }
    for (int i = 0; i < MAX_ASSETS; i++) {
        int d = i % NUM_DEFAULT_ASSETS;
        memset(config->assets[i].name, 0, sizeof(config->assets[i].name));
        if (i < NUM_DEFAULT_ASSETS) {
            strncpy(config->assets[i].name, DEFAULT_ASSET_NAMES[d],
                    sizeof(config->assets[i].name) - 1);
        } else {
            snprintf(config->assets[i].name, sizeof(config->assets[i].name),
                     "%s_%d", DEFAULT_ASSET_NAMES[d], i / NUM_DEFAULT_ASSETS + 1);
        }
        config->assets[i].expected_return = DEFAULT_RETURNS[d];
        config->assets[i].volatility = DEFAULT_VOLATILITIES[d];
        config->assets[i].weight = 1.0 / config->num_assets;  // Equal weight initially
        config->assets[i].correlations = calloc(MAX_ASSETS, sizeof(double));
        if (!config->assets[i].correlations) {
            fprintf(stderr, "Error: out of memory allocating correlations\n");
            exit(1);
        }

        // Initialize correlations (simplified single-factor structure)
        for (int j = 0; j < MAX_ASSETS; j++) {
            config->assets[i].correlations[j] = (i == j) ? 1.0 : 0.3;
        }
    }

    strncpy(config->seed, "portfolio", sizeof(config->seed) - 1);
    config->seed_length = (int)strlen(config->seed);
    config->output_mode = OUTPUT_NORMAL;
    config->show_progress = 1;
    config->output_file[0] = '\0';
    config->show_efficient_frontier = 0;
    config->show_rebalancing = 0;
}

void free_portfolio_config(portfolio_config_t *config) {
    if (config->assets) {
        for (int i = 0; i < MAX_ASSETS; i++) {
            free(config->assets[i].correlations);
        }
        free(config->assets);
        config->assets = NULL;
    }
}

void print_portfolio_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -n, --assets N        Number of assets (1-%d, default: %d)\n",
           MAX_ASSETS, DEFAULT_NUM_ASSETS);
    printf("  -s, --simulations N   Number of simulations (%d-%d, default: %d)\n",
           MIN_SIMULATIONS, MAX_SIMULATIONS, DEFAULT_NUM_SIMULATIONS);
    printf("  -t, --horizon N       Time horizon in days (default: %d)\n",
           DEFAULT_TIME_HORIZON);
    printf("  -r, --rate N          Risk-free rate (default: %.2f)\n",
           DEFAULT_RISK_FREE_RATE);
    printf("  -R, --target N        Target return (default: %.2f)\n",
           DEFAULT_TARGET_RETURN);
    printf("  -T, --tolerance N     Risk tolerance (default: %.2f)\n",
           DEFAULT_RISK_TOLERANCE);
    printf("  -S, --seed STRING     Random seed\n");
    printf("  -q, --quiet           Quiet mode (only output weights)\n");
    printf("  -v, --verbose         Verbose mode (show additional statistics)\n");
    printf("  -j, --json            Output results in JSON format\n");
    printf("  -c, --csv             Output results in CSV format\n");
    printf("  -P, --no-progress     Hide progress bar\n");
    printf("  -E, --frontier        Show efficient frontier\n");
    printf("  -B, --rebalance       Show rebalancing suggestions\n");
    printf("  -o, --output FILE     Write output to file\n");
    printf("  -h, --help            Show this help message\n");
}

void parse_portfolio_args(int argc, char *argv[], portfolio_config_t *config) {
    static struct option long_options[] = {
        {"assets",      required_argument, 0, 'n'},
        {"simulations", required_argument, 0, 's'},
        {"horizon",     required_argument, 0, 't'},
        {"rate",        required_argument, 0, 'r'},
        {"target",      required_argument, 0, 'R'},
        {"tolerance",   required_argument, 0, 'T'},
        {"seed",        required_argument, 0, 'S'},
        {"quiet",       no_argument,       0, 'q'},
        {"verbose",     no_argument,       0, 'v'},
        {"json",        no_argument,       0, 'j'},
        {"csv",         no_argument,       0, 'c'},
        {"no-progress", no_argument,       0, 'P'},
        {"frontier",    no_argument,       0, 'E'},
        {"rebalance",   no_argument,       0, 'B'},
        {"output",      required_argument, 0, 'o'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "n:s:t:r:R:T:S:qvjcPEBo:h",
           long_options, &option_index)) != -1) {
        switch (c) {
            case 'n':
                config->num_assets = atoi(optarg);
                if (config->num_assets < 1 || config->num_assets > MAX_ASSETS) {
                    fprintf(stderr, "Error: Number of assets must be between 1 and %d\n",
                            MAX_ASSETS);
                    exit(1);
                }
                for (int i = 0; i < MAX_ASSETS; i++) {
                    config->assets[i].weight = 1.0 / config->num_assets;
                }
                break;

            case 's':
                config->num_simulations = atoi(optarg);
                if (config->num_simulations < MIN_SIMULATIONS ||
                    config->num_simulations > MAX_SIMULATIONS) {
                    fprintf(stderr, "Error: Number of simulations must be between %d and %d\n",
                            MIN_SIMULATIONS, MAX_SIMULATIONS);
                    exit(1);
                }
                break;

            case 't':
                config->time_horizon = atoi(optarg);
                if (config->time_horizon <= 0) {
                    fprintf(stderr, "Error: Time horizon must be positive\n");
                    exit(1);
                }
                break;

            case 'r':
                config->risk_free_rate = atof(optarg);
                break;

            case 'R':
                config->target_return = atof(optarg);
                break;

            case 'T':
                config->risk_tolerance = atof(optarg);
                if (config->risk_tolerance < 0 || config->risk_tolerance > 1) {
                    fprintf(stderr, "Error: Risk tolerance must be between 0 and 1\n");
                    exit(1);
                }
                break;

            case 'S':
                memset(config->seed, 0, sizeof(config->seed));
                strncpy(config->seed, optarg, sizeof(config->seed) - 1);
                config->seed_length = (int)strlen(config->seed);
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

            case 'c':
                config->output_mode = OUTPUT_CSV;
                break;

            case 'P':
                config->show_progress = 0;
                break;

            case 'E':
                config->show_efficient_frontier = 1;
                break;

            case 'B':
                config->show_rebalancing = 1;
                break;

            case 'o':
                strncpy(config->output_file, optarg, sizeof(config->output_file) - 1);
                config->output_file[sizeof(config->output_file) - 1] = '\0';
                break;

            case 'h':
                print_portfolio_usage(argv[0]);
                exit(0);

            case '?':
                exit(1);

            default:
                fprintf(stderr, "Error: Unknown option %c\n", c);
                exit(1);
        }
    }
}

// Calculate portfolio return and risk
void calculate_portfolio_metrics(portfolio_metrics_t *metrics,
                               const double *weights,
                               const portfolio_config_t *config) {
    // Calculate expected return
    metrics->expected_return = 0;
    for (int i = 0; i < config->num_assets; i++) {
        metrics->expected_return += weights[i] * config->assets[i].expected_return;
    }

    // Calculate portfolio variance
    double variance = 0;
    for (int i = 0; i < config->num_assets; i++) {
        for (int j = 0; j < config->num_assets; j++) {
            variance += weights[i] * weights[j] *
                       config->assets[i].volatility * config->assets[j].volatility *
                       config->assets[i].correlations[j];
        }
    }
    metrics->volatility = sqrt(fmax(variance, 0.0));

    // Calculate Sharpe ratio
    metrics->sharpe_ratio = metrics->volatility > 0.0
        ? (metrics->expected_return - config->risk_free_rate) / metrics->volatility
        : 0.0;
}

// Generate random portfolio weights using quantum randomness
static void generate_random_weights(qrng_ctx *ctx, double *weights, int num_assets) {
    double sum = 0;
    for (int i = 0; i < num_assets; i++) {
        weights[i] = qrng_double(ctx);
        sum += weights[i];
    }
    if (sum <= 0.0) {           // All-zero draw is astronomically unlikely; be safe
        for (int i = 0; i < num_assets; i++) weights[i] = 1.0 / num_assets;
        return;
    }
    // Normalize weights
    for (int i = 0; i < num_assets; i++) {
        weights[i] /= sum;
    }
}

/*
 * GA fitness.  In Sharpe mode the optimizer maximizes the Sharpe ratio.
 * In target mode (used for the efficient frontier) it minimizes portfolio
 * volatility subject to a soft constraint that the expected return hits
 * the requested target.
 */
#define TARGET_RETURN_PENALTY 25.0

static double ga_fitness(const portfolio_metrics_t *m, double target_return, int target_mode) {
    if (target_mode) {
        return -(m->volatility + TARGET_RETURN_PENALTY * fabs(m->expected_return - target_return));
    }
    return m->sharpe_ratio;
}

// Pick the fitter of two quantum-randomly chosen population members
static int tournament_select(qrng_ctx *ctx, const double *fitness) {
    int a = (int)(qrng_uint64(ctx) % POPULATION_SIZE);
    int b = (int)(qrng_uint64(ctx) % POPULATION_SIZE);
    return fitness[a] >= fitness[b] ? a : b;
}

/*
 * Quantum-enhanced genetic algorithm over the weight simplex.
 * Writes the best weights found into best_weights and returns their fitness.
 */
static double ga_optimize(const portfolio_config_t *config, qrng_ctx *ctx,
                          int iterations, int target_mode, double target_return,
                          double *best_weights, int show_progress) {
    int n = config->num_assets;

    double **population = malloc(POPULATION_SIZE * sizeof(double*));
    double **next_population = malloc(POPULATION_SIZE * sizeof(double*));
    double *fitness = malloc(POPULATION_SIZE * sizeof(double));
    if (!population || !next_population || !fitness) {
        fprintf(stderr, "Error: out of memory in optimizer\n");
        exit(1);
    }
    for (int i = 0; i < POPULATION_SIZE; i++) {
        population[i] = malloc(n * sizeof(double));
        next_population[i] = malloc(n * sizeof(double));
        if (!population[i] || !next_population[i]) {
            fprintf(stderr, "Error: out of memory in optimizer\n");
            exit(1);
        }
        generate_random_weights(ctx, population[i], n);
    }

    double best_fitness = -INFINITY;
    int progress_step = iterations / 100;
    if (progress_step < 1) progress_step = 1;

    for (int gen = 0; gen < iterations; gen++) {
        // Evaluate fitness
        for (int i = 0; i < POPULATION_SIZE; i++) {
            portfolio_metrics_t metrics = {0};
            calculate_portfolio_metrics(&metrics, population[i], config);
            fitness[i] = ga_fitness(&metrics, target_return, target_mode);

            if (fitness[i] > best_fitness) {
                best_fitness = fitness[i];
                memcpy(best_weights, population[i], n * sizeof(double));
            }
        }

        if (show_progress && gen % progress_step == 0) {
            printf("\rOptimization progress: %d%%", gen * 100 / iterations);
            fflush(stdout);
        }

        // Elitism: carry the best-so-far portfolio into the next generation
        memcpy(next_population[0], best_weights, n * sizeof(double));

        // Tournament selection + blend crossover + mutation
        for (int i = 1; i < POPULATION_SIZE; i++) {
            const double *parent1 = population[tournament_select(ctx, fitness)];
            const double *parent2 = population[tournament_select(ctx, fitness)];

            double alpha = qrng_double(ctx);
            double *child = next_population[i];
            for (int j = 0; j < n; j++) {
                child[j] = alpha * parent1[j] + (1.0 - alpha) * parent2[j];
            }

            // Mutation
            if (qrng_double(ctx) < MUTATION_RATE) {
                int asset = (int)(qrng_uint64(ctx) % n);
                child[asset] = qrng_double(ctx);
            }

            // Renormalize onto the weight simplex
            double sum = 0.0;
            for (int j = 0; j < n; j++) sum += child[j];
            if (sum <= 0.0) {
                generate_random_weights(ctx, child, n);
            } else {
                for (int j = 0; j < n; j++) child[j] /= sum;
            }
        }

        // Swap generations
        double **tmp = population;
        population = next_population;
        next_population = tmp;
    }

    if (show_progress) {
        printf("\rOptimization progress: 100%%\n");
    }

    for (int i = 0; i < POPULATION_SIZE; i++) {
        free(population[i]);
        free(next_population[i]);
    }
    free(population);
    free(next_population);
    free(fitness);

    return best_fitness;
}

/*
 * Cholesky factorization (in place, row-major, lower triangle).
 * Returns 1 on success, 0 if the matrix is not positive definite.
 */
static int cholesky_decompose(double *a, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double sum = a[i*n + j];
            for (int k = 0; k < j; k++) {
                sum -= a[i*n + k] * a[j*n + k];
            }
            if (i == j) {
                if (sum <= 0.0) return 0;
                a[i*n + i] = sqrt(sum);
            } else {
                a[i*n + j] = sum / a[j*n + j];
            }
        }
        for (int j = i + 1; j < n; j++) a[i*n + j] = 0.0;  // zero the upper triangle
    }
    return 1;
}

// Optimize portfolio using quantum-enhanced genetic algorithm
portfolio_results_t optimize_portfolio(const portfolio_config_t *config) {
    portfolio_results_t results = {0};

    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx,
                               config->seed_length > 0 ? (const uint8_t*)config->seed : NULL,
                               config->seed_length);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize quantum RNG: %s\n", qrng_error_string(err));
        return results;
    }

    int n = config->num_assets;
    results.weights = calloc(n, sizeof(double));
    results.simulated_returns = calloc(config->num_simulations, sizeof(double));
    results.num_simulations = config->num_simulations;
    if (!results.weights || !results.simulated_returns) {
        fprintf(stderr, "Error: out of memory allocating results\n");
        exit(1);
    }

    // Maximize the Sharpe ratio with the genetic algorithm
    ga_optimize(config, ctx, MAX_ITERATIONS, 0, config->target_return,
                results.weights, config->show_progress);

    // Calculate final metrics
    calculate_portfolio_metrics(&results.metrics, results.weights, config);

    // Run Monte Carlo simulation of the optimized portfolio
    if (config->show_progress) {
        printf("Running Monte Carlo simulation...\n");
    }

    // Build the Cholesky factor of the asset correlation matrix so the
    // simulated shocks respect the correlations used by the optimizer.
    double *chol = malloc((size_t)n * n * sizeof(double));
    double *z = malloc(n * sizeof(double));
    double *eps = malloc(n * sizeof(double));
    if (!chol || !z || !eps) {
        fprintf(stderr, "Error: out of memory in simulation\n");
        exit(1);
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            chol[i*n + j] = config->assets[i].correlations[j];
        }
    }
    int correlated = cholesky_decompose(chol, n);
    if (!correlated) {
        fprintf(stderr, "Warning: correlation matrix is not positive definite; "
                        "simulating independent shocks\n");
    }

    double dt = 1.0 / config->time_horizon;
    double normal_cache = 0.0;
    int has_normal_cache = 0;
    double drawdown_sum = 0.0;

    for (int sim = 0; sim < config->num_simulations; sim++) {
        double portfolio_value = 1.0;
        double peak = 1.0;
        double max_dd = 0.0;

        for (int t = 0; t < config->time_horizon; t++) {
            for (int i = 0; i < n; i++) {
                z[i] = generate_normal(ctx, &normal_cache, &has_normal_cache);
            }
            if (correlated) {
                for (int i = 0; i < n; i++) {
                    double acc = 0.0;
                    for (int k = 0; k <= i; k++) acc += chol[i*n + k] * z[k];
                    eps[i] = acc;
                }
            } else {
                memcpy(eps, z, n * sizeof(double));
            }

            double return_t = 0;
            for (int i = 0; i < n; i++) {
                return_t += results.weights[i] * (
                    config->assets[i].expected_return * dt +
                    config->assets[i].volatility * sqrt(dt) * eps[i]
                );
            }
            portfolio_value *= (1.0 + return_t);

            if (portfolio_value > peak) peak = portfolio_value;
            double dd = (peak - portfolio_value) / peak;
            if (dd > max_dd) max_dd = dd;
        }

        results.simulated_returns[sim] = portfolio_value - 1.0;
        drawdown_sum += max_dd;
    }

    free(chol);
    free(z);
    free(eps);

    // Expected maximum drawdown over the horizon (mean across paths)
    results.metrics.max_drawdown = drawdown_sum / config->num_simulations;

    // Calculate VaR and CVaR from the sorted return distribution
    qsort(results.simulated_returns, config->num_simulations,
          sizeof(double), compare_doubles);
    int var_index = (int)(0.05 * config->num_simulations);
    if (var_index < 1) var_index = 1;
    results.metrics.var_95 = -results.simulated_returns[var_index];

    double cvar_sum = 0;
    for (int i = 0; i < var_index; i++) {
        cvar_sum += results.simulated_returns[i];
    }
    results.metrics.cvar_95 = -cvar_sum / var_index;

    qrng_free(ctx);

    // Generate efficient frontier if requested
    if (config->show_efficient_frontier) {
        generate_efficient_frontier(&results, config);
    }

    return results;
}

void generate_efficient_frontier(portfolio_results_t *results,
                               const portfolio_config_t *config) {
    const int num_points = 100;
    results->metrics.frontier_points = num_points;
    results->metrics.efficient_frontier_returns = malloc(num_points * sizeof(double));
    results->metrics.efficient_frontier_risks = malloc(num_points * sizeof(double));
    double *weights = malloc(config->num_assets * sizeof(double));
    if (!results->metrics.efficient_frontier_returns ||
        !results->metrics.efficient_frontier_risks || !weights) {
        fprintf(stderr, "Error: out of memory generating frontier\n");
        exit(1);
    }

    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx,
                               config->seed_length > 0 ? (const uint8_t*)config->seed : NULL,
                               config->seed_length);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize quantum RNG: %s\n", qrng_error_string(err));
        free(weights);
        results->metrics.frontier_points = 0;
        return;
    }

    double min_return = INFINITY;
    double max_return = -INFINITY;
    for (int i = 0; i < config->num_assets; i++) {
        if (config->assets[i].expected_return < min_return) {
            min_return = config->assets[i].expected_return;
        }
        if (config->assets[i].expected_return > max_return) {
            max_return = config->assets[i].expected_return;
        }
    }

    if (config->show_progress) {
        printf("Generating efficient frontier (%d points)...\n", num_points);
    }

    for (int i = 0; i < num_points; i++) {
        double target_return = min_return +
                             (max_return - min_return) * i / (num_points - 1);

        // Minimum-variance portfolio for this target return (GA in target
        // mode, no Monte Carlo needed for the frontier itself)
        ga_optimize(config, ctx, FRONTIER_ITERATIONS, 1, target_return, weights, 0);

        portfolio_metrics_t metrics = {0};
        calculate_portfolio_metrics(&metrics, weights, config);
        results->metrics.efficient_frontier_returns[i] = metrics.expected_return;
        results->metrics.efficient_frontier_risks[i] = metrics.volatility;

        if (config->show_progress && (i + 1) % 10 == 0) {
            printf("\rFrontier progress: %d%%", (i + 1) * 100 / num_points);
            fflush(stdout);
        }
    }
    if (config->show_progress) {
        printf("\rFrontier progress: 100%%\n");
    }

    free(weights);
    qrng_free(ctx);
}

void output_results_json(FILE *output, const portfolio_results_t *results,
                        const portfolio_config_t *config) {
    fprintf(output, "{\n");
    fprintf(output, "  \"portfolio\": {\n");
    fprintf(output, "    \"expected_return\": %.4f,\n", results->metrics.expected_return);
    fprintf(output, "    \"volatility\": %.4f,\n", results->metrics.volatility);
    fprintf(output, "    \"sharpe_ratio\": %.4f,\n", results->metrics.sharpe_ratio);
    fprintf(output, "    \"var_95\": %.4f,\n", results->metrics.var_95);
    fprintf(output, "    \"cvar_95\": %.4f,\n", results->metrics.cvar_95);
    fprintf(output, "    \"expected_max_drawdown\": %.4f\n", results->metrics.max_drawdown);
    fprintf(output, "  },\n");
    fprintf(output, "  \"weights\": {\n");

    for (int i = 0; i < config->num_assets; i++) {
        fprintf(output, "    \"%s\": %.4f%s\n",
               config->assets[i].name, results->weights[i],
               i < config->num_assets - 1 ? "," : "");
    }

    fprintf(output, "  }");

    if (config->show_efficient_frontier && results->metrics.frontier_points > 0) {
        fprintf(output, ",\n  \"efficient_frontier\": [\n");
        for (int i = 0; i < results->metrics.frontier_points; i++) {
            fprintf(output, "    {\"return\": %.4f, \"risk\": %.4f}%s\n",
                   results->metrics.efficient_frontier_returns[i],
                   results->metrics.efficient_frontier_risks[i],
                   i < results->metrics.frontier_points - 1 ? "," : "");
        }
        fprintf(output, "  ]");
    }

    fprintf(output, "\n}\n");
}

void output_results_csv(FILE *output, const portfolio_results_t *results,
                       const portfolio_config_t *config) {
    fprintf(output, "asset,weight\n");
    for (int i = 0; i < config->num_assets; i++) {
        fprintf(output, "%s,%.4f\n", config->assets[i].name, results->weights[i]);
    }

    if (config->show_efficient_frontier && results->metrics.frontier_points > 0) {
        fprintf(output, "\nreturn,risk\n");
        for (int i = 0; i < results->metrics.frontier_points; i++) {
            fprintf(output, "%.4f,%.4f\n",
                   results->metrics.efficient_frontier_returns[i],
                   results->metrics.efficient_frontier_risks[i]);
        }
    }
}

void calculate_rebalancing(FILE *output, const portfolio_results_t *results,
                           const portfolio_config_t *config) {
    fprintf(output, "\nRebalancing Suggestions (current -> optimal):\n");
    fprintf(output, "---------------------------------------------\n");
    for (int i = 0; i < config->num_assets; i++) {
        double delta = results->weights[i] - config->assets[i].weight;
        fprintf(output, "%-8s %6.2f%% -> %6.2f%%  (%s%.2f%%)\n",
                config->assets[i].name,
                config->assets[i].weight * 100,
                results->weights[i] * 100,
                delta >= 0 ? "+" : "",
                delta * 100);
    }
}

void print_results(FILE *output, const portfolio_results_t *results,
                  const portfolio_config_t *config) {
    switch (config->output_mode) {
        case OUTPUT_QUIET:
            for (int i = 0; i < config->num_assets; i++) {
                fprintf(output, "%.4f%s", results->weights[i],
                       i < config->num_assets - 1 ? "," : "\n");
            }
            break;

        case OUTPUT_JSON:
            output_results_json(output, results, config);
            break;

        case OUTPUT_CSV:
            output_results_csv(output, results, config);
            break;

        case OUTPUT_VERBOSE:
        case OUTPUT_NORMAL:
            fprintf(output, "\nOptimal Portfolio Allocation:\n");
            fprintf(output, "===========================\n\n");

            for (int i = 0; i < config->num_assets; i++) {
                fprintf(output, "%s: %.2f%%\n", config->assets[i].name,
                       results->weights[i] * 100);
            }

            fprintf(output, "\nPortfolio Metrics:\n");
            fprintf(output, "Expected Return: %.2f%%\n",
                   results->metrics.expected_return * 100);
            fprintf(output, "Volatility: %.2f%%\n",
                   results->metrics.volatility * 100);
            fprintf(output, "Sharpe Ratio: %.2f\n",
                   results->metrics.sharpe_ratio);
            fprintf(output, "95%% VaR: %.2f%%\n",
                   results->metrics.var_95 * 100);
            fprintf(output, "95%% CVaR: %.2f%%\n",
                   results->metrics.cvar_95 * 100);
            fprintf(output, "Expected Max Drawdown: %.2f%%\n",
                   results->metrics.max_drawdown * 100);

            if (config->show_rebalancing) {
                calculate_rebalancing(output, results, config);
            }

            if (config->show_efficient_frontier && results->metrics.frontier_points > 0) {
                fprintf(output, "\nEfficient Frontier (return, risk):\n");
                for (int i = 0; i < results->metrics.frontier_points; i += 10) {
                    fprintf(output, "  %.2f%%  %.2f%%\n",
                           results->metrics.efficient_frontier_returns[i] * 100,
                           results->metrics.efficient_frontier_risks[i] * 100);
                }
            }

            if (config->output_mode == OUTPUT_VERBOSE) {
                fprintf(output, "\nSimulation Parameters:\n");
                fprintf(output, "Number of Assets: %d\n", config->num_assets);
                fprintf(output, "Number of Simulations: %d\n", config->num_simulations);
                fprintf(output, "Time Horizon: %d days\n", config->time_horizon);
                fprintf(output, "Risk-free Rate: %.2f%%\n",
                       config->risk_free_rate * 100);
                fprintf(output, "Target Return: %.2f%%\n",
                       config->target_return * 100);
                fprintf(output, "Risk Tolerance: %.2f\n",
                       config->risk_tolerance);
            }
            break;
    }
}

void free_portfolio_results(portfolio_results_t *results) {
    free(results->weights);
    free(results->simulated_returns);
    if (results->metrics.efficient_frontier_returns) {
        free(results->metrics.efficient_frontier_returns);
    }
    if (results->metrics.efficient_frontier_risks) {
        free(results->metrics.efficient_frontier_risks);
    }
}

int compare_doubles(const void *a, const void *b) {
    double diff = *(double*)a - *(double*)b;
    return (diff > 0) - (diff < 0);
}

int main(int argc, char *argv[]) {
    portfolio_config_t config;
    init_portfolio_config(&config);
    parse_portfolio_args(argc, argv, &config);

    FILE *output = config.output_file[0] ? fopen(config.output_file, "w") : stdout;
    if (!output) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", config.output_file);
        free_portfolio_config(&config);
        return 1;
    }

    portfolio_results_t results = optimize_portfolio(&config);
    print_results(output, &results, &config);

    free_portfolio_results(&results);
    free_portfolio_config(&config);

    if (output != stdout) {
        fclose(output);
    }

    return 0;
}

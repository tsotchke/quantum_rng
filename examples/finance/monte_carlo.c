#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "monte_carlo.h"
#include "../../src/quantum_rng/quantum_rng.h"

void init_simulation_config(simulation_config_t *config) {
    if (!config) return;
    
    config->num_simulations = DEFAULT_NUM_SIMULATIONS;
    config->trading_days = DEFAULT_TRADING_DAYS;
    config->asset.initial_price = DEFAULT_INITIAL_PRICE;
    config->asset.volatility = DEFAULT_VOLATILITY;
    config->asset.risk_free_rate = DEFAULT_RISK_FREE_RATE;
    config->asset.dividend_yield = DEFAULT_DIVIDEND_YIELD;
    config->output_mode = OUTPUT_NORMAL;
    config->show_progress = 1;
    config->seed_length = 0;
    config->confidence_level = CONFIDENCE_95;
    memset(config->output_file, 0, sizeof(config->output_file));
}

void parse_simulation_args(int argc, char *argv[], simulation_config_t *config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            config->num_simulations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            config->trading_days = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config->asset.initial_price = atof(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            config->asset.volatility = atof(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            config->asset.risk_free_rate = atof(argv[++i]);
        } else if (strcmp(argv[i], "-y") == 0 && i + 1 < argc) {
            config->asset.dividend_yield = atof(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            if (strcmp(argv[++i], "json") == 0) {
                config->output_mode = OUTPUT_JSON;
            } else if (strcmp(argv[i], "csv") == 0) {
                config->output_mode = OUTPUT_CSV;
            }
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            strncpy(config->seed, argv[++i], sizeof(config->seed) - 1);
            config->seed_length = strlen(config->seed);
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            strncpy(config->output_file, argv[++i], sizeof(config->output_file) - 1);
        }
    }
}

simulation_results_t run_simulation(const simulation_config_t *config) {
    simulation_results_t results = {0};
    
    // Validate parameters
    if (!config || config->num_simulations < MIN_SIMULATIONS || 
        config->num_simulations > MAX_SIMULATIONS) {
        results.prices = NULL;
        return results;
    }
    
    // Initialize QRNG
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx, 
                              (uint8_t*)config->seed, 
                              config->seed_length);
    if (err != QRNG_SUCCESS) {
        results.prices = NULL;
        return results;
    }
    
    // Allocate memory for price paths
    results.prices = malloc(config->num_simulations * sizeof(double));
    if (!results.prices) {
        qrng_free(ctx);
        return results;
    }
    
    // Run simulations
    double dt = 1.0 / config->trading_days;
    double drift = (config->asset.risk_free_rate - 
                   config->asset.dividend_yield - 
                   0.5 * config->asset.volatility * config->asset.volatility) * dt;
    double vol = config->asset.volatility * sqrt(dt);
    
    double sum = 0.0;
    double sum_squared = 0.0;
    results.min_price = INFINITY;
    results.max_price = -INFINITY;
    
    for (int i = 0; i < config->num_simulations; i++) {
        double price = config->asset.initial_price;
        
        for (int t = 0; t < config->trading_days; t++) {
            double z = qrng_double(ctx);
            price *= exp(drift + vol * z);
        }
        
        results.prices[i] = price;
        sum += price;
        sum_squared += price * price;
        
        if (price < results.min_price) results.min_price = price;
        if (price > results.max_price) results.max_price = price;
        
        if (config->show_progress && i % (config->num_simulations / 100) == 0) {
            fprintf(stderr, "\rProgress: %d%%", i * 100 / config->num_simulations);
        }
    }
    
    if (config->show_progress) {
        fprintf(stderr, "\rProgress: 100%%\n");
    }
    
    // Calculate statistics
    results.mean_price = sum / config->num_simulations;
    results.std_dev = sqrt((sum_squared / config->num_simulations) - 
                          (results.mean_price * results.mean_price));
    
    double z_score = config->confidence_level;
    double margin = z_score * results.std_dev / sqrt(config->num_simulations);
    results.confidence_lower = results.mean_price - margin;
    results.confidence_upper = results.mean_price + margin;
    
    qrng_free(ctx);
    return results;
}

void print_results(FILE *output, const simulation_results_t *results, 
                  const simulation_config_t *config) {
    fprintf(output, "\nSimulation Results:\n");
    fprintf(output, "Mean Price: %.2f\n", results->mean_price);
    fprintf(output, "Standard Deviation: %.2f\n", results->std_dev);
    fprintf(output, "Min Price: %.2f\n", results->min_price);
    fprintf(output, "Max Price: %.2f\n", results->max_price);
    fprintf(output, "%.0f%% Confidence Interval: [%.2f, %.2f]\n",
            config->confidence_level * 100,
            results->confidence_lower,
            results->confidence_upper);
}

void output_results_json(FILE *output, const simulation_results_t *results,
                        const simulation_config_t *config) {
    fprintf(output, "{\n");
    fprintf(output, "  \"mean_price\": %.2f,\n", results->mean_price);
    fprintf(output, "  \"standard_deviation\": %.2f,\n", results->std_dev);
    fprintf(output, "  \"min_price\": %.2f,\n", results->min_price);
    fprintf(output, "  \"max_price\": %.2f,\n", results->max_price);
    fprintf(output, "  \"confidence_interval\": {\n");
    fprintf(output, "    \"level\": %.2f,\n", config->confidence_level);
    fprintf(output, "    \"lower\": %.2f,\n", results->confidence_lower);
    fprintf(output, "    \"upper\": %.2f\n", results->confidence_upper);
    fprintf(output, "  },\n");
    fprintf(output, "  \"prices\": [");
    
    for (int i = 0; i < config->num_simulations; i++) {
        fprintf(output, "%.2f%s", 
                results->prices[i], 
                i < config->num_simulations - 1 ? "," : "");
    }
    
    fprintf(output, "]\n}\n");
}

void output_results_csv(FILE *output, const simulation_results_t *results,
                       const simulation_config_t *config) {
    fprintf(output, "Statistic,Value\n");
    fprintf(output, "Mean Price,%.2f\n", results->mean_price);
    fprintf(output, "Standard Deviation,%.2f\n", results->std_dev);
    fprintf(output, "Min Price,%.2f\n", results->min_price);
    fprintf(output, "Max Price,%.2f\n", results->max_price);
    fprintf(output, "Confidence Level,%.2f\n", config->confidence_level);
    fprintf(output, "Confidence Lower,%.2f\n", results->confidence_lower);
    fprintf(output, "Confidence Upper,%.2f\n", results->confidence_upper);
    
    fprintf(output, "\nPath,Price\n");
    for (int i = 0; i < config->num_simulations; i++) {
        fprintf(output, "%d,%.2f\n", i + 1, results->prices[i]);
    }
}

void free_simulation_results(simulation_results_t *results) {
    if (results) {
        free(results->prices);
        results->prices = NULL;
    }
}

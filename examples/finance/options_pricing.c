#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "options_pricing.h"
#include "heston_model.h"
#include "../../src/quantum_rng/quantum_rng.h"

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

// Standard normal cumulative distribution function
static double norm_cdf(double x) {
    return 0.5 * (1.0 + erf(x / M_SQRT2));
}

// Standard normal probability density function
static double norm_pdf(double x) {
    return exp(-0.5 * x * x) / sqrt(2.0 * M_PI);
}

/*
 * Black-Scholes analytical price for European calls and puts with a
 * continuous dividend yield.  Used as a reference alongside the Monte
 * Carlo estimator and for closed-form Greeks under constant volatility.
 */
double black_scholes_price(const option_params_t *params) {
    double S = params->spot_price;
    double K = params->strike_price;
    double r = params->risk_free_rate;
    double q = params->dividend_yield;
    double sigma = params->volatility;
    double T = params->time_to_maturity;

    if (S <= 0.0 || K <= 0.0 || T <= 0.0 || sigma <= 0.0) return NAN;

    double d1 = (log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrt(T));
    double d2 = d1 - sigma * sqrt(T);

    switch (params->type) {
        case OPTION_CALL:
            return S * exp(-q * T) * norm_cdf(d1) - K * exp(-r * T) * norm_cdf(d2);
        case OPTION_PUT:
            return K * exp(-r * T) * norm_cdf(-d2) - S * exp(-q * T) * norm_cdf(-d1);
        default:
            return NAN;  // No closed form for the exotic payoffs here
    }
}

/*
 * Closed-form Black-Scholes Greeks for European calls and puts.
 * Vega and rho are per unit change (1.0 = 100 percentage points).
 */
void black_scholes_greeks(const option_params_t *params, greeks_t *greeks) {
    double S = params->spot_price;
    double K = params->strike_price;
    double r = params->risk_free_rate;
    double q = params->dividend_yield;
    double sigma = params->volatility;
    double T = params->time_to_maturity;

    memset(greeks, 0, sizeof(*greeks));
    if (S <= 0.0 || K <= 0.0 || T <= 0.0 || sigma <= 0.0) return;

    double sqrtT = sqrt(T);
    double d1 = (log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;
    double disc_q = exp(-q * T);
    double disc_r = exp(-r * T);

    greeks->gamma = disc_q * norm_pdf(d1) / (S * sigma * sqrtT);
    greeks->vega  = S * disc_q * norm_pdf(d1) * sqrtT;

    if (params->type == OPTION_CALL) {
        greeks->delta = disc_q * norm_cdf(d1);
        greeks->theta = -S * disc_q * norm_pdf(d1) * sigma / (2.0 * sqrtT)
                        - r * K * disc_r * norm_cdf(d2)
                        + q * S * disc_q * norm_cdf(d1);
        greeks->rho   = K * T * disc_r * norm_cdf(d2);
    } else {  // OPTION_PUT
        greeks->delta = -disc_q * norm_cdf(-d1);
        greeks->theta = -S * disc_q * norm_pdf(d1) * sigma / (2.0 * sqrtT)
                        + r * K * disc_r * norm_cdf(-d2)
                        - q * S * disc_q * norm_cdf(-d1);
        greeks->rho   = -K * T * disc_r * norm_cdf(-d2);
    }
}

const char* get_option_type_name(option_type_t type) {
    switch (type) {
        case OPTION_CALL: return "call";
        case OPTION_PUT: return "put";
        case OPTION_BINARY_CALL: return "binary_call";
        case OPTION_BINARY_PUT: return "binary_put";
        case OPTION_ASIAN_CALL: return "asian_call";
        case OPTION_ASIAN_PUT: return "asian_put";
        case OPTION_LOOKBACK_CALL: return "lookback_call";
        case OPTION_LOOKBACK_PUT: return "lookback_put";
        default: return "unknown";
    }
}

void init_pricing_config(pricing_config_t *config) {
    memset(config, 0, sizeof(pricing_config_t));
    config->num_paths = DEFAULT_NUM_PATHS;
    config->time_steps = DEFAULT_TIME_STEPS;
    config->option.spot_price = DEFAULT_SPOT_PRICE;
    config->option.strike_price = DEFAULT_STRIKE_PRICE;
    config->option.time_to_maturity = DEFAULT_TIME_TO_MATURITY;
    config->option.volatility = DEFAULT_VOLATILITY;
    config->option.risk_free_rate = DEFAULT_RISK_FREE_RATE;
    config->option.dividend_yield = DEFAULT_DIVIDEND_YIELD;
    config->option.type = OPTION_CALL;
    config->output_mode = OUTPUT_NORMAL;
    config->show_progress = 1;
    config->greek_flags = GREEK_NONE;
    config->show_paths = 0;
    config->vol_model = VOL_CONSTANT;
}

double calculate_payoff(double spot, double strike, option_type_t type) {
    switch (type) {
        case OPTION_CALL:
            return fmax(spot - strike, 0.0);
        case OPTION_PUT:
            return fmax(strike - spot, 0.0);
        case OPTION_BINARY_CALL:
            return spot > strike ? 1.0 : 0.0;
        case OPTION_BINARY_PUT:
            return spot < strike ? 1.0 : 0.0;
        default:
            return 0.0;
    }
}

static void simulate_path(double *path, int steps, const pricing_config_t *config,
                          qrng_ctx *ctx, double *cache, int *has_cache) {
    double dt = config->option.time_to_maturity / steps;
    double drift = (config->option.risk_free_rate - config->option.dividend_yield
                   - 0.5 * config->option.volatility * config->option.volatility) * dt;
    double vol_sqrt_dt = config->option.volatility * sqrt(dt);

    path[0] = config->option.spot_price;
    for (int i = 1; i <= steps; i++) {
        double z = generate_normal(ctx, cache, has_cache);
        path[i] = path[i-1] * exp(drift + vol_sqrt_dt * z);
    }
}

/*
 * Payoff for a full simulated path.  Vanilla and binary payoffs only need
 * the terminal price; Asian payoffs use the arithmetic average and
 * lookback payoffs the path extremes (fixed strike).
 */
static double path_payoff(const double *path, int steps, const pricing_config_t *config) {
    double strike = config->option.strike_price;

    switch (config->option.type) {
        case OPTION_ASIAN_CALL:
        case OPTION_ASIAN_PUT: {
            double avg = 0.0;
            for (int i = 1; i <= steps; i++) avg += path[i];
            avg /= steps;
            return config->option.type == OPTION_ASIAN_CALL
                   ? fmax(avg - strike, 0.0)
                   : fmax(strike - avg, 0.0);
        }
        case OPTION_LOOKBACK_CALL: {
            double s_max = path[0];
            for (int i = 1; i <= steps; i++) if (path[i] > s_max) s_max = path[i];
            return fmax(s_max - strike, 0.0);
        }
        case OPTION_LOOKBACK_PUT: {
            double s_min = path[0];
            for (int i = 1; i <= steps; i++) if (path[i] < s_min) s_min = path[i];
            return fmax(strike - s_min, 0.0);
        }
        default:
            return calculate_payoff(path[steps], strike, config->option.type);
    }
}

// Validate config; returns 0 and fills results with NaN on invalid input.
static int validate_pricing_config(const pricing_config_t *config, pricing_results_t *results) {
    int ok = config != NULL &&
             config->option.spot_price > 0.0 &&
             config->option.strike_price > 0.0 &&
             config->option.time_to_maturity > 0.0 &&
             config->option.volatility >= 0.0 &&
             config->num_paths >= MIN_PATHS &&
             config->num_paths <= MAX_PATHS &&
             config->time_steps >= 1;

    if (ok && config->vol_model == VOL_HESTON) {
        ok = config->heston.kappa > 0.0 &&
             config->heston.theta >= 0.0 &&
             config->heston.sigma >= 0.0 &&
             config->heston.v0 >= 0.0 &&
             fabs(config->heston.rho) <= 1.0;
    }

    if (!ok && results) {
        results->price = NAN;
        results->std_error = NAN;
        results->confidence_lower = NAN;
        results->confidence_upper = NAN;
    }
    return ok;
}

pricing_results_t run_pricing_simulation(const pricing_config_t *config) {
    pricing_results_t results = {0};

    if (!validate_pricing_config(config, &results)) {
        return results;
    }

    // Stochastic volatility model is handled by the Heston engine
    if (config->vol_model == VOL_HESTON) {
        return run_heston_simulation(config, &config->heston);
    }

    // Pass NULL when unseeded: qrng_init indexes the seed with
    // (i % seed_len) for any non-NULL pointer, so seed_len == 0 with a
    // non-NULL pointer would divide by zero.
    qrng_ctx *ctx;
    qrng_error err = qrng_init(&ctx,
                               config->seed_length > 0 ? (const uint8_t*)config->seed : NULL,
                               config->seed_length);
    if (err != QRNG_SUCCESS) {
        fprintf(stderr, "Failed to initialize quantum RNG: %s\n", qrng_error_string(err));
        results.price = NAN;
        results.std_error = NAN;
        return results;
    }

    int steps = config->time_steps;
    double *all_paths = NULL;
    if (config->show_paths) {
        all_paths = (double *)malloc((size_t)config->num_paths * (steps + 1) * sizeof(double));
        if (!all_paths) {
            fprintf(stderr, "Failed to allocate path storage\n");
            qrng_free(ctx);
            results.price = NAN;
            results.std_error = NAN;
            return results;
        }
    }

    double sum = 0.0, sum_squared = 0.0;
    double discount = exp(-config->option.risk_free_rate * config->option.time_to_maturity);
    double normal_cache = 0.0;
    int has_normal_cache = 0;
    int progress_step = config->num_paths / 10;
    if (progress_step < 1) progress_step = 1;

    double *path = (double *)malloc((steps + 1) * sizeof(double));
    if (!path) {
        fprintf(stderr, "Failed to allocate path buffer\n");
        free(all_paths);
        qrng_free(ctx);
        results.price = NAN;
        results.std_error = NAN;
        return results;
    }

    for (int i = 0; i < config->num_paths; i++) {
        simulate_path(path, steps, config, ctx, &normal_cache, &has_normal_cache);

        if (config->show_paths) {
            memcpy(&all_paths[(size_t)i * (steps + 1)], path, (steps + 1) * sizeof(double));
        }

        double payoff = path_payoff(path, steps, config);
        double discounted_payoff = payoff * discount;

        sum += discounted_payoff;
        sum_squared += discounted_payoff * discounted_payoff;

        if (config->show_progress && (i + 1) % progress_step == 0) {
            printf("Progress: %d%%\n", (i + 1) * 100 / config->num_paths);
            fflush(stdout);
        }
    }

    free(path);
    qrng_free(ctx);

    // Calculate price and error statistics
    results.price = sum / config->num_paths;
    double variance = (sum_squared / config->num_paths - results.price * results.price);
    results.std_error = sqrt(fmax(variance, 0.0) / config->num_paths);

    // 95% confidence interval
    double confidence_width = 1.96 * results.std_error;
    results.confidence_lower = results.price - confidence_width;
    results.confidence_upper = results.price + confidence_width;

    if (config->show_paths) {
        results.paths = all_paths;
        results.paths_size = config->num_paths * (steps + 1);
    }

    return results;
}

// Helper for the finite-difference Greeks: price with one bumped config.
static double bumped_price(pricing_config_t temp_config) {
    pricing_results_t r = run_pricing_simulation(&temp_config);
    free_pricing_results(&r);
    return r.price;
}

void calculate_greeks(pricing_results_t *results, const pricing_config_t *config) {
    if (!config->greek_flags) return;

    /*
     * For European calls/puts under constant volatility the Black-Scholes
     * Greeks are known in closed form.  Use them: the quantum RNG mixes in
     * hardware entropy on every draw, so paths are not reproducible across
     * runs and Monte Carlo finite differences would be dominated by
     * sampling noise.
     */
    if (config->vol_model == VOL_CONSTANT &&
        (config->option.type == OPTION_CALL || config->option.type == OPTION_PUT)) {
        greeks_t bs;
        black_scholes_greeks(&config->option, &bs);
        if (config->greek_flags & GREEK_DELTA) results->greeks.delta = bs.delta;
        if (config->greek_flags & GREEK_GAMMA) results->greeks.gamma = bs.gamma;
        if (config->greek_flags & GREEK_THETA) results->greeks.theta = bs.theta;
        if (config->greek_flags & GREEK_VEGA)  results->greeks.vega  = bs.vega;
        if (config->greek_flags & GREEK_RHO)   results->greeks.rho   = bs.rho;
        return;
    }

    /*
     * Otherwise fall back to Monte Carlo central differences.  These
     * estimates carry statistical noise of the order of the pricing
     * standard error divided by the bump size.
     */
    pricing_config_t temp_config = *config;
    temp_config.num_paths = (int)(config->num_paths * GREEK_PATHS_MULTIPLIER);
    if (temp_config.num_paths < MIN_PATHS) temp_config.num_paths = MIN_PATHS;
    temp_config.show_progress = 0;
    temp_config.show_paths = 0;
    temp_config.greek_flags = GREEK_NONE;

    printf("Calculating Monte Carlo Greeks with %d paths per estimate...\n",
           temp_config.num_paths);
    fflush(stdout);

    // Re-estimate the base price with the same path count as the bumps so
    // the finite differences compare like with like.
    double base = bumped_price(temp_config);

    if (config->greek_flags & (GREEK_DELTA | GREEK_GAMMA)) {
        double h = 0.01 * config->option.spot_price;
        temp_config.option.spot_price = config->option.spot_price + h;
        double up = bumped_price(temp_config);
        temp_config.option.spot_price = config->option.spot_price - h;
        double down = bumped_price(temp_config);
        temp_config.option.spot_price = config->option.spot_price;

        if (config->greek_flags & GREEK_DELTA)
            results->greeks.delta = (up - down) / (2.0 * h);          // central difference
        if (config->greek_flags & GREEK_GAMMA)
            results->greeks.gamma = (up - 2.0 * base + down) / (h * h); // central second difference
    }

    if (config->greek_flags & GREEK_THETA) {
        double h = 1.0 / 365.0;   // one day
        temp_config.option.time_to_maturity = config->option.time_to_maturity - h;
        double shorter = bumped_price(temp_config);
        temp_config.option.time_to_maturity = config->option.time_to_maturity;
        results->greeks.theta = (shorter - base) / h;
    }

    if (config->greek_flags & GREEK_VEGA) {
        double h = 0.01;          // one volatility point
        temp_config.option.volatility = config->option.volatility + h;
        double up = bumped_price(temp_config);
        temp_config.option.volatility = config->option.volatility - h;
        double down = bumped_price(temp_config);
        temp_config.option.volatility = config->option.volatility;
        results->greeks.vega = (up - down) / (2.0 * h);
    }

    if (config->greek_flags & GREEK_RHO) {
        double h = 0.01;          // one rate point
        temp_config.option.risk_free_rate = config->option.risk_free_rate + h;
        double up = bumped_price(temp_config);
        temp_config.option.risk_free_rate = config->option.risk_free_rate - h;
        double down = bumped_price(temp_config);
        temp_config.option.risk_free_rate = config->option.risk_free_rate;
        results->greeks.rho = (up - down) / (2.0 * h);
    }

    printf("All Greeks calculations complete.\n");
    fflush(stdout);
}

void print_results(FILE *output, const pricing_results_t *results, const pricing_config_t *config) {
    fprintf(output, "\n\n");
    fprintf(output, "Option Pricing Results\n");
    fprintf(output, "=====================\n\n");
    fprintf(output, "Parameters:\n");
    fprintf(output, "-----------\n");
    fprintf(output, "Option Type:       %s\n", get_option_type_name(config->option.type));
    fprintf(output, "Spot Price:        %.2f\n", config->option.spot_price);
    fprintf(output, "Strike Price:      %.2f\n", config->option.strike_price);
    fprintf(output, "Time to Maturity:  %.2f years\n", config->option.time_to_maturity);
    fprintf(output, "Volatility:        %.2f%%\n", config->option.volatility * 100);
    fprintf(output, "Risk-free Rate:    %.2f%%\n", config->option.risk_free_rate * 100);
    fprintf(output, "Dividend Yield:    %.2f%%\n\n", config->option.dividend_yield * 100);
    
    fprintf(output, "Results:\n");
    fprintf(output, "--------\n");
    fprintf(output, "Option Price:      %.4f\n", results->price);
    fprintf(output, "Standard Error:    %.4f\n", results->std_error);
    fprintf(output, "95%% CI:           [%.4f, %.4f]\n\n", 
            results->confidence_lower, results->confidence_upper);
    
    if (config->greek_flags) {
        fprintf(output, "Greeks:\n");
        fprintf(output, "-------\n");
        if (config->greek_flags & GREEK_DELTA)
            fprintf(output, "Delta:            %.4f\n", results->greeks.delta);
        if (config->greek_flags & GREEK_GAMMA)
            fprintf(output, "Gamma:            %.4f\n", results->greeks.gamma);
        if (config->greek_flags & GREEK_THETA)
            fprintf(output, "Theta:            %.4f\n", results->greeks.theta);
        if (config->greek_flags & GREEK_VEGA)
            fprintf(output, "Vega:             %.4f\n", results->greeks.vega);
        if (config->greek_flags & GREEK_RHO)
            fprintf(output, "Rho:              %.4f\n", results->greeks.rho);
        fprintf(output, "\n");
    }
    
    fflush(output);
}

void output_results_json(FILE *output, const pricing_results_t *results, const pricing_config_t *config) {
    fprintf(output, "{\n");
    fprintf(output, "  \"option\": {\n");
    fprintf(output, "    \"type\": \"%s\",\n", get_option_type_name(config->option.type));
    fprintf(output, "    \"spot_price\": %.2f,\n", config->option.spot_price);
    fprintf(output, "    \"strike_price\": %.2f,\n", config->option.strike_price);
    fprintf(output, "    \"time_to_maturity\": %.2f,\n", config->option.time_to_maturity);
    
    if (config->vol_model == VOL_CONSTANT) {
        fprintf(output, "    \"volatility_model\": \"constant\",\n");
        fprintf(output, "    \"volatility\": %.2f,\n", config->option.volatility);
    } else {
        fprintf(output, "    \"volatility_model\": \"heston\",\n");
        fprintf(output, "    \"heston_params\": {\n");
        fprintf(output, "      \"kappa\": %.2f,\n", config->heston.kappa);
        fprintf(output, "      \"theta\": %.2f,\n", config->heston.theta);
        fprintf(output, "      \"sigma\": %.2f,\n", config->heston.sigma);
        fprintf(output, "      \"rho\": %.2f,\n", config->heston.rho);
        fprintf(output, "      \"v0\": %.2f\n", config->heston.v0);
        fprintf(output, "    },\n");
    }
    
    fprintf(output, "    \"risk_free_rate\": %.2f,\n", config->option.risk_free_rate);
    fprintf(output, "    \"dividend_yield\": %.2f\n", config->option.dividend_yield);
    fprintf(output, "  },\n");
    fprintf(output, "  \"results\": {\n");
    fprintf(output, "    \"price\": %.4f,\n", results->price);
    fprintf(output, "    \"std_error\": %.4f,\n", results->std_error);
    fprintf(output, "    \"confidence_interval\": {\n");
    fprintf(output, "      \"lower\": %.4f,\n", results->confidence_lower);
    fprintf(output, "      \"upper\": %.4f\n", results->confidence_upper);
    fprintf(output, "    }");

    if (config->greek_flags) {
        fprintf(output, ",\n    \"greeks\": {\n");
        if (config->greek_flags & GREEK_DELTA)
            fprintf(output, "      \"delta\": %.4f,\n", results->greeks.delta);
        if (config->greek_flags & GREEK_GAMMA)
            fprintf(output, "      \"gamma\": %.4f,\n", results->greeks.gamma);
        if (config->greek_flags & GREEK_THETA)
            fprintf(output, "      \"theta\": %.4f,\n", results->greeks.theta);
        if (config->greek_flags & GREEK_VEGA)
            fprintf(output, "      \"vega\": %.4f,\n", results->greeks.vega);
        if (config->greek_flags & GREEK_RHO)
            fprintf(output, "      \"rho\": %.4f\n", results->greeks.rho);
        fprintf(output, "    }");
    }

    if (config->show_paths && results->paths) {
        fprintf(output, ",\n    \"paths\": [");
        for (int i = 0; i < results->paths_size; i++) {
            fprintf(output, "%.4f%s", results->paths[i], 
                    i < results->paths_size - 1 ? ", " : "");
        }
        fprintf(output, "]");
    }

    fprintf(output, "\n  }\n");
    fprintf(output, "}\n");
    fflush(output);
}

void output_results_csv(FILE *output, const pricing_results_t *results, const pricing_config_t *config) {
    // Header
    fprintf(output, "Type,Spot,Strike,Maturity,Volatility,Rate,Dividend,Price,StdError,CI_Lower,CI_Upper");
    if (config->greek_flags) {
        if (config->greek_flags & GREEK_DELTA) fprintf(output, ",Delta");
        if (config->greek_flags & GREEK_GAMMA) fprintf(output, ",Gamma");
        if (config->greek_flags & GREEK_THETA) fprintf(output, ",Theta");
        if (config->greek_flags & GREEK_VEGA) fprintf(output, ",Vega");
        if (config->greek_flags & GREEK_RHO) fprintf(output, ",Rho");
    }
    fprintf(output, "\n");
    
    // Data
    fprintf(output, "%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.4f,%.4f,%.4f,%.4f",
            get_option_type_name(config->option.type),
            config->option.spot_price,
            config->option.strike_price,
            config->option.time_to_maturity,
            config->option.volatility,
            config->option.risk_free_rate,
            config->option.dividend_yield,
            results->price,
            results->std_error,
            results->confidence_lower,
            results->confidence_upper);
    
    if (config->greek_flags) {
        if (config->greek_flags & GREEK_DELTA) fprintf(output, ",%.4f", results->greeks.delta);
        if (config->greek_flags & GREEK_GAMMA) fprintf(output, ",%.4f", results->greeks.gamma);
        if (config->greek_flags & GREEK_THETA) fprintf(output, ",%.4f", results->greeks.theta);
        if (config->greek_flags & GREEK_VEGA) fprintf(output, ",%.4f", results->greeks.vega);
        if (config->greek_flags & GREEK_RHO) fprintf(output, ",%.4f", results->greeks.rho);
    }
    fprintf(output, "\n");
    fflush(output);
}

void free_pricing_results(pricing_results_t *results) {
    if (results->paths) {
        free(results->paths);
        results->paths = NULL;
        results->paths_size = 0;
    }
}

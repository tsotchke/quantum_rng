#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "heston_model.h"

void init_heston_params(heston_params_t *params) {
    params->kappa = DEFAULT_KAPPA;
    params->theta = DEFAULT_THETA;
    params->sigma = DEFAULT_SIGMA;
    params->rho = DEFAULT_RHO;
    params->v0 = DEFAULT_V0;
}

/*
 * One Euler step of the Heston stochastic volatility model:
 *
 *   dS = (r - q) S dt + sqrt(v) S dW_s
 *   dv = kappa (theta - v) dt + sigma sqrt(v) dW_v,   corr(dW_s, dW_v) = rho
 *
 * The stock is stepped in log space (exact for constant variance over the
 * step), which requires the -0.5 v dt Ito correction so the discounted
 * expectation of S stays a martingale.  Negative variance excursions of the
 * Euler scheme are truncated at zero (full truncation scheme).
 */
double calculate_heston_price_path(double spot, double *variance, double dt, double drift_dt,
                                   const heston_params_t *params, qrng_ctx *ctx) {
    // Box-Muller: two uniforms -> two independent standard normals
    double u1;
    do {
        u1 = qrng_double(ctx);   /* qrng_double is in [0,1); reject 0 for log() */
    } while (u1 <= 0.0);
    double u2 = qrng_double(ctx);
    double r = sqrt(-2.0 * log(u1));
    double z1 = r * cos(2.0 * M_PI * u2);
    double z2 = r * sin(2.0 * M_PI * u2);

    // Correlate the stock and variance shocks
    double z_s = z1;
    double z_v = params->rho * z1 + sqrt(1.0 - params->rho * params->rho) * z2;

    double v_pos = fmax(*variance, 0.0);

    // Update variance using the square-root (CIR) process
    double var_drift = params->kappa * (params->theta - v_pos) * dt;
    double var_diffusion = params->sigma * sqrt(v_pos * dt) * z_v;
    double new_variance = *variance + var_drift + var_diffusion;
    if (new_variance < 0.0) new_variance = 0.0;  // Prevent negative variance

    // Update stock price in log space with the Ito correction
    double new_spot = spot * exp(drift_dt - 0.5 * v_pos * dt + sqrt(v_pos * dt) * z_s);

    *variance = new_variance;
    return new_spot;
}

pricing_results_t run_heston_simulation(const pricing_config_t *config, const heston_params_t *heston) {
    pricing_results_t results = {0};

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
    double dt = config->option.time_to_maturity / steps;
    double drift_dt = (config->option.risk_free_rate - config->option.dividend_yield) * dt;

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
        double variance = heston->v0;
        path[0] = config->option.spot_price;

        for (int j = 1; j <= steps; j++) {
            path[j] = calculate_heston_price_path(path[j-1], &variance, dt, drift_dt,
                                                  heston, ctx);
        }

        if (config->show_paths) {
            memcpy(&all_paths[(size_t)i * (steps + 1)], path, (steps + 1) * sizeof(double));
        }

        double payoff = calculate_payoff(path[steps], config->option.strike_price, config->option.type);
        double discounted_payoff = payoff * discount;

        sum += discounted_payoff;
        sum_squared += discounted_payoff * discounted_payoff;
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

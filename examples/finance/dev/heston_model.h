#ifndef HESTON_MODEL_H
#define HESTON_MODEL_H

#include "../../src/quantum_rng/quantum_rng.h"
#include "options_pricing.h"

// Function declarations
void init_heston_params(heston_params_t *params);
pricing_results_t run_heston_simulation(const pricing_config_t *config, const heston_params_t *heston);

/*
 * Advance one Euler step of the Heston model.  Returns the new spot price
 * and updates *variance in place with the new variance level.
 * drift_dt is the risk-neutral drift (r - q) * dt.
 */
double calculate_heston_price_path(double spot, double *variance, double dt, double drift_dt,
                                   const heston_params_t *params, qrng_ctx *ctx);

#endif /* HESTON_MODEL_H */

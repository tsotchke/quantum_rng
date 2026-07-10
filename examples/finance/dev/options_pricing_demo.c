#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "options_pricing.h"

int main() {
    printf("Starting options pricing simulation...\n");
    fflush(stdout);

    pricing_config_t config;
    init_pricing_config(&config);

    // Set up a basic European call option
    config.option.type = OPTION_CALL;
    config.option.spot_price = 100.0;
    config.option.strike_price = 100.0;
    config.option.volatility = 0.2;
    config.option.time_to_maturity = 1.0;
    config.option.risk_free_rate = 0.05;
    config.option.dividend_yield = 0.02;
    config.num_paths = 10000;
    config.greek_flags = GREEK_ALL;
    config.show_progress = 1;

    // Generate a seed using system time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t seed_val = ((uint64_t)tv.tv_sec << 32) | tv.tv_usec;
    memcpy(config.seed, &seed_val, sizeof(seed_val));
    config.seed_length = sizeof(seed_val);

    printf("Running simulation with %d paths...\n", config.num_paths);
    fflush(stdout);

    // Run simulation
    pricing_results_t results = run_pricing_simulation(&config);

    printf("Initial price calculation complete.\n");
    printf("Calculating Greeks...\n");
    fflush(stdout);

    // Calculate Greeks
    calculate_greeks(&results, &config);

    // Output results
    print_results(stdout, &results, &config);

    // Cleanup
    free_pricing_results(&results);

    printf("Simulation complete.\n\n");
    fflush(stdout);

    return 0;
}

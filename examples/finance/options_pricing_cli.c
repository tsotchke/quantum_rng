/*
 * options_pricing_cli.c - Command-line front end for the quantum-RNG option
 * pricing engine (options_pricing.c / heston_model.c).
 *
 * This file only provides main(), argument parsing and result presentation.
 * All numerical work is done by the library functions declared in
 * options_pricing.h:
 *
 *   black_scholes_price()      - analytic European call/put reference
 *   run_pricing_simulation()   - Monte Carlo pricing (constant vol or Heston)
 *   calculate_greeks()         - closed-form or finite-difference Greeks
 *   print_results() / output_results_json() / output_results_csv()
 *
 * Build (see the finance README / Makefile target `options_pricing`):
 *   links against options_pricing.o, heston_model.o and the quantum RNG lib.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

#include "options_pricing.h"
#include "heston_model.h"

/* Long-only options for the Heston parameters (no natural short letters). */
enum {
    OPT_HESTON_KAPPA = 1000,
    OPT_HESTON_THETA,
    OPT_HESTON_SIGMA,
    OPT_HESTON_RHO,
    OPT_HESTON_V0
};

static void usage(const char *prog) {
    printf(
"Usage: %s [options]\n"
"\n"
"Prices an option with the quantum-RNG Monte Carlo engine.  For European\n"
"calls and puts under constant volatility the Black-Scholes analytic price\n"
"is printed alongside the Monte Carlo estimate.\n"
"\n"
"Contract parameters:\n"
"  -S <spot>        Spot price of the underlying        (default %.2f)\n"
"  -K <strike>      Strike price                         (default %.2f)\n"
"  -T <years>       Time to maturity in years            (default %.2f)\n"
"  -v <vol>         Volatility, e.g. 0.2 = 20%%           (default %.2f)\n"
"  -r <rate>        Risk-free rate, e.g. 0.05 = 5%%       (default %.2f)\n"
"  -q <yield>       Continuous dividend yield            (default %.2f)\n"
"  -y <type>        Option type: call, put, binary_call, binary_put,\n"
"                   asian_call, asian_put, lookback_call, lookback_put\n"
"                                                        (default call)\n"
"\n"
"Model:\n"
"  -M <model>       Volatility model: bs (constant) or heston (default bs)\n"
"  --heston-kappa <k>   Mean-reversion rate      (default %.2f)\n"
"  --heston-theta <t>   Long-run variance        (default %.2f)\n"
"  --heston-sigma <s>   Vol of variance          (default %.2f)\n"
"  --heston-rho <r>     Spot/vol correlation     (default %.2f)\n"
"  --heston-v0 <v>      Initial variance         (default %.2f)\n"
"\n"
"Simulation / output:\n"
"  -n <paths>       Number of Monte Carlo paths          (default %d)\n"
"  -G <greeks>      Greeks to compute: any of d,g,t,v,r (delta,gamma,\n"
"                   theta,vega,rho), or 'all', or 'none'  (default none)\n"
"  -o <mode>        Output: normal, quiet, verbose, json, csv (default normal)\n"
"  -p               Store and show individual price paths\n"
"  -s <seed>        Seed string for the RNG (omit for hardware entropy)\n"
"  -h               Show this help and exit\n",
        prog,
        DEFAULT_SPOT_PRICE, DEFAULT_STRIKE_PRICE, DEFAULT_TIME_TO_MATURITY,
        DEFAULT_VOLATILITY, DEFAULT_RISK_FREE_RATE, DEFAULT_DIVIDEND_YIELD,
        DEFAULT_KAPPA, DEFAULT_THETA, DEFAULT_SIGMA, DEFAULT_RHO, DEFAULT_V0,
        DEFAULT_NUM_PATHS);
}

/* Map an option-type name to the enum; returns -1 on an unknown name. */
static int parse_option_type(const char *s, option_type_t *out) {
    static const struct { const char *name; option_type_t type; } table[] = {
        { "call",          OPTION_CALL },
        { "put",           OPTION_PUT },
        { "binary_call",   OPTION_BINARY_CALL },
        { "binary_put",    OPTION_BINARY_PUT },
        { "asian_call",    OPTION_ASIAN_CALL },
        { "asian_put",     OPTION_ASIAN_PUT },
        { "lookback_call", OPTION_LOOKBACK_CALL },
        { "lookback_put",  OPTION_LOOKBACK_PUT },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(s, table[i].name) == 0) {
            *out = table[i].type;
            return 0;
        }
    }
    return -1;
}

/* Map an output-mode name to the enum; returns -1 on an unknown name. */
static int parse_output_mode(const char *s, output_mode_t *out) {
    if      (strcmp(s, "normal") == 0)  *out = OUTPUT_NORMAL;
    else if (strcmp(s, "quiet") == 0)   *out = OUTPUT_QUIET;
    else if (strcmp(s, "verbose") == 0) *out = OUTPUT_VERBOSE;
    else if (strcmp(s, "json") == 0)    *out = OUTPUT_JSON;
    else if (strcmp(s, "csv") == 0)     *out = OUTPUT_CSV;
    else return -1;
    return 0;
}

/*
 * Parse a Greeks selector such as "all", "none" or a set of letters like
 * "dgt" / "d,g,t".  Returns the flag bitmask, or -1 on an unknown letter.
 */
static int parse_greeks(const char *s) {
    if (strcmp(s, "all") == 0)  return GREEK_ALL;
    if (strcmp(s, "none") == 0) return GREEK_NONE;

    int flags = GREEK_NONE;
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case 'd': case 'D': flags |= GREEK_DELTA; break;
            case 'g': case 'G': flags |= GREEK_GAMMA; break;
            case 't': case 'T': flags |= GREEK_THETA; break;
            case 'v': case 'V': flags |= GREEK_VEGA;  break;
            case 'r': case 'R': flags |= GREEK_RHO;   break;
            case ',': case ' ': break;   /* allow comma/space separated lists */
            default:
                fprintf(stderr, "Unknown Greek '%c' (use d,g,t,v,r or 'all')\n", *p);
                return -1;
        }
    }
    return flags;
}

int main(int argc, char *argv[]) {
    pricing_config_t config;
    init_pricing_config(&config);
    init_heston_params(&config.heston);   /* used only if -M heston */

    static struct option long_opts[] = {
        { "heston-kappa", required_argument, 0, OPT_HESTON_KAPPA },
        { "heston-theta", required_argument, 0, OPT_HESTON_THETA },
        { "heston-sigma", required_argument, 0, OPT_HESTON_SIGMA },
        { "heston-rho",   required_argument, 0, OPT_HESTON_RHO },
        { "heston-v0",    required_argument, 0, OPT_HESTON_V0 },
        { "help",         no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "S:K:T:v:r:q:M:y:n:G:o:ps:h",
                              long_opts, NULL)) != -1) {
        switch (opt) {
            case 'S': config.option.spot_price = atof(optarg); break;
            case 'K': config.option.strike_price = atof(optarg); break;
            case 'T': config.option.time_to_maturity = atof(optarg); break;
            case 'v': config.option.volatility = atof(optarg); break;
            case 'r': config.option.risk_free_rate = atof(optarg); break;
            case 'q': config.option.dividend_yield = atof(optarg); break;
            case 'M':
                if (strcmp(optarg, "bs") == 0) {
                    config.vol_model = VOL_CONSTANT;
                } else if (strcmp(optarg, "heston") == 0) {
                    config.vol_model = VOL_HESTON;
                } else {
                    fprintf(stderr, "Unknown model '%s' (use bs or heston)\n", optarg);
                    return 1;
                }
                break;
            case 'y':
                if (parse_option_type(optarg, &config.option.type) != 0) {
                    fprintf(stderr, "Unknown option type '%s'\n", optarg);
                    return 1;
                }
                break;
            case 'n': config.num_paths = atoi(optarg); break;
            case 'G': {
                int g = parse_greeks(optarg);
                if (g < 0) return 1;
                config.greek_flags = g;
                break;
            }
            case 'o':
                if (parse_output_mode(optarg, &config.output_mode) != 0) {
                    fprintf(stderr, "Unknown output mode '%s'\n", optarg);
                    return 1;
                }
                break;
            case 'p': config.show_paths = 1; break;
            case 's':
                strncpy(config.seed, optarg, sizeof(config.seed) - 1);
                config.seed[sizeof(config.seed) - 1] = '\0';
                config.seed_length = (int)strlen(config.seed);
                break;
            case 'h': usage(argv[0]); return 0;
            default:  usage(argv[0]); return 1;
        }
    }

    /* Basic validation with actionable messages. */
    if (config.option.spot_price <= 0.0 || config.option.strike_price <= 0.0) {
        fprintf(stderr, "Error: spot (-S) and strike (-K) must be positive.\n");
        return 1;
    }
    if (config.option.time_to_maturity <= 0.0) {
        fprintf(stderr, "Error: time to maturity (-T) must be positive.\n");
        return 1;
    }
    if (config.option.volatility < 0.0) {
        fprintf(stderr, "Error: volatility (-v) must be non-negative.\n");
        return 1;
    }
    if (config.num_paths < MIN_PATHS || config.num_paths > MAX_PATHS) {
        fprintf(stderr, "Error: number of paths (-n) must be between %d and %d.\n",
                MIN_PATHS, MAX_PATHS);
        return 1;
    }

    /* Progress and human-readable chatter would corrupt machine formats. */
    int machine_output = (config.output_mode == OUTPUT_JSON ||
                          config.output_mode == OUTPUT_CSV ||
                          config.output_mode == OUTPUT_QUIET);
    config.show_progress = machine_output ? 0 : config.show_progress;

    int has_analytic = (config.vol_model == VOL_CONSTANT &&
                        (config.option.type == OPTION_CALL ||
                         config.option.type == OPTION_PUT));

    pricing_results_t results = run_pricing_simulation(&config);

    if (isnan(results.price)) {
        fprintf(stderr, "Error: pricing failed (check parameters).\n");
        return 1;
    }

    if (config.greek_flags) {
        calculate_greeks(&results, &config);
    }

    switch (config.output_mode) {
        case OUTPUT_JSON:
            output_results_json(stdout, &results, &config);
            break;
        case OUTPUT_CSV:
            output_results_csv(stdout, &results, &config);
            break;
        case OUTPUT_QUIET:
            printf("%.4f\n", results.price);
            break;
        case OUTPUT_VERBOSE:
            printf("Model:             %s\n",
                   config.vol_model == VOL_HESTON ? "Heston" : "Black-Scholes (constant vol)");
            printf("Monte Carlo paths: %d\n", config.num_paths);
            printf("Time steps:        %d\n", config.time_steps);
            printf("RNG seed:          %s\n",
                   config.seed_length > 0 ? config.seed : "(hardware entropy)");
            /* fall through to the normal report */
            /* FALLTHROUGH */
        case OUTPUT_NORMAL:
        default:
            print_results(stdout, &results, &config);
            if (has_analytic) {
                double bs = black_scholes_price(&config.option);
                printf("Black-Scholes (analytic): %.4f\n", bs);
                printf("Monte Carlo - Analytic:   %+.4f\n", results.price - bs);
                printf("\n");
            }
            break;
    }

    free_pricing_results(&results);
    return 0;
}

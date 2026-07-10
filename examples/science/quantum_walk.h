#ifndef QUANTUM_WALK_H
#define QUANTUM_WALK_H

#include <stdint.h>
#include <stddef.h>
#include "../../src/quantum_rng/quantum_rng.h"

// Simulation parameters. The walker state grows with the number of steps
// and the position bookkeeping is quadratic in the support size, so the
// step limit is kept modest to guarantee reasonable runtimes.
#define MIN_STEPS 10
#define MAX_STEPS 5000
#define DEFAULT_STEPS 200
#define MIN_WALKERS 1
#define MAX_WALKERS 1000
#define DEFAULT_WALKERS 10
#define DEFAULT_DIMENSIONS 1
#define MAX_DIMENSIONS 3

// Analysis parameters
#define HISTOGRAM_BINS 100
#define CONFIDENCE_LEVEL 0.95
#define MIN_SAMPLES 1000

// Walk types
typedef enum {
    WALK_CLASSICAL,     // Classical random walk
    WALK_HADAMARD,      // Quantum walk with Hadamard coin
    WALK_GROVER,        // Quantum walk with Grover diffusion
    WALK_FOURIER        // Quantum walk with Fourier transform
} walk_type_t;

// Output modes
typedef enum {
    OUTPUT_NORMAL,
    OUTPUT_QUIET,
    OUTPUT_VERBOSE,
    OUTPUT_JSON,
    OUTPUT_CSV
} output_mode_t;

// Position in n-dimensional space
typedef struct {
    int coordinates[MAX_DIMENSIONS];
    double amplitude_real;
    double amplitude_imag;
} position_t;

// Walker state
typedef struct {
    position_t *positions;
    size_t num_positions;
    int dimensions;
    double *coin_state;
    size_t coin_dimension;
} walker_t;

// Simulation configuration
typedef struct {
    walk_type_t type;
    int num_steps;
    int num_walkers;
    int dimensions;
    char seed[256];
    int seed_length;
    output_mode_t output_mode;
    int show_progress;
    char output_file[1024];
    int show_histogram;
    int show_statistics;
    int animate;
} walk_config_t;

// Statistical metrics
typedef struct {
    double mean_distance;
    double std_deviation;
    double spreading_rate;
    double quantum_speedup;
    double entanglement;
    double *position_distribution;
    int distribution_size;
} walk_metrics_t;

// Simulation results
typedef struct {
    walker_t *walkers;
    int num_walkers;
    walk_metrics_t metrics;
    double *histogram;
    int histogram_bins;
} walk_results_t;

// Function declarations
void init_walk_config(walk_config_t *config);
void parse_walk_args(int argc, char *argv[], walk_config_t *config);
void print_walk_usage(const char *program_name);
walk_results_t run_simulation(const walk_config_t *config);
void print_results(const walk_results_t *results, const walk_config_t *config);
void output_results_json(const walk_results_t *results, const walk_config_t *config);
void output_results_csv(const walk_results_t *results, const walk_config_t *config);
void analyze_results(walk_results_t *results, const walk_config_t *config);
void generate_histogram(walk_results_t *results, const walk_config_t *config);
void free_walk_results(walk_results_t *results);

// Quantum operations
void apply_hadamard(double *state, size_t dimension);
void apply_grover(double *state, size_t dimension);
void apply_fourier(double *state, size_t dimension);
void evolve_walker(qrng_ctx *ctx, walker_t *walker, walk_type_t type);
double calculate_entanglement(const walker_t *walker);
void normalize_state(walker_t *walker);

// Utility functions
double calculate_distance(const position_t *pos, int dimensions);
void print_histogram(const double *histogram, int bins);
void animate_walk(const walk_results_t *results, const walk_config_t *config);
void print_statistics(const walk_metrics_t *metrics);

#endif /* QUANTUM_WALK_H */

#include "quantum_walk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>

// Checked allocation helper
static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "Error: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return p;
}

static bool positions_equal(const position_t *a, const position_t *b, int dimensions) {
    for (int i = 0; i < dimensions; i++) {
        if (a->coordinates[i] != b->coordinates[i]) return false;
    }
    return true;
}

void normalize_state(walker_t *walker) {
    double norm = 0;
    for (size_t i = 0; i < walker->num_positions; i++) {
        norm += walker->positions[i].amplitude_real * walker->positions[i].amplitude_real +
                walker->positions[i].amplitude_imag * walker->positions[i].amplitude_imag;
    }
    
    norm = sqrt(norm);
    if (norm > 0) {
        for (size_t i = 0; i < walker->num_positions; i++) {
            walker->positions[i].amplitude_real /= norm;
            walker->positions[i].amplitude_imag /= norm;
        }
    }
}

void init_walk_config(walk_config_t *config) {
    config->type = WALK_HADAMARD;
    config->num_steps = DEFAULT_STEPS;
    config->num_walkers = DEFAULT_WALKERS;
    config->dimensions = DEFAULT_DIMENSIONS;
    strncpy(config->seed, "quantum_walk", sizeof(config->seed) - 1);
    config->seed_length = (int)strlen(config->seed);
    config->output_mode = OUTPUT_NORMAL;
    config->show_progress = 1;
    config->output_file[0] = '\0';
    config->show_histogram = 1;
    config->show_statistics = 1;
    config->animate = 0;
}

void print_walk_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -t, --type TYPE       Walk type (classical, hadamard, grover, fourier)\n");
    printf("  -n, --steps N         Number of steps (%d-%d, default: %d)\n",
           MIN_STEPS, MAX_STEPS, DEFAULT_STEPS);
    printf("  -w, --walkers N       Number of walkers (%d-%d, default: %d)\n",
           MIN_WALKERS, MAX_WALKERS, DEFAULT_WALKERS);
    printf("  -d, --dimensions N    Number of dimensions (1-%d, default: %d)\n",
           MAX_DIMENSIONS, DEFAULT_DIMENSIONS);
    printf("  -s, --seed STRING     Random seed\n");
    printf("  -q, --quiet           Quiet mode (only output final positions)\n");
    printf("  -v, --verbose         Verbose mode (show additional statistics)\n");
    printf("  -j, --json            Output results in JSON format\n");
    printf("  -c, --csv             Output results in CSV format\n");
    printf("  -H, --no-histogram    Hide position histogram\n");
    printf("  -S, --no-stats        Hide statistical analysis\n");
    printf("  -a, --animate         Show animation of walk\n");
    printf("  -P, --no-progress     Hide progress bar\n");
    printf("  -o, --output FILE     Write output to file\n");
    printf("  -h, --help            Show this help message\n");
}

void parse_walk_args(int argc, char *argv[], walk_config_t *config) {
    static struct option long_options[] = {
        {"type",        required_argument, 0, 't'},
        {"steps",       required_argument, 0, 'n'},
        {"walkers",     required_argument, 0, 'w'},
        {"dimensions",  required_argument, 0, 'd'},
        {"seed",        required_argument, 0, 's'},
        {"quiet",       no_argument,       0, 'q'},
        {"verbose",     no_argument,       0, 'v'},
        {"json",        no_argument,       0, 'j'},
        {"csv",         no_argument,       0, 'c'},
        {"no-histogram",no_argument,       0, 'H'},
        {"no-stats",    no_argument,       0, 'S'},
        {"animate",     no_argument,       0, 'a'},
        {"no-progress", no_argument,       0, 'P'},
        {"output",      required_argument, 0, 'o'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "t:n:w:d:s:qvjcHSaPo:h", 
           long_options, &option_index)) != -1) {
        switch (c) {
            case 't':
                if (strcmp(optarg, "classical") == 0) config->type = WALK_CLASSICAL;
                else if (strcmp(optarg, "hadamard") == 0) config->type = WALK_HADAMARD;
                else if (strcmp(optarg, "grover") == 0) config->type = WALK_GROVER;
                else if (strcmp(optarg, "fourier") == 0) config->type = WALK_FOURIER;
                else {
                    fprintf(stderr, "Error: Unknown walk type '%s'\n", optarg);
                    exit(1);
                }
                break;

            case 'n':
                config->num_steps = atoi(optarg);
                if (config->num_steps < MIN_STEPS || config->num_steps > MAX_STEPS) {
                    fprintf(stderr, "Error: Number of steps must be between %d and %d\n",
                            MIN_STEPS, MAX_STEPS);
                    exit(1);
                }
                break;

            case 'w':
                config->num_walkers = atoi(optarg);
                if (config->num_walkers < MIN_WALKERS || 
                    config->num_walkers > MAX_WALKERS) {
                    fprintf(stderr, "Error: Number of walkers must be between %d and %d\n",
                            MIN_WALKERS, MAX_WALKERS);
                    exit(1);
                }
                break;

            case 'd':
                config->dimensions = atoi(optarg);
                if (config->dimensions < 1 || config->dimensions > MAX_DIMENSIONS) {
                    fprintf(stderr, "Error: Dimensions must be between 1 and %d\n",
                            MAX_DIMENSIONS);
                    exit(1);
                }
                break;

            case 's':
                strncpy(config->seed, optarg, sizeof(config->seed) - 1);
                config->seed[sizeof(config->seed) - 1] = '\0';
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

            case 'H':
                config->show_histogram = 0;
                break;

            case 'S':
                config->show_statistics = 0;
                break;

            case 'a':
                config->animate = 1;
                break;

            case 'P':
                config->show_progress = 0;
                break;

            case 'o':
                strncpy(config->output_file, optarg, sizeof(config->output_file) - 1);
                break;

            case 'h':
                print_walk_usage(argv[0]);
                exit(0);

            case '?':
                exit(1);

            default:
                fprintf(stderr, "Error: Unknown option %c\n", c);
                exit(1);
        }
    }
}

void apply_hadamard(double *state, size_t dimension) {
    double *temp = xmalloc(dimension * sizeof(double));
    memcpy(temp, state, dimension * sizeof(double));
    
    for (size_t i = 0; i < dimension; i++) {
        state[i] = 0;
        for (size_t j = 0; j < dimension; j++) {
            double hadamard = (i & j) % 2 == 0 ? 1.0 : -1.0;
            state[i] += hadamard * temp[j] / sqrt(dimension);
        }
    }
    
    free(temp);
}

void apply_grover(double *state, size_t dimension) {
    double mean = 0;
    for (size_t i = 0; i < dimension; i++) {
        mean += state[i];
    }
    mean /= dimension;
    
    for (size_t i = 0; i < dimension; i++) {
        state[i] = 2 * mean - state[i];
    }
}

void apply_fourier(double *state, size_t dimension) {
    double *temp_real = xmalloc(dimension * sizeof(double));
    double *temp_imag = xmalloc(dimension * sizeof(double));
    memcpy(temp_real, state, dimension * sizeof(double));
    memset(temp_imag, 0, dimension * sizeof(double));
    
    const double pi2 = 2.0 * M_PI;
    
    for (size_t i = 0; i < dimension; i++) {
        state[i] = 0;
        for (size_t j = 0; j < dimension; j++) {
            double angle = pi2 * i * j / dimension;
            double cos_angle = cos(angle);
            double sin_angle = sin(angle);
            state[i] += (temp_real[j] * cos_angle - temp_imag[j] * sin_angle) / 
                       sqrt(dimension);
        }
    }
    
    free(temp_real);
    free(temp_imag);
}

void evolve_walker(qrng_ctx *ctx, walker_t *walker, walk_type_t type) {
    // Apply coin operation
    switch (type) {
        case WALK_CLASSICAL: {
            // Classical walk: quantum-randomly pick one definite direction
            size_t chosen = (size_t)qrng_range32(ctx, 0,
                                (int32_t)walker->coin_dimension - 1);
            for (size_t i = 0; i < walker->coin_dimension; i++) {
                walker->coin_state[i] = (i == chosen) ? 1.0 : 0.0;
            }
            break;
        }
        case WALK_HADAMARD:
            apply_hadamard(walker->coin_state, walker->coin_dimension);
            break;
        case WALK_GROVER:
            apply_grover(walker->coin_state, walker->coin_dimension);
            break;
        case WALK_FOURIER:
            apply_fourier(walker->coin_state, walker->coin_dimension);
            break;
    }
    
    // Apply shift operation
    size_t new_positions = walker->num_positions * walker->coin_dimension;
    position_t *new_pos = xmalloc(new_positions * sizeof(position_t));
    size_t pos_count = 0;
    
    for (size_t i = 0; i < walker->num_positions; i++) {
        position_t current = walker->positions[i];
        
        for (size_t j = 0; j < walker->coin_dimension; j++) {
            if (fabs(walker->coin_state[j]) < 1e-10) continue;
            
            position_t pos = current;
            
            // Update position based on coin state
            for (int d = 0; d < walker->dimensions; d++) {
                int direction = ((j >> d) & 1) ? 1 : -1;
                pos.coordinates[d] += direction;
            }
            
            // Calculate new amplitudes
            double coin_val = walker->coin_state[j];
            pos.amplitude_real = current.amplitude_real * coin_val;
            pos.amplitude_imag = current.amplitude_imag * coin_val;
            
            // Check if position already exists
            bool found = false;
            for (size_t k = 0; k < pos_count; k++) {
                if (positions_equal(&new_pos[k], &pos, walker->dimensions)) {
                    new_pos[k].amplitude_real += pos.amplitude_real;
                    new_pos[k].amplitude_imag += pos.amplitude_imag;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                new_pos[pos_count++] = pos;
            }
        }
    }
    
    free(walker->positions);
    if (pos_count > 0) {
        position_t *shrunk = realloc(new_pos, pos_count * sizeof(position_t));
        walker->positions = shrunk ? shrunk : new_pos;
    } else {
        walker->positions = new_pos;  // Keep allocation; state is empty
    }
    walker->num_positions = pos_count;
    
    normalize_state(walker);
}

double calculate_distance(const position_t *pos, int dimensions) {
    double sum = 0;
    for (int i = 0; i < dimensions; i++) {
        sum += pos->coordinates[i] * pos->coordinates[i];
    }
    return sqrt(sum);
}

double calculate_entanglement(const walker_t *walker) {
    // In this simplified model the coin state is shared across all positions
    // (a product state), so the reduced coin density matrix is diagonal with
    // the normalized coin probabilities. Report its von Neumann entropy,
    // which is the Shannon entropy of the coin distribution in bits. (The
    // previous version summed the total probability onto every diagonal
    // element, which made the result independent of the actual state.)
    double norm = 0;
    for (size_t j = 0; j < walker->coin_dimension; j++) {
        norm += walker->coin_state[j] * walker->coin_state[j];
    }
    if (norm <= 0) {
        return 0.0;
    }

    double entropy = 0;
    for (size_t j = 0; j < walker->coin_dimension; j++) {
        double p = walker->coin_state[j] * walker->coin_state[j] / norm;
        if (p > 0) {
            entropy -= p * log2(p);
        }
    }
    return entropy;
}

void generate_histogram(walk_results_t *results, const walk_config_t *config) {
    double max_distance = 0;
    for (int i = 0; i < results->num_walkers; i++) {
        for (size_t j = 0; j < results->walkers[i].num_positions; j++) {
            double dist = calculate_distance(&results->walkers[i].positions[j], 
                                          config->dimensions);
            if (dist > max_distance) max_distance = dist;
        }
    }

    results->histogram = calloc(HISTOGRAM_BINS, sizeof(double));
    if (!results->histogram) {
        fprintf(stderr, "Error: out of memory\n");
        exit(1);
    }
    results->histogram_bins = HISTOGRAM_BINS;

    if (max_distance <= 0) {
        // All probability still at the origin: everything lands in bin 0
        max_distance = 1.0;
    }

    for (int i = 0; i < results->num_walkers; i++) {
        for (size_t j = 0; j < results->walkers[i].num_positions; j++) {
            double dist = calculate_distance(&results->walkers[i].positions[j], 
                                          config->dimensions);
            int bin = (int)(dist * HISTOGRAM_BINS / max_distance);
            if (bin >= HISTOGRAM_BINS) bin = HISTOGRAM_BINS - 1;
            
            double prob = results->walkers[i].positions[j].amplitude_real * 
                         results->walkers[i].positions[j].amplitude_real +
                         results->walkers[i].positions[j].amplitude_imag * 
                         results->walkers[i].positions[j].amplitude_imag;
            
            results->histogram[bin] += prob;
        }
    }
}

void print_histogram(const double *histogram, int bins) {
    printf("\nPosition Distribution:\n");
    printf("=====================\n");
    
    double max_value = 0;
    for (int i = 0; i < bins; i++) {
        if (histogram[i] > max_value) max_value = histogram[i];
    }
    if (max_value <= 0) {
        printf("(no probability mass recorded)\n");
        return;
    }

    const int width = 50;
    for (int i = 0; i < bins; i++) {
        int stars = (int)(histogram[i] * width / max_value);
        printf("%3d |", i);
        for (int j = 0; j < stars; j++) printf("*");
        printf("\n");
    }
}

void analyze_results(walk_results_t *results, const walk_config_t *config) {
    double sum_distance = 0;
    double sum_squared = 0;
    double max_distance = 0;
    
    // Calculate basic statistics
    for (int i = 0; i < results->num_walkers; i++) {
        for (size_t j = 0; j < results->walkers[i].num_positions; j++) {
            double dist = calculate_distance(&results->walkers[i].positions[j], 
                                          config->dimensions);
            double prob = results->walkers[i].positions[j].amplitude_real * 
                         results->walkers[i].positions[j].amplitude_real +
                         results->walkers[i].positions[j].amplitude_imag * 
                         results->walkers[i].positions[j].amplitude_imag;
            
            sum_distance += dist * prob;
            sum_squared += dist * dist * prob;
            if (dist > max_distance) max_distance = dist;
        }
    }
    
    results->metrics.mean_distance = sum_distance / results->num_walkers;
    double variance = sum_squared / results->num_walkers -
                      results->metrics.mean_distance *
                      results->metrics.mean_distance;
    results->metrics.std_deviation = sqrt(variance > 0 ? variance : 0);
    
    // Calculate spreading rate (variance growth)
    results->metrics.spreading_rate = sum_squared / (results->num_walkers * 
                                                   config->num_steps);
    
    // Calculate quantum speedup compared to classical random walk
    double classical_spread = sqrt(config->num_steps);
    results->metrics.quantum_speedup = results->metrics.mean_distance / classical_spread;
    
    // Calculate average entanglement
    double total_entanglement = 0;
    for (int i = 0; i < results->num_walkers; i++) {
        total_entanglement += calculate_entanglement(&results->walkers[i]);
    }
    results->metrics.entanglement = total_entanglement / results->num_walkers;
    
    // Generate position distribution
    results->metrics.position_distribution = xmalloc(HISTOGRAM_BINS * sizeof(double));
    results->metrics.distribution_size = HISTOGRAM_BINS;
    if (max_distance <= 0) max_distance = 1.0;
    
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        double r = i * max_distance / HISTOGRAM_BINS;
        double count = 0;
        
        for (int j = 0; j < results->num_walkers; j++) {
            for (size_t k = 0; k < results->walkers[j].num_positions; k++) {
                double dist = calculate_distance(&results->walkers[j].positions[k], 
                                              config->dimensions);
                if (fabs(dist - r) < max_distance / HISTOGRAM_BINS) {
                    count += results->walkers[j].positions[k].amplitude_real * 
                            results->walkers[j].positions[k].amplitude_real +
                            results->walkers[j].positions[k].amplitude_imag * 
                            results->walkers[j].positions[k].amplitude_imag;
                }
            }
        }
        
        results->metrics.position_distribution[i] = count / results->num_walkers;
    }
}

void animate_walk(const walk_results_t *results, const walk_config_t *config) {
    printf("\033[2J\033[H");  // Clear screen
    printf("Quantum Walk Animation\n");
    printf("====================\n\n");
    
    // Simple ASCII animation for 1D walk
    if (config->dimensions == 1) {
        const int width = 80;
        const int center = width / 2;
        char line[81] = {0};
        memset(line, ' ', width);
        
        for (int i = 0; i < results->num_walkers; i++) {
            for (size_t j = 0; j < results->walkers[i].num_positions; j++) {
                int pos = center + results->walkers[i].positions[j].coordinates[0];
                if (pos >= 0 && pos < width) {
                    line[pos] = '*';
                }
            }
        }
        
        printf("%s\n", line);
    }
    
    usleep(100000);  // 100ms delay
}

void print_statistics(const walk_metrics_t *metrics) {
    printf("\nStatistical Analysis:\n");
    printf("====================\n");
    printf("Mean Distance: %.2f\n", metrics->mean_distance);
    printf("Standard Deviation: %.2f\n", metrics->std_deviation);
    printf("Spreading Rate: %.2f\n", metrics->spreading_rate);
    printf("Quantum Speedup: %.2fx\n", metrics->quantum_speedup);
    printf("Average Entanglement: %.2f bits\n", metrics->entanglement);
}

walk_results_t run_simulation(const walk_config_t *config) {
    walk_results_t results = {0};
    results.num_walkers = config->num_walkers;
    results.walkers = xmalloc(config->num_walkers * sizeof(walker_t));

    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)config->seed, config->seed_length) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        exit(1);
    }
    
    // Initialize walkers
    for (int i = 0; i < config->num_walkers; i++) {
        walker_t *walker = &results.walkers[i];
        walker->dimensions = config->dimensions;
        walker->coin_dimension = 1 << config->dimensions;  // 2^d coin states
        
        // Initialize position
        walker->num_positions = 1;
        walker->positions = xmalloc(sizeof(position_t));
        memset(walker->positions[0].coordinates, 0, 
               sizeof(walker->positions[0].coordinates));
        walker->positions[0].amplitude_real = 1.0;
        walker->positions[0].amplitude_imag = 0.0;
        
        // Initialize coin state
        walker->coin_state = xmalloc(walker->coin_dimension * sizeof(double));
        for (size_t j = 0; j < walker->coin_dimension; j++) {
            walker->coin_state[j] = 1.0 / sqrt(walker->coin_dimension);
        }
    }
    
    // Perform walks
    int progress_step = config->num_steps / 100;
    if (progress_step < 1) progress_step = 1;
    for (int step = 0; step < config->num_steps; step++) {
        for (int i = 0; i < config->num_walkers; i++) {
            evolve_walker(ctx, &results.walkers[i], config->type);
        }
        
        if (config->show_progress && step % progress_step == 0) {
            printf("\rSimulation progress: %d%%", 100 * step / config->num_steps);
            fflush(stdout);
        }
        
        if (config->animate) {
            animate_walk(&results, config);
        }
    }
    
    if (config->show_progress) {
        printf("\rSimulation progress: 100%%\n");
    }
    
    // Generate histogram and analyze results
    if (config->show_histogram) {
        generate_histogram(&results, config);
    }
    
    if (config->show_statistics) {
        analyze_results(&results, config);
    }
    
    qrng_free(ctx);
    return results;
}

static const char *walk_type_name(walk_type_t type) {
    switch (type) {
        case WALK_CLASSICAL: return "classical";
        case WALK_HADAMARD:  return "hadamard";
        case WALK_GROVER:    return "grover";
        case WALK_FOURIER:   return "fourier";
    }
    return "unknown";
}

void output_results_json(const walk_results_t *results, const walk_config_t *config) {
    printf("{\n");
    printf("  \"config\": {\n");
    printf("    \"type\": \"%s\",\n", walk_type_name(config->type));
    printf("    \"steps\": %d,\n", config->num_steps);
    printf("    \"walkers\": %d,\n", config->num_walkers);
    printf("    \"dimensions\": %d\n", config->dimensions);
    printf("  },\n");
    printf("  \"metrics\": {\n");
    printf("    \"mean_distance\": %.4f,\n", results->metrics.mean_distance);
    printf("    \"std_deviation\": %.4f,\n", results->metrics.std_deviation);
    printf("    \"spreading_rate\": %.4f,\n", results->metrics.spreading_rate);
    printf("    \"quantum_speedup\": %.4f,\n", results->metrics.quantum_speedup);
    printf("    \"entanglement\": %.4f\n", results->metrics.entanglement);
    printf("  },\n");
    
    if (results->histogram) {
        printf("  \"histogram\": [");
        for (int i = 0; i < results->histogram_bins; i++) {
            printf("%.4f%s", results->histogram[i],
                   i < results->histogram_bins - 1 ? "," : "");
        }
        printf("]\n");
    }
    
    printf("}\n");
}

void output_results_csv(const walk_results_t *results, const walk_config_t *config) {
    printf("metric,value\n");
    printf("walk_type,%s\n", walk_type_name(config->type));
    printf("steps,%d\n", config->num_steps);
    printf("walkers,%d\n", config->num_walkers);
    printf("dimensions,%d\n", config->dimensions);
    printf("mean_distance,%.4f\n", results->metrics.mean_distance);
    printf("std_deviation,%.4f\n", results->metrics.std_deviation);
    printf("spreading_rate,%.4f\n", results->metrics.spreading_rate);
    printf("quantum_speedup,%.4f\n", results->metrics.quantum_speedup);
    printf("entanglement,%.4f\n", results->metrics.entanglement);
    
    if (results->histogram) {
        printf("\nbin,probability\n");
        for (int i = 0; i < results->histogram_bins; i++) {
            printf("%d,%.4f\n", i, results->histogram[i]);
        }
    }
}

void print_results(const walk_results_t *results, const walk_config_t *config) {
    switch (config->output_mode) {
        case OUTPUT_QUIET:
            for (int i = 0; i < results->num_walkers; i++) {
                for (size_t j = 0; j < results->walkers[i].num_positions; j++) {
                    for (int d = 0; d < config->dimensions; d++) {
                        printf("%d ", results->walkers[i].positions[j].coordinates[d]);
                    }
                    printf("%.4f\n", results->walkers[i].positions[j].amplitude_real * 
                                   results->walkers[i].positions[j].amplitude_real +
                                   results->walkers[i].positions[j].amplitude_imag * 
                                   results->walkers[i].positions[j].amplitude_imag);
                }
            }
            break;

        case OUTPUT_JSON:
            output_results_json(results, config);
            break;

        case OUTPUT_CSV:
            output_results_csv(results, config);
            break;

        case OUTPUT_VERBOSE:
        case OUTPUT_NORMAL:
            printf("\nQuantum Walk Results:\n");
            printf("====================\n\n");
            
            for (int i = 0; i < results->num_walkers; i++) {
                printf("Walker %d:\n", i + 1);
                for (size_t j = 0; j < results->walkers[i].num_positions; j++) {
                    printf("  Position (");
                    for (int d = 0; d < config->dimensions; d++) {
                        printf("%d%s", results->walkers[i].positions[j].coordinates[d],
                               d < config->dimensions - 1 ? "," : "");
                    }
                    printf("): %.4f\n",
                           results->walkers[i].positions[j].amplitude_real * 
                           results->walkers[i].positions[j].amplitude_real +
                           results->walkers[i].positions[j].amplitude_imag * 
                           results->walkers[i].positions[j].amplitude_imag);
                }
                printf("\n");
            }
            
            if (config->show_statistics) {
                print_statistics(&results->metrics);
            }
            
            if (config->show_histogram && results->histogram) {
                print_histogram(results->histogram, results->histogram_bins);
            }
            
            if (config->output_mode == OUTPUT_VERBOSE) {
                printf("\nConfiguration:\n");
                printf("  Walk Type: %s\n",
                       config->type == WALK_CLASSICAL ? "Classical" :
                       config->type == WALK_HADAMARD ? "Hadamard" :
                       config->type == WALK_GROVER ? "Grover" : "Fourier");
                printf("  Dimensions: %d\n", config->dimensions);
                printf("  Steps: %d\n", config->num_steps);
                printf("  Walkers: %d\n", config->num_walkers);
            }
            break;
    }
}

void free_walk_results(walk_results_t *results) {
    for (int i = 0; i < results->num_walkers; i++) {
        free(results->walkers[i].positions);
        free(results->walkers[i].coin_state);
    }
    free(results->walkers);
    free(results->histogram);
    free(results->metrics.position_distribution);
}

int main(int argc, char *argv[]) {
    walk_config_t config;
    init_walk_config(&config);
    parse_walk_args(argc, argv, &config);

    // All reporting goes through printf, so honor -o by redirecting stdout
    if (config.output_file[0]) {
        if (!freopen(config.output_file, "w", stdout)) {
            fprintf(stderr, "Error: Could not open output file '%s'\n",
                    config.output_file);
            return 1;
        }
        config.show_progress = 0;  // Progress bars do not belong in files
    }

    walk_results_t results = run_simulation(&config);
    print_results(&results, &config);
    free_walk_results(&results);

    return 0;
}

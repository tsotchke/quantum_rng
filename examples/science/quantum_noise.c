#include "../../src/quantum_rng/quantum_rng.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 8192
#define NUM_POLES 4

typedef enum {
    NOISE_WHITE,
    NOISE_PINK,
    NOISE_BROWN,
    NOISE_PERLIN,
    NOISE_FRACTAL
} noise_type;

typedef struct {
    double poles[NUM_POLES];
    double outputs[NUM_POLES];
} pink_filter_t;

// Initialize pink noise filter
void init_pink_filter(pink_filter_t *filter) {
    // Poles for approximating 1/f spectrum
    filter->poles[0] = 0.99765;
    filter->poles[1] = 0.96300;
    filter->poles[2] = 0.57000;
    filter->poles[3] = 0.11000;
    
    for(int i = 0; i < NUM_POLES; i++) {
        filter->outputs[i] = 0.0;
    }
}

// Generate white noise using quantum randomness
double generate_white_noise(qrng_ctx *ctx) {
    return 2.0 * qrng_double(ctx) - 1.0;
}

// Generate pink noise using filtered white noise
double generate_pink_noise(qrng_ctx *ctx, pink_filter_t *filter) {
    double white = generate_white_noise(ctx);
    double pink = 0.0;
    
    // Apply pole filtering
    for(int i = 0; i < NUM_POLES; i++) {
        filter->outputs[i] = (white + filter->outputs[i] * filter->poles[i]) / 2.0;
        pink += filter->outputs[i];
    }
    
    return pink * 0.5;  // Scale to match white noise amplitude
}

// Generate brown noise using integration of white noise
double generate_brown_noise(qrng_ctx *ctx, double *last_value) {
    double white = generate_white_noise(ctx);
    *last_value += white * 0.1;  // Smaller step size for stability
    
    // Prevent unbounded drift
    if(*last_value > 1.0) *last_value = 1.0;
    if(*last_value < -1.0) *last_value = -1.0;
    
    return *last_value;
}

// Perlin noise helper functions
double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

double lerp(double t, double a, double b) {
    return a + t * (b - a);
}

double grad(int hash, double x) {
    int h = hash & 15;
    double g = 1.0 + (h & 7);
    if(h & 8) g = -g;
    return g * x;
}

// Permutation table for Perlin noise, shuffled with quantum randomness.
// Gradients must be a *fixed* function of the lattice coordinate; drawing
// fresh random gradients on every call (as the original code did) yields
// plain white noise instead of smooth Perlin noise.
static int perlin_perm[512];

void init_perlin(qrng_ctx *ctx) {
    for(int i = 0; i < 256; i++) {
        perlin_perm[i] = i;
    }
    // Fisher-Yates shuffle driven by the quantum RNG
    for(int i = 255; i > 0; i--) {
        int j = qrng_range32(ctx, 0, i);
        int tmp = perlin_perm[i];
        perlin_perm[i] = perlin_perm[j];
        perlin_perm[j] = tmp;
    }
    for(int i = 0; i < 256; i++) {
        perlin_perm[256 + i] = perlin_perm[i];
    }
}

// Generate 1D Perlin noise (init_perlin must have been called)
double generate_perlin_noise(double x) {
    int X = (int)floor(x) & 255;
    x -= floor(x);

    double g1 = grad(perlin_perm[X], x);
    double g2 = grad(perlin_perm[X + 1], x - 1);

    double u = fade(x);

    return lerp(u, g1, g2);
}

// Generate fractal noise (multiple octaves of Perlin noise)
double generate_fractal_noise(double x, int octaves, double persistence) {
    double total = 0;
    double frequency = 1;
    double amplitude = 1;
    double max_value = 0;

    for(int i = 0; i < octaves; i++) {
        total += generate_perlin_noise(x * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2;
    }

    return total / max_value;
}

// Generate and analyze noise samples
void generate_noise_samples(noise_type type, const char *filename) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"noise", 5) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        exit(1);
    }

    FILE *fp = fopen(filename, "w");
    if(!fp) {
        printf("Error: Could not open file %s\n", filename);
        qrng_free(ctx);
        return;
    }

    double *samples = malloc(BUFFER_SIZE * sizeof(double));
    if(!samples) {
        fprintf(stderr, "Error: out of memory\n");
        fclose(fp);
        qrng_free(ctx);
        exit(1);
    }

    pink_filter_t pink_filter;
    init_pink_filter(&pink_filter);
    double brown_last = 0.0;
    init_perlin(ctx);
    
    // Statistics
    double sum = 0.0;
    double sum_squared = 0.0;
    double min_value = 1e9;
    double max_value = -1e9;
    
    // Generate samples (Perlin/fractal are sampled over 32 lattice cells so
    // the smooth structure is visible across the buffer)
    const double perlin_span = 32.0;
    fprintf(fp, "Sample,Value\n");
    for(int i = 0; i < BUFFER_SIZE; i++) {
        double sample;

        switch(type) {
            case NOISE_WHITE:
                sample = generate_white_noise(ctx);
                break;
            case NOISE_PINK:
                sample = generate_pink_noise(ctx, &pink_filter);
                break;
            case NOISE_BROWN:
                sample = generate_brown_noise(ctx, &brown_last);
                break;
            case NOISE_PERLIN:
                sample = generate_perlin_noise((double)i / BUFFER_SIZE * perlin_span);
                break;
            case NOISE_FRACTAL:
                sample = generate_fractal_noise((double)i / BUFFER_SIZE * perlin_span, 4, 0.5);
                break;
            default:
                sample = 0.0;  // Unreachable; silences uninitialized-use paths
                break;
        }

        samples[i] = sample;
        fprintf(fp, "%d,%f\n", i, sample);
        
        // Update statistics
        sum += sample;
        sum_squared += sample * sample;
        if(sample < min_value) min_value = sample;
        if(sample > max_value) max_value = sample;
    }
    
    fclose(fp);
    
    // Calculate statistics
    double mean = sum / BUFFER_SIZE;
    double variance = (sum_squared / BUFFER_SIZE) - (mean * mean);
    double std_dev = sqrt(variance);
    
    // Print analysis
    printf("\nNoise Analysis:\n");
    printf("Mean: %f\n", mean);
    printf("Standard Deviation: %f\n", std_dev);
    printf("Min Value: %f\n", min_value);
    printf("Max Value: %f\n", max_value);
    
    // Calculate and print power spectrum
    printf("\nPower Spectrum Analysis:\n");
    double *spectrum = malloc(BUFFER_SIZE/2 * sizeof(double));
    double complex *fft_buffer = malloc(BUFFER_SIZE * sizeof(double complex));
    if(!spectrum || !fft_buffer) {
        fprintf(stderr, "Error: out of memory\n");
        exit(1);
    }

    // Use the in-memory samples directly (re-parsing the CSV risked reading
    // uninitialized values on any short read)
    for(int i = 0; i < BUFFER_SIZE; i++) {
        fft_buffer[i] = samples[i];
    }

    // Simple DFT (in practice, use a proper FFT library)
    for(int k = 0; k < BUFFER_SIZE/2; k++) {
        double complex sum = 0;
        for(int n = 0; n < BUFFER_SIZE; n++) {
            double angle = -2 * M_PI * k * n / BUFFER_SIZE;
            sum += fft_buffer[n] * (cos(angle) + I * sin(angle));
        }
        spectrum[k] = cabs(sum) / BUFFER_SIZE;
    }
    
    // Print spectrum in octave bands
    printf("Frequency Band (Hz) | Magnitude\n");
    printf("----------------------------\n");
    for(int i = 0; i < 10; i++) {
        int start_bin = (int)(pow(2, i) * SAMPLE_RATE / BUFFER_SIZE);
        int end_bin = (int)(pow(2, i+1) * SAMPLE_RATE / BUFFER_SIZE);
        if(end_bin > BUFFER_SIZE/2) end_bin = BUFFER_SIZE/2;
        
        double band_power = 0;
        for(int bin = start_bin; bin < end_bin; bin++) {
            band_power += spectrum[bin];
        }
        band_power /= (end_bin - start_bin);
        
        printf("%-16d | %.6f\n", 
               (int)(pow(2, i) * SAMPLE_RATE / BUFFER_SIZE), 
               band_power);
    }
    
    free(spectrum);
    free(fft_buffer);
    free(samples);
    qrng_free(ctx);
}

int main(void) {
    printf("Quantum Noise Generator\n");
    printf("=====================\n");
    
    const char *type_names[] = {
        "White Noise",
        "Pink Noise",
        "Brown Noise",
        "Perlin Noise",
        "Fractal Noise"
    };
    
    for(int i = 0; i < 5; i++) {
        printf("\nGenerating %s...\n", type_names[i]);
        char filename[64];
        snprintf(filename, sizeof(filename), "noise_%d.csv", i);
        generate_noise_samples(i, filename);
    }
    
    printf("\nNoise samples have been written to CSV files.\n");
    printf("Each file contains %d samples.\n", BUFFER_SIZE);
    printf("Sample rate: %d Hz\n", SAMPLE_RATE);
    
    return 0;
}

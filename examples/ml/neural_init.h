#ifndef NEURAL_INIT_H
#define NEURAL_INIT_H

#include "../../src/quantum_rng/quantum_rng.h"

/*
 * Quantum-seeded neural network initialization demo.
 *
 * Demonstrates Xavier/Glorot and He/Kaiming weight initialization driven
 * by the quantum RNG, quantum dropout masks, and a small feed-forward
 * network with a forward pass in training and inference modes.
 */

#define MAX_LAYERS 10
#define MAX_NEURONS 1000

typedef struct {
    double value;
    double gradient;
} neuron_t;

typedef struct {
    int num_inputs;
    int num_neurons;
    neuron_t *neurons;
    double *weights;
    double *biases;
    double *weight_momentums;
    double *bias_momentums;
} layer_t;

typedef struct {
    int num_layers;
    layer_t layers[MAX_LAYERS];
    double learning_rate;
    double momentum;
    double dropout_rate;
} network_t;

/* Weight initialization schemes (quantum randomness) */
void quantum_xavier_init(qrng_ctx *ctx, double *weights, int fan_in, int fan_out);
void quantum_he_init(qrng_ctx *ctx, double *weights, int fan_in, int fan_out);

/* Network construction and inference */
network_t *init_network(qrng_ctx *ctx, int *layer_sizes, int num_layers,
                        double learning_rate, double momentum, double dropout_rate);
void generate_dropout_mask(qrng_ctx *ctx, int size, double rate, int *mask);
void forward_pass(qrng_ctx *ctx, network_t *net, double *input, int training);

/* Activation functions */
double relu(double x);
double relu_derivative(double x);

/* Demonstrations */
void compare_initialization_schemes(qrng_ctx *ctx);
void demonstrate_network(void);

#endif /* NEURAL_INIT_H */

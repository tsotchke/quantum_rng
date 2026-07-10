#include "neural_init.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Checked allocation helpers
static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "Error: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return p;
}

static void *xcalloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    if (!p) {
        fprintf(stderr, "Error: out of memory (%zu bytes)\n", count * size);
        exit(1);
    }
    return p;
}

// Xavier/Glorot initialization with quantum randomness
void quantum_xavier_init(qrng_ctx *ctx, double *weights, int fan_in, int fan_out) {
    double limit = sqrt(6.0 / (fan_in + fan_out));
    for(int i = 0; i < fan_in * fan_out; i++) {
        // Convert uniform [0,1] to uniform [-limit,limit]
        weights[i] = (2.0 * qrng_double(ctx) - 1.0) * limit;
    }
}

// He/Kaiming initialization with quantum randomness
void quantum_he_init(qrng_ctx *ctx, double *weights, int fan_in, int fan_out) {
    double std_dev = sqrt(2.0 / fan_in);
    for(int i = 0; i < fan_in * fan_out; i++) {
        // Box-Muller transform for normal distribution.
        // qrng_double returns [0,1); use 1-u so log() never sees 0.
        double u1 = 1.0 - qrng_double(ctx);
        double u2 = qrng_double(ctx);
        double normal = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        weights[i] = normal * std_dev;
    }
}

// Initialize network with quantum randomness
network_t* init_network(qrng_ctx *ctx, int *layer_sizes, int num_layers, 
                       double learning_rate, double momentum, double dropout_rate) {
    if (num_layers > MAX_LAYERS) {
        fprintf(stderr, "Error: too many layers (%d > %d)\n", num_layers, MAX_LAYERS);
        exit(1);
    }
    network_t *net = xmalloc(sizeof(network_t));
    net->num_layers = num_layers;
    net->learning_rate = learning_rate;
    net->momentum = momentum;
    net->dropout_rate = dropout_rate;
    
    for(int i = 0; i < num_layers; i++) {
        layer_t *layer = &net->layers[i];
        layer->num_neurons = layer_sizes[i];
        layer->num_inputs = (i > 0) ? layer_sizes[i-1] : 0;
        
        // Allocate memory
        layer->neurons = xcalloc(layer->num_neurons, sizeof(neuron_t));
        if(i > 0) {
            int weights_size = layer->num_inputs * layer->num_neurons;
            layer->weights = xmalloc(weights_size * sizeof(double));
            layer->biases = xcalloc(layer->num_neurons, sizeof(double));
            layer->weight_momentums = xcalloc(weights_size, sizeof(double));
            layer->bias_momentums = xcalloc(layer->num_neurons, sizeof(double));
            
            // Initialize weights using He initialization for ReLU networks
            quantum_he_init(ctx, layer->weights, layer->num_inputs, layer->num_neurons);
            
            // Initialize biases with small quantum random values
            for(int j = 0; j < layer->num_neurons; j++) {
                layer->biases[j] = qrng_double(ctx) * 0.1;
            }
        }
    }
    
    return net;
}

// Generate quantum dropout mask
void generate_dropout_mask(qrng_ctx *ctx, int size, double rate, int *mask) {
    for(int i = 0; i < size; i++) {
        mask[i] = (qrng_double(ctx) > rate) ? 1 : 0;
    }
}

// ReLU activation function and its derivative
double relu(double x) {
    return (x > 0) ? x : 0;
}

double relu_derivative(double x) {
    return (x > 0) ? 1 : 0;
}

// Forward pass with quantum dropout
void forward_pass(qrng_ctx *ctx, network_t *net, double *input, int training) {
    // Copy input to first layer
    for(int i = 0; i < net->layers[0].num_neurons; i++) {
        net->layers[0].neurons[i].value = input[i];
    }
    
    // Process hidden and output layers
    for(int l = 1; l < net->num_layers; l++) {
        layer_t *layer = &net->layers[l];
        layer_t *prev_layer = &net->layers[l-1];
        
        // Generate dropout mask for training
        int *dropout_mask = NULL;
        if(training && l < net->num_layers - 1) {  // No dropout in output layer
            dropout_mask = xmalloc(layer->num_neurons * sizeof(int));
            generate_dropout_mask(ctx, layer->num_neurons, net->dropout_rate, dropout_mask);
        }
        
        // Compute layer outputs
        for(int i = 0; i < layer->num_neurons; i++) {
            double sum = layer->biases[i];
            for(int j = 0; j < layer->num_inputs; j++) {
                sum += prev_layer->neurons[j].value * 
                      layer->weights[i * layer->num_inputs + j];
            }
            layer->neurons[i].value = relu(sum);
            
            // Apply dropout during training
            if(dropout_mask) {
                layer->neurons[i].value *= dropout_mask[i];
                // Scale up to maintain expected value
                if(dropout_mask[i]) {
                    layer->neurons[i].value /= (1.0 - net->dropout_rate);
                }
            }
        }
        
        free(dropout_mask);
    }
}

// Demonstrate different initialization schemes
void compare_initialization_schemes(qrng_ctx *ctx) {
    printf("Neural Network Initialization Analysis\n");
    printf("====================================\n\n");
    
    int layer_sizes[] = {784, 512, 256, 128, 10};  // Example MNIST-like architecture
    int num_layers = sizeof(layer_sizes) / sizeof(layer_sizes[0]);
    
    printf("Architecture:\n");
    for(int i = 0; i < num_layers; i++) {
        printf("Layer %d: %d neurons\n", i, layer_sizes[i]);
    }
    printf("\n");
    
    // Initialize with different schemes
    printf("Weight Distribution Statistics:\n");
    printf("-----------------------------\n");
    
    // Analyze Xavier initialization
    double *xavier_weights = xmalloc(layer_sizes[1] * layer_sizes[0] * sizeof(double));
    quantum_xavier_init(ctx, xavier_weights, layer_sizes[0], layer_sizes[1]);
    
    double xavier_mean = 0, xavier_var = 0;
    for(int i = 0; i < layer_sizes[1] * layer_sizes[0]; i++) {
        xavier_mean += xavier_weights[i];
    }
    xavier_mean /= (layer_sizes[1] * layer_sizes[0]);
    
    for(int i = 0; i < layer_sizes[1] * layer_sizes[0]; i++) {
        xavier_var += pow(xavier_weights[i] - xavier_mean, 2);
    }
    xavier_var /= (layer_sizes[1] * layer_sizes[0] - 1);
    
    printf("Xavier/Glorot Initialization:\n");
    printf("Mean: %.6f\n", xavier_mean);
    printf("Std Dev: %.6f\n", sqrt(xavier_var));
    printf("Expected Std Dev: %.6f\n\n", 
           sqrt(2.0 / (layer_sizes[0] + layer_sizes[1])));
    
    // Analyze He initialization
    double *he_weights = xmalloc(layer_sizes[1] * layer_sizes[0] * sizeof(double));
    quantum_he_init(ctx, he_weights, layer_sizes[0], layer_sizes[1]);
    
    double he_mean = 0, he_var = 0;
    for(int i = 0; i < layer_sizes[1] * layer_sizes[0]; i++) {
        he_mean += he_weights[i];
    }
    he_mean /= (layer_sizes[1] * layer_sizes[0]);
    
    for(int i = 0; i < layer_sizes[1] * layer_sizes[0]; i++) {
        he_var += pow(he_weights[i] - he_mean, 2);
    }
    he_var /= (layer_sizes[1] * layer_sizes[0] - 1);
    
    printf("He/Kaiming Initialization:\n");
    printf("Mean: %.6f\n", he_mean);
    printf("Std Dev: %.6f\n", sqrt(he_var));
    printf("Expected Std Dev: %.6f\n\n", sqrt(2.0 / layer_sizes[0]));
    
    // Demonstrate dropout
    printf("Dropout Analysis:\n");
    printf("----------------\n");
    int *dropout_mask = xmalloc(layer_sizes[1] * sizeof(int));
    int total_active = 0;
    
    // Generate multiple dropout masks and analyze statistics
    const int num_trials = 1000;
    for(int i = 0; i < num_trials; i++) {
        generate_dropout_mask(ctx, layer_sizes[1], 0.5, dropout_mask);
        for(int j = 0; j < layer_sizes[1]; j++) {
            total_active += dropout_mask[j];
        }
    }
    
    double avg_active_rate = (double)total_active / (num_trials * layer_sizes[1]);
    printf("Average Active Rate: %.4f (Expected: 0.5000)\n", avg_active_rate);
    
    // Clean up
    free(xavier_weights);
    free(he_weights);
    free(dropout_mask);
}

// Demonstrate network initialization and forward pass
void demonstrate_network(void) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"neural", 6) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        exit(1);
    }
    
    // Define network architecture
    int layer_sizes[] = {4, 8, 8, 2};  // Simple example network
    int num_layers = sizeof(layer_sizes) / sizeof(layer_sizes[0]);
    
    // Initialize network
    network_t *net = init_network(ctx, layer_sizes, num_layers, 0.001, 0.9, 0.5);
    
    // Generate random input
    double input[4];
    for(int i = 0; i < 4; i++) {
        input[i] = qrng_double(ctx);
    }
    
    printf("\nNetwork Forward Pass Demo:\n");
    printf("-------------------------\n");
    printf("Input: ");
    for(int i = 0; i < 4; i++) {
        printf("%.4f ", input[i]);
    }
    printf("\n");
    
    // Perform forward pass with and without dropout
    printf("\nTraining Mode (with dropout):\n");
    forward_pass(ctx, net, input, 1);
    printf("Output: ");
    for(int i = 0; i < net->layers[num_layers-1].num_neurons; i++) {
        printf("%.4f ", net->layers[num_layers-1].neurons[i].value);
    }
    printf("\n");
    
    printf("\nInference Mode (without dropout):\n");
    forward_pass(ctx, net, input, 0);
    printf("Output: ");
    for(int i = 0; i < net->layers[num_layers-1].num_neurons; i++) {
        printf("%.4f ", net->layers[num_layers-1].neurons[i].value);
    }
    printf("\n");
    
    // Clean up
    for(int i = 0; i < num_layers; i++) {
        free(net->layers[i].neurons);
        if(i > 0) {
            free(net->layers[i].weights);
            free(net->layers[i].biases);
            free(net->layers[i].weight_momentums);
            free(net->layers[i].bias_momentums);
        }
    }
    free(net);
    qrng_free(ctx);
}

int main(void) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"neural", 6) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        return 1;
    }
    
    compare_initialization_schemes(ctx);
    demonstrate_network();
    
    qrng_free(ctx);
    return 0;
}

#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LATENT_DIM 10
#define DATA_DIM 2
#define HIDDEN_DIM 16
#define BATCH_SIZE 32
#define NUM_EPOCHS 2000
#define LEARNING_RATE_D 0.0005   /* discriminator step size */
#define LEARNING_RATE_G 0.002    /* generator step size (larger to keep pace) */

typedef struct {
    double *weights;
    double *biases;
    double *weight_gradients;
    double *bias_gradients;
    int input_dim;
    int output_dim;
} layer_t;

typedef struct {
    layer_t hidden;
    layer_t output;
} network_t;

// Checked allocation helper
static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "Error: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return p;
}

// Clamp probability away from 0/1 so log() stays finite
static double clamp_prob(double p) {
    const double eps = 1e-12;
    if (p < eps) return eps;
    if (p > 1.0 - eps) return 1.0 - eps;
    return p;
}

// Initialize layer with quantum randomness
void init_layer(qrng_ctx *ctx, layer_t *layer, int input_dim, int output_dim) {
    layer->input_dim = input_dim;
    layer->output_dim = output_dim;
    
    // Xavier/Glorot initialization
    double scale = sqrt(2.0 / (input_dim + output_dim));
    
    layer->weights = xmalloc(input_dim * output_dim * sizeof(double));
    layer->biases = xmalloc(output_dim * sizeof(double));
    layer->weight_gradients = xmalloc(input_dim * output_dim * sizeof(double));
    layer->bias_gradients = xmalloc(output_dim * sizeof(double));
    
    for(int i = 0; i < input_dim * output_dim; i++) {
        // Convert uniform [0,1] to [-scale,scale]
        layer->weights[i] = (2.0 * qrng_double(ctx) - 1.0) * scale;
        layer->weight_gradients[i] = 0.0;
    }
    
    for(int i = 0; i < output_dim; i++) {
        layer->biases[i] = (2.0 * qrng_double(ctx) - 1.0) * scale;
        layer->bias_gradients[i] = 0.0;
    }
}

void free_layer(layer_t *layer) {
    free(layer->weights);
    free(layer->biases);
    free(layer->weight_gradients);
    free(layer->bias_gradients);
}

// Initialize network
void init_network(qrng_ctx *ctx, network_t *net, int input_dim, int hidden_dim, int output_dim) {
    init_layer(ctx, &net->hidden, input_dim, hidden_dim);
    init_layer(ctx, &net->output, hidden_dim, output_dim);
}

void free_network(network_t *net) {
    free_layer(&net->hidden);
    free_layer(&net->output);
}

// Activation functions and their derivatives
double relu(double x) {
    return x > 0 ? x : 0;
}

double relu_derivative(double x) {
    return x > 0 ? 1 : 0;
}

double sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

double sigmoid_derivative(double x) {
    double s = sigmoid(x);
    return s * (1 - s);
}

double tanh_activation(double x) {
    return tanh(x);
}

double tanh_derivative(double x) {
    double t = tanh(x);
    return 1 - t * t;
}

// Activation selector shared by forward and backward passes
#define ACT_SIGMOID 0
#define ACT_RELU    1
#define ACT_TANH    2

static double apply_activation(double x, int activation) {
    switch(activation) {
        case ACT_RELU: return relu(x);
        case ACT_TANH: return tanh_activation(x);
        default:       return sigmoid(x);
    }
}

static double activation_derivative(double x, int activation) {
    switch(activation) {
        case ACT_RELU: return relu_derivative(x);
        case ACT_TANH: return tanh_derivative(x);
        default:       return sigmoid_derivative(x);
    }
}

// Forward pass through layer with activation
void layer_forward(const layer_t *layer, const double *input, double *output,
                  double *pre_activation, int activation) {
    for(int i = 0; i < layer->output_dim; i++) {
        double sum = layer->biases[i];
        for(int j = 0; j < layer->input_dim; j++) {
            sum += input[j] * layer->weights[j * layer->output_dim + i];
        }
        pre_activation[i] = sum;
        output[i] = apply_activation(sum, activation);
    }
}

// Backward pass through layer
void layer_backward(layer_t *layer, const double *input, const double *output,
                   const double *pre_activation, const double *output_gradients,
                   double *input_gradients, int activation) {
    (void)output;  // Activations are recomputed from pre_activation

    // Clear gradients
    memset(layer->weight_gradients, 0, layer->input_dim * layer->output_dim * sizeof(double));
    memset(layer->bias_gradients, 0, layer->output_dim * sizeof(double));
    if(input_gradients) {
        memset(input_gradients, 0, layer->input_dim * sizeof(double));
    }

    for(int i = 0; i < layer->output_dim; i++) {
        double activation_gradient = activation_derivative(pre_activation[i], activation);
        double grad = output_gradients[i] * activation_gradient;
        
        // Bias gradients
        layer->bias_gradients[i] += grad;
        
        // Weight gradients
        for(int j = 0; j < layer->input_dim; j++) {
            layer->weight_gradients[j * layer->output_dim + i] += input[j] * grad;
            if(input_gradients) {
                input_gradients[j] += layer->weights[j * layer->output_dim + i] * grad;
            }
        }
    }
}

// Update layer parameters using gradients
void update_layer(layer_t *layer, double learning_rate) {
    for(int i = 0; i < layer->input_dim * layer->output_dim; i++) {
        layer->weights[i] -= learning_rate * layer->weight_gradients[i];
    }
    for(int i = 0; i < layer->output_dim; i++) {
        layer->biases[i] -= learning_rate * layer->bias_gradients[i];
    }
}

// Generate latent vector using quantum randomness
void generate_latent(qrng_ctx *ctx, double *latent, int dim) {
    for(int i = 0; i < dim; i++) {
        // Box-Muller transform for normal distribution.
        // qrng_double returns [0,1); use 1-u so log() never sees 0.
        double u1 = 1.0 - qrng_double(ctx);
        double u2 = qrng_double(ctx);
        latent[i] = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    }
}

// Forward pass through generator
void generator_forward(const network_t *generator, const double *latent,
                      double *hidden, double *hidden_pre, 
                      double *output, double *output_pre) {
    layer_forward(&generator->hidden, latent, hidden, hidden_pre, ACT_RELU);
    // tanh output so generated samples span [-1,1] like the real ring data
    layer_forward(&generator->output, hidden, output, output_pre, ACT_TANH);
}

// Forward pass through discriminator
void discriminator_forward(const network_t *discriminator, const double *sample,
                         double *hidden, double *hidden_pre,
                         double *output, double *output_pre) {
    layer_forward(&discriminator->hidden, sample, hidden, hidden_pre, ACT_RELU);
    layer_forward(&discriminator->output, hidden, output, output_pre, ACT_SIGMOID);
}

// Backward pass through generator
void generator_backward(network_t *generator, const double *latent,
                      const double *hidden, const double *hidden_pre,
                      const double *output, const double *output_pre,
                      const double *disc_gradients) {
    double *output_gradients = xmalloc(DATA_DIM * sizeof(double));
    double *hidden_gradients = xmalloc(HIDDEN_DIM * sizeof(double));
    
    // Output layer backward
    layer_backward(&generator->output, hidden, output, output_pre,
                  disc_gradients, hidden_gradients, ACT_TANH);

    // Hidden layer backward
    layer_backward(&generator->hidden, latent, hidden, hidden_pre,
                  hidden_gradients, NULL, ACT_RELU);
    
    free(output_gradients);
    free(hidden_gradients);
}

// Backpropagate the generator's loss -log(D(G(z))) through the discriminator
// to obtain the gradient with respect to the fake sample. The discriminator's
// own gradient buffers are used as scratch space; its weights are not updated.
void discriminator_input_gradients(network_t *discriminator, const double *sample,
                                   const double *hidden, const double *hidden_pre,
                                   const double *output, const double *output_pre,
                                   double *sample_gradients) {
    double output_gradient;
    double *hidden_gradients = xmalloc(HIDDEN_DIM * sizeof(double));

    // Non-saturating generator loss L = -log(D(G(z))).
    // dL/dD = -1/D; layer_backward multiplies by sigmoid'(z) = D(1-D),
    // so the combined pre-activation gradient becomes D - 1.
    output_gradient = -1.0 / clamp_prob(output[0]);

    layer_backward(&discriminator->output, hidden, output, output_pre,
                   &output_gradient, hidden_gradients, ACT_SIGMOID);
    layer_backward(&discriminator->hidden, sample, hidden, hidden_pre,
                   hidden_gradients, sample_gradients, ACT_RELU);

    free(hidden_gradients);
}

// Backward pass through discriminator
void discriminator_backward(network_t *discriminator, const double *sample,
                          const double *hidden, const double *hidden_pre,
                          const double *output, const double *output_pre,
                          double target) {
    double *output_gradients = xmalloc(1 * sizeof(double));
    double *hidden_gradients = xmalloc(HIDDEN_DIM * sizeof(double));

    // Binary cross entropy: dL/dD = (D - target) / (D(1-D)).
    // layer_backward multiplies by sigmoid'(z) = D(1-D), so the combined
    // pre-activation gradient is exactly D - target.
    double d = clamp_prob(output[0]);
    output_gradients[0] = (d - target) / (d * (1.0 - d));
    
    // Output layer backward
    layer_backward(&discriminator->output, hidden, output, output_pre,
                  output_gradients, hidden_gradients, ACT_SIGMOID);

    // Hidden layer backward
    layer_backward(&discriminator->hidden, sample, hidden, hidden_pre,
                  hidden_gradients, NULL, ACT_RELU);
    
    free(output_gradients);
    free(hidden_gradients);
}

// Generate circular data distribution
void generate_real_sample(qrng_ctx *ctx, double *sample) {
    double angle = qrng_double(ctx) * 2.0 * M_PI;
    double radius = 0.8 + qrng_double(ctx) * 0.2;  // Ring with some noise
    
    sample[0] = cos(angle) * radius;
    sample[1] = sin(angle) * radius;
}

void train_gan() {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"gan", 3) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        exit(1);
    }

    // Initialize networks
    network_t generator, discriminator;
    init_network(ctx, &generator, LATENT_DIM, HIDDEN_DIM, DATA_DIM);
    init_network(ctx, &discriminator, DATA_DIM, HIDDEN_DIM, 1);
    
    // Allocate memory for intermediate values
    double *latent = xmalloc(LATENT_DIM * sizeof(double));
    double *gen_hidden = xmalloc(HIDDEN_DIM * sizeof(double));
    double *gen_hidden_pre = xmalloc(HIDDEN_DIM * sizeof(double));
    double *fake_sample = xmalloc(DATA_DIM * sizeof(double));
    double *fake_sample_pre = xmalloc(DATA_DIM * sizeof(double));
    double *real_sample = xmalloc(DATA_DIM * sizeof(double));
    double *disc_hidden = xmalloc(HIDDEN_DIM * sizeof(double));
    double *disc_hidden_pre = xmalloc(HIDDEN_DIM * sizeof(double));
    double *disc_output = xmalloc(1 * sizeof(double));
    double *disc_output_pre = xmalloc(1 * sizeof(double));
    
    printf("Training Quantum GAN\n");
    printf("==================\n\n");
    
    // Training loop
    for(int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        double gen_loss = 0, disc_loss = 0;
        
        for(int batch = 0; batch < BATCH_SIZE; batch++) {
            // Train discriminator on real samples
            generate_real_sample(ctx, real_sample);
            discriminator_forward(&discriminator, real_sample, 
                               disc_hidden, disc_hidden_pre,
                               disc_output, disc_output_pre);
            double real_loss = -log(clamp_prob(disc_output[0]));
            discriminator_backward(&discriminator, real_sample,
                                disc_hidden, disc_hidden_pre,
                                disc_output, disc_output_pre, 1.0);
            
            // Train discriminator on fake samples
            generate_latent(ctx, latent, LATENT_DIM);
            generator_forward(&generator, latent,
                           gen_hidden, gen_hidden_pre,
                           fake_sample, fake_sample_pre);
            discriminator_forward(&discriminator, fake_sample,
                               disc_hidden, disc_hidden_pre,
                               disc_output, disc_output_pre);
            double fake_loss = -log(clamp_prob(1.0 - disc_output[0]));
            discriminator_backward(&discriminator, fake_sample,
                                disc_hidden, disc_hidden_pre,
                                disc_output, disc_output_pre, 0.0);
            
            // Update discriminator
            update_layer(&discriminator.hidden, LEARNING_RATE_D);
            update_layer(&discriminator.output, LEARNING_RATE_D);
            
            // Train generator
            generate_latent(ctx, latent, LATENT_DIM);
            generator_forward(&generator, latent,
                           gen_hidden, gen_hidden_pre,
                           fake_sample, fake_sample_pre);
            discriminator_forward(&discriminator, fake_sample,
                               disc_hidden, disc_hidden_pre,
                               disc_output, disc_output_pre);
            
            // Generator tries to maximize discriminator output on fake
            // samples: backpropagate -log(D(G(z))) through the discriminator
            // to get the true gradient with respect to the fake sample.
            double *gen_gradients = xmalloc(DATA_DIM * sizeof(double));
            discriminator_input_gradients(&discriminator, fake_sample,
                                          disc_hidden, disc_hidden_pre,
                                          disc_output, disc_output_pre,
                                          gen_gradients);

            generator_backward(&generator, latent,
                            gen_hidden, gen_hidden_pre,
                            fake_sample, fake_sample_pre,
                            gen_gradients);
            
            // Update generator
            update_layer(&generator.hidden, LEARNING_RATE_G);
            update_layer(&generator.output, LEARNING_RATE_G);
            
            free(gen_gradients);
            
            // Update losses
            gen_loss += -log(clamp_prob(disc_output[0]));
            disc_loss += real_loss + fake_loss;
        }
        
        gen_loss /= BATCH_SIZE;
        disc_loss /= BATCH_SIZE;
        
        if(epoch % 400 == 0) {
            printf("Epoch %d:\n", epoch);
            printf("Generator Loss: %.4f\n", gen_loss);
            printf("Discriminator Loss: %.4f\n\n", disc_loss);
            
            // Generate and print some samples
            printf("Generated Samples:\n");
            for(int i = 0; i < 5; i++) {
                generate_latent(ctx, latent, LATENT_DIM);
                generator_forward(&generator, latent,
                               gen_hidden, gen_hidden_pre,
                               fake_sample, fake_sample_pre);
                printf("(%6.3f, %6.3f)\n", fake_sample[0], fake_sample[1]);
            }
            printf("\n");
        }
    }
    
    // Generate final samples
    printf("Final Generated Samples:\n");
    printf("======================\n");
    for(int i = 0; i < 10; i++) {
        generate_latent(ctx, latent, LATENT_DIM);
        generator_forward(&generator, latent,
                       gen_hidden, gen_hidden_pre,
                       fake_sample, fake_sample_pre);
        discriminator_forward(&discriminator, fake_sample,
                           disc_hidden, disc_hidden_pre,
                           disc_output, disc_output_pre);
        printf("Sample %2d: (%6.3f, %6.3f) ", i+1, fake_sample[0], fake_sample[1]);
        printf("Discriminator Probability: %.3f\n", disc_output[0]);
    }

    // Measure how close generated samples are to the target ring
    // (real data has radius uniform in [0.8, 1.0], mean 0.9)
    const int num_eval = 500;
    double radius_sum = 0.0, radius_sq_sum = 0.0;
    for(int i = 0; i < num_eval; i++) {
        generate_latent(ctx, latent, LATENT_DIM);
        generator_forward(&generator, latent,
                       gen_hidden, gen_hidden_pre,
                       fake_sample, fake_sample_pre);
        double r = sqrt(fake_sample[0]*fake_sample[0] +
                        fake_sample[1]*fake_sample[1]);
        radius_sum += r;
        radius_sq_sum += r * r;
    }
    double radius_mean = radius_sum / num_eval;
    double radius_var = radius_sq_sum / num_eval - radius_mean * radius_mean;
    printf("\nGenerated sample radius over %d samples: mean %.3f, std %.3f\n",
           num_eval, radius_mean, sqrt(radius_var > 0 ? radius_var : 0));
    printf("Target ring radius: mean 0.900, range [0.8, 1.0]\n");

    // Cleanup
    free(latent);
    free(gen_hidden);
    free(gen_hidden_pre);
    free(fake_sample);
    free(fake_sample_pre);
    free(real_sample);
    free(disc_hidden);
    free(disc_hidden_pre);
    free(disc_output);
    free(disc_output_pre);
    
    free_network(&generator);
    free_network(&discriminator);
    qrng_free(ctx);
}

int main() {
    train_gan();
    return 0;
}

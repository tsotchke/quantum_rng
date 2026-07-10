#include "../../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_SEQ_LEN 128
#define VOCAB_SIZE 1000
#define EMBED_DIM 64
#define NUM_HEADS 4
#define HEAD_DIM 16
#define FFN_DIM 256
#define MAX_TOKENS 10000
#define TEMPERATURE 0.8

typedef struct {
    double *key;
    double *query;
    double *value;
    double *output;
} attention_head_t;

typedef struct {
    attention_head_t heads[NUM_HEADS];
    double *combined_output;
    double *ffn1;
    double *ffn2;
    double *layer_norm1;
    double *layer_norm2;
} transformer_layer_t;

// Checked allocation helper
static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "Error: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return p;
}

typedef struct {
    double *token_embeddings;
    double *positional_embeddings;
    transformer_layer_t *layers;
    int num_layers;
    double *output_projection;
} transformer_t;

// Initialize weights with quantum randomness
static void init_matrix(qrng_ctx *ctx, double *matrix, int rows, int cols) {
    double scale = sqrt(2.0 / (rows + cols));
    for(int i = 0; i < rows * cols; i++) {
        // Box-Muller transform for normal distribution.
        // qrng_double returns [0,1); use 1-u so log() never sees 0.
        double u1 = 1.0 - qrng_double(ctx);
        double u2 = qrng_double(ctx);
        matrix[i] = scale * sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    }
}

static void init_attention_head(qrng_ctx *ctx, attention_head_t *head) {
    head->key = xmalloc(EMBED_DIM * HEAD_DIM * sizeof(double));
    head->query = xmalloc(EMBED_DIM * HEAD_DIM * sizeof(double));
    head->value = xmalloc(EMBED_DIM * HEAD_DIM * sizeof(double));
    head->output = xmalloc(HEAD_DIM * EMBED_DIM * sizeof(double));
    
    init_matrix(ctx, head->key, EMBED_DIM, HEAD_DIM);
    init_matrix(ctx, head->query, EMBED_DIM, HEAD_DIM);
    init_matrix(ctx, head->value, EMBED_DIM, HEAD_DIM);
    init_matrix(ctx, head->output, HEAD_DIM, EMBED_DIM);
}

static void init_transformer_layer(qrng_ctx *ctx, transformer_layer_t *layer) {
    for(int i = 0; i < NUM_HEADS; i++) {
        init_attention_head(ctx, &layer->heads[i]);
    }
    
    layer->combined_output = xmalloc(EMBED_DIM * EMBED_DIM * sizeof(double));
    layer->ffn1 = xmalloc(EMBED_DIM * FFN_DIM * sizeof(double));
    layer->ffn2 = xmalloc(FFN_DIM * EMBED_DIM * sizeof(double));
    layer->layer_norm1 = xmalloc(EMBED_DIM * sizeof(double));
    layer->layer_norm2 = xmalloc(EMBED_DIM * sizeof(double));
    
    init_matrix(ctx, layer->combined_output, EMBED_DIM, EMBED_DIM);
    init_matrix(ctx, layer->ffn1, EMBED_DIM, FFN_DIM);
    init_matrix(ctx, layer->ffn2, FFN_DIM, EMBED_DIM);
    
    // Initialize layer norms to 1
    for(int i = 0; i < EMBED_DIM; i++) {
        layer->layer_norm1[i] = 1.0;
        layer->layer_norm2[i] = 1.0;
    }
}

static void init_transformer(qrng_ctx *ctx, transformer_t *transformer, int num_layers) {
    transformer->num_layers = num_layers;
    
    // Token embeddings
    transformer->token_embeddings = xmalloc(VOCAB_SIZE * EMBED_DIM * sizeof(double));
    init_matrix(ctx, transformer->token_embeddings, VOCAB_SIZE, EMBED_DIM);
    
    // Positional embeddings
    transformer->positional_embeddings = xmalloc(MAX_SEQ_LEN * EMBED_DIM * sizeof(double));
    init_matrix(ctx, transformer->positional_embeddings, MAX_SEQ_LEN, EMBED_DIM);
    
    // Transformer layers
    transformer->layers = xmalloc(num_layers * sizeof(transformer_layer_t));
    for(int i = 0; i < num_layers; i++) {
        init_transformer_layer(ctx, &transformer->layers[i]);
    }
    
    // Output projection
    transformer->output_projection = xmalloc(EMBED_DIM * VOCAB_SIZE * sizeof(double));
    init_matrix(ctx, transformer->output_projection, EMBED_DIM, VOCAB_SIZE);
}

// Matrix operations
static void matrix_multiply(const double *a, const double *b, double *c,
                    int m, int n, int p) {
    for(int i = 0; i < m; i++) {
        for(int j = 0; j < p; j++) {
            double sum = 0;
            for(int k = 0; k < n; k++) {
                sum += a[i * n + k] * b[k * p + j];
            }
            c[i * p + j] = sum;
        }
    }
}

static void softmax(double *x, int n) {
    double max_val = x[0];
    for(int i = 1; i < n; i++) {
        if(x[i] > max_val) max_val = x[i];
    }
    
    double sum = 0;
    for(int i = 0; i < n; i++) {
        x[i] = exp(x[i] - max_val);
        sum += x[i];
    }
    
    for(int i = 0; i < n; i++) {
        x[i] /= sum;
    }
}

static void layer_normalize(double *x, const double *gamma, int n) {
    // Calculate mean
    double mean = 0;
    for(int i = 0; i < n; i++) {
        mean += x[i];
    }
    mean /= n;
    
    // Calculate variance
    double var = 0;
    for(int i = 0; i < n; i++) {
        double diff = x[i] - mean;
        var += diff * diff;
    }
    var /= n;
    
    // Normalize
    double epsilon = 1e-5;
    for(int i = 0; i < n; i++) {
        x[i] = gamma[i] * (x[i] - mean) / sqrt(var + epsilon);
    }
}

// Self-attention with quantum sampling
static void self_attention(qrng_ctx *ctx, const attention_head_t *head,
                   const double *input, double *output,
                   int seq_len) {
    double *keys = xmalloc(seq_len * HEAD_DIM * sizeof(double));
    double *queries = xmalloc(seq_len * HEAD_DIM * sizeof(double));
    double *values = xmalloc(seq_len * HEAD_DIM * sizeof(double));
    double *scores = xmalloc(seq_len * seq_len * sizeof(double));
    
    // Project inputs to K, Q, V
    matrix_multiply(input, head->key, keys, seq_len, EMBED_DIM, HEAD_DIM);
    matrix_multiply(input, head->query, queries, seq_len, EMBED_DIM, HEAD_DIM);
    matrix_multiply(input, head->value, values, seq_len, EMBED_DIM, HEAD_DIM);
    
    // Calculate attention scores with quantum noise
    for(int i = 0; i < seq_len; i++) {
        for(int j = 0; j < seq_len; j++) {
            double score = 0;
            for(int k = 0; k < HEAD_DIM; k++) {
                score += queries[i * HEAD_DIM + k] * keys[j * HEAD_DIM + k];
            }
            score /= sqrt(HEAD_DIM);
            
            // Add quantum noise for exploration
            score += (qrng_double(ctx) * 2.0 - 1.0) * 0.1;
            
            scores[i * seq_len + j] = score;
        }
        softmax(&scores[i * seq_len], seq_len);
    }
    
    // Apply attention
    double *attended = xmalloc(seq_len * HEAD_DIM * sizeof(double));
    matrix_multiply(scores, values, attended, seq_len, seq_len, HEAD_DIM);
    
    // Project to output
    matrix_multiply(attended, head->output, output, seq_len, HEAD_DIM, EMBED_DIM);
    
    free(keys);
    free(queries);
    free(values);
    free(scores);
    free(attended);
}

static void transformer_forward(qrng_ctx *ctx, transformer_t *transformer,
                       const int *input_tokens, int seq_len,
                       double *output) {
    // Embedding layer
    double *hidden = xmalloc(seq_len * EMBED_DIM * sizeof(double));
    for(int i = 0; i < seq_len; i++) {
        for(int j = 0; j < EMBED_DIM; j++) {
            hidden[i * EMBED_DIM + j] = 
                transformer->token_embeddings[input_tokens[i] * EMBED_DIM + j] +
                transformer->positional_embeddings[i * EMBED_DIM + j];
        }
    }
    
    // Process layers
    double *temp = xmalloc(seq_len * EMBED_DIM * sizeof(double));
    double *attention_output = xmalloc(seq_len * EMBED_DIM * sizeof(double));
    double *ffn_output = xmalloc(seq_len * FFN_DIM * sizeof(double));
    
    for(int layer = 0; layer < transformer->num_layers; layer++) {
        transformer_layer_t *current = &transformer->layers[layer];
        
        // Multi-head attention
        memset(attention_output, 0, seq_len * EMBED_DIM * sizeof(double));
        for(int head = 0; head < NUM_HEADS; head++) {
            self_attention(ctx, &current->heads[head], hidden, temp, seq_len);
            for(int i = 0; i < seq_len * EMBED_DIM; i++) {
                attention_output[i] += temp[i] / NUM_HEADS;
            }
        }
        
        // First residual connection and layer norm
        for(int i = 0; i < seq_len * EMBED_DIM; i++) {
            hidden[i] += attention_output[i];
        }
        for(int i = 0; i < seq_len; i++) {
            layer_normalize(&hidden[i * EMBED_DIM], current->layer_norm1, EMBED_DIM);
        }
        
        // Feed-forward network
        matrix_multiply(hidden, current->ffn1, ffn_output, seq_len, EMBED_DIM, FFN_DIM);
        for(int i = 0; i < seq_len * FFN_DIM; i++) {
            ffn_output[i] = fmax(0, ffn_output[i]);  // ReLU
        }
        matrix_multiply(ffn_output, current->ffn2, temp, seq_len, FFN_DIM, EMBED_DIM);
        
        // Second residual connection and layer norm
        for(int i = 0; i < seq_len * EMBED_DIM; i++) {
            hidden[i] += temp[i];
        }
        for(int i = 0; i < seq_len; i++) {
            layer_normalize(&hidden[i * EMBED_DIM], current->layer_norm2, EMBED_DIM);
        }
    }
    
    // Output projection
    matrix_multiply(hidden, transformer->output_projection, output,
                   seq_len, EMBED_DIM, VOCAB_SIZE);
    
    free(hidden);
    free(temp);
    free(attention_output);
    free(ffn_output);
}

// Sample next token using quantum randomness
static int sample_token(qrng_ctx *ctx, const double *logits) {
    // Apply temperature
    double *probs = xmalloc(VOCAB_SIZE * sizeof(double));
    for(int i = 0; i < VOCAB_SIZE; i++) {
        probs[i] = logits[i] / TEMPERATURE;
    }
    softmax(probs, VOCAB_SIZE);
    
    // Quantum random sampling (inverse CDF); if floating-point rounding
    // leaves r >= total cumulative sum, fall back to the last token rather
    // than silently biasing towards token 0.
    double r = qrng_double(ctx);
    double cumsum = 0;
    int token = VOCAB_SIZE - 1;

    for(int i = 0; i < VOCAB_SIZE; i++) {
        cumsum += probs[i];
        if(r < cumsum) {
            token = i;
            break;
        }
    }
    
    free(probs);
    return token;
}

static void generate_text(qrng_ctx *ctx, transformer_t *transformer,
                  const int *prompt_tokens, int prompt_len,
                  int max_new_tokens) {
    if (prompt_len + max_new_tokens > MAX_SEQ_LEN) {
        fprintf(stderr, "Error: sequence length %d exceeds MAX_SEQ_LEN (%d)\n",
                prompt_len + max_new_tokens, MAX_SEQ_LEN);
        exit(1);
    }
    int *sequence = xmalloc((prompt_len + max_new_tokens) * sizeof(int));
    memcpy(sequence, prompt_tokens, prompt_len * sizeof(int));
    int current_len = prompt_len;
    
    double *output = xmalloc(MAX_SEQ_LEN * VOCAB_SIZE * sizeof(double));
    
    printf("Generating text...\n");
    printf("Prompt: ");
    for(int i = 0; i < prompt_len; i++) {
        printf("%d ", prompt_tokens[i]);
    }
    printf("\n\nGenerated: ");
    
    while(current_len < prompt_len + max_new_tokens) {
        // Forward pass
        transformer_forward(ctx, transformer, sequence, current_len, output);
        
        // Sample next token
        int next_token = sample_token(ctx, &output[(current_len-1) * VOCAB_SIZE]);
        sequence[current_len++] = next_token;
        
        printf("%d ", next_token);
        fflush(stdout);
        
        // Stop if end token is generated
        if(next_token == 1) break;  // Assuming 1 is end token
    }
    
    printf("\n");
    free(sequence);
    free(output);
}

static void free_transformer(transformer_t *transformer) {
    for(int i = 0; i < transformer->num_layers; i++) {
        transformer_layer_t *layer = &transformer->layers[i];
        for(int h = 0; h < NUM_HEADS; h++) {
            free(layer->heads[h].key);
            free(layer->heads[h].query);
            free(layer->heads[h].value);
            free(layer->heads[h].output);
        }
        free(layer->combined_output);
        free(layer->ffn1);
        free(layer->ffn2);
        free(layer->layer_norm1);
        free(layer->layer_norm2);
    }
    free(transformer->layers);
    free(transformer->token_embeddings);
    free(transformer->positional_embeddings);
    free(transformer->output_projection);
}

static void demonstrate_transformer(void) {
    qrng_ctx *ctx;
    if (qrng_init(&ctx, (uint8_t*)"transformer", 11) != QRNG_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize quantum RNG\n");
        exit(1);
    }
    
    printf("Quantum Transformer Demo\n");
    printf("=======================\n\n");
    
    // Initialize transformer
    transformer_t transformer;
    init_transformer(ctx, &transformer, 3);  // 3 layers
    
    // Example prompt
    int prompt_tokens[] = {2, 3, 4, 5};  // Some example tokens
    int prompt_len = sizeof(prompt_tokens) / sizeof(prompt_tokens[0]);
    
    // Generate text
    generate_text(ctx, &transformer, prompt_tokens, prompt_len, 20);

    free_transformer(&transformer);
    qrng_free(ctx);
}

int main(void) {
    demonstrate_transformer();
    return 0;
}

#include "../src/quantum_rng/quantum_rng.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#define MATRIX_SIZE 1000
#define NUM_OPERATIONS 100
#define CACHE_LINE_SIZE 64
#define L1_CACHE_SIZE (32 * 1024)
#define L2_CACHE_SIZE (256 * 1024)

typedef struct {
    const char* name;
    double time_ms;
    double throughput;
    double entropy;
    double cache_misses;
    size_t memory_used;
} benchmark_entry;

// Memory tracking
static size_t current_memory = 0;
static size_t peak_memory = 0;

void *tracked_malloc(size_t size) {
    void *ptr = malloc(size);
    if(ptr) {
        current_memory += size;
        if(current_memory > peak_memory) {
            peak_memory = current_memory;
        }
    }
    return ptr;
}

void tracked_free(void *ptr, size_t size) {
    free(ptr);
    current_memory -= size;
}

// Cache simulation (simplified)
double estimate_cache_misses(size_t data_size, size_t access_pattern) {
    double l1_misses = 0;
    double l2_misses = 0;
    
    // Estimate L1 cache misses
    if(data_size > L1_CACHE_SIZE) {
        l1_misses = (double)(data_size - L1_CACHE_SIZE) / data_size;
    }
    
    // Estimate L2 cache misses
    if(data_size > L2_CACHE_SIZE) {
        l2_misses = (double)(data_size - L2_CACHE_SIZE) / data_size;
    }
    
    // Adjust based on access pattern (0=sequential, 1=random)
    double pattern_factor = 1.0 + access_pattern * 0.5;
    return (l1_misses + l2_misses) * pattern_factor;
}

void matrix_operations_benchmark(void) {
    qrng_ctx *ctx;
    qrng_init(&ctx, (uint8_t*)"benchmark", 9);
    
    // Allocate matrices
    size_t matrix_bytes = MATRIX_SIZE * MATRIX_SIZE * sizeof(double);
    double (*matrix1)[MATRIX_SIZE] = tracked_malloc(matrix_bytes);
    double (*matrix2)[MATRIX_SIZE] = tracked_malloc(matrix_bytes);
    double (*result)[MATRIX_SIZE] = tracked_malloc(matrix_bytes);
    
    benchmark_entry results[6];
    int result_idx = 0;
    clock_t start;
    
    // Sequential fill
    start = clock();
    for(int i = 0; i < MATRIX_SIZE; i++) {
        for(int j = 0; j < MATRIX_SIZE; j++) {
            matrix1[i][j] = qrng_double(ctx);
        }
    }
    double fill_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    results[result_idx++] = (benchmark_entry){
        "Sequential Fill",
        fill_time * 1000,
        matrix_bytes / (fill_time * 1024 * 1024),
        qrng_get_entropy_estimate(ctx),
        estimate_cache_misses(matrix_bytes, 0),
        matrix_bytes
    };
    
    // Random access fill
    start = clock();
    for(int k = 0; k < MATRIX_SIZE * MATRIX_SIZE; k++) {
        int i = qrng_uint64(ctx) % MATRIX_SIZE;
        int j = qrng_uint64(ctx) % MATRIX_SIZE;
        matrix2[i][j] = qrng_double(ctx);
    }
    double random_fill_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    results[result_idx++] = (benchmark_entry){
        "Random Fill",
        random_fill_time * 1000,
        matrix_bytes / (random_fill_time * 1024 * 1024),
        qrng_get_entropy_estimate(ctx),
        estimate_cache_misses(matrix_bytes, 1),
        matrix_bytes
    };
    
    // Matrix addition with quantum noise
    start = clock();
    for(int i = 0; i < MATRIX_SIZE; i++) {
        for(int j = 0; j < MATRIX_SIZE; j++) {
            result[i][j] = matrix1[i][j] + matrix2[i][j] + 
                          (qrng_double(ctx) - 0.5) * 0.01;  // Small quantum noise
        }
    }
    double add_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    results[result_idx++] = (benchmark_entry){
        "Matrix Addition",
        add_time * 1000,
        (matrix_bytes * 3) / (add_time * 1024 * 1024),  // Read 2, write 1
        qrng_get_entropy_estimate(ctx),
        estimate_cache_misses(matrix_bytes * 3, 0),
        matrix_bytes * 3
    };
    
    // Quantum-weighted average
    start = clock();
    for(int i = 0; i < MATRIX_SIZE; i++) {
        for(int j = 0; j < MATRIX_SIZE; j++) {
            double weight = qrng_double(ctx);
            result[i][j] = matrix1[i][j] * weight + matrix2[i][j] * (1.0 - weight);
        }
    }
    double avg_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    results[result_idx++] = (benchmark_entry){
        "Quantum Weighted Average",
        avg_time * 1000,
        (matrix_bytes * 3) / (avg_time * 1024 * 1024),
        qrng_get_entropy_estimate(ctx),
        estimate_cache_misses(matrix_bytes * 3, 0),
        matrix_bytes * 3
    };
    
    // Random submatrix operations
    start = clock();
    for(int op = 0; op < NUM_OPERATIONS; op++) {
        // Select random submatrix
        int start_i = qrng_uint64(ctx) % (MATRIX_SIZE - 10);
        int start_j = qrng_uint64(ctx) % (MATRIX_SIZE - 10);
        
        // Perform operation on 10x10 submatrix
        for(int i = 0; i < 10; i++) {
            for(int j = 0; j < 10; j++) {
                result[start_i + i][start_j + j] = 
                    matrix1[start_i + i][start_j + j] * qrng_double(ctx) +
                    matrix2[start_i + i][start_j + j] * qrng_double(ctx);
            }
        }
    }
    double submatrix_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    results[result_idx++] = (benchmark_entry){
        "Random Submatrix Operations",
        submatrix_time * 1000,
        (100 * NUM_OPERATIONS * sizeof(double)) / (submatrix_time * 1024 * 1024),
        qrng_get_entropy_estimate(ctx),
        estimate_cache_misses(100 * sizeof(double), 1),
        matrix_bytes * 3
    };
    
    // Print results
    printf("\nMatrix Operation Benchmarks:\n");
    printf("==========================\n");
    printf("\nOperation Details:\n");
    printf("Matrix Size: %dx%d (%.2f MB per matrix)\n", 
           MATRIX_SIZE, MATRIX_SIZE, matrix_bytes / (1024.0 * 1024.0));
    printf("Peak Memory Usage: %.2f MB\n", peak_memory / (1024.0 * 1024.0));
    
    printf("\nOperation Results:\n");
    for(int i = 0; i < result_idx; i++) {
        printf("\n%s:\n", results[i].name);
        printf("  Time: %.2f ms\n", results[i].time_ms);
        printf("  Throughput: %.2f MB/sec\n", results[i].throughput);
        printf("  Entropy: %.6f bits\n", results[i].entropy);
        printf("  Est. Cache Misses: %.1f%%\n", results[i].cache_misses * 100.0);
        printf("  Memory Used: %.2f MB\n", results[i].memory_used / (1024.0 * 1024.0));
    }
    
    // Cleanup
    tracked_free(matrix1, matrix_bytes);
    tracked_free(matrix2, matrix_bytes);
    tracked_free(result, matrix_bytes);
    qrng_free(ctx);
}

int main() {
    printf("Quantum RNG Matrix Operations Benchmark\n");
    printf("=====================================\n");
    
    // Print system info
    printf("\nSystem Information:\n");
    printf("CPU Cache Line Size: %d bytes\n", CACHE_LINE_SIZE);
    printf("L1 Cache Size: %d KB\n", L1_CACHE_SIZE / 1024);
    printf("L2 Cache Size: %d KB\n", L2_CACHE_SIZE / 1024);
    
    matrix_operations_benchmark();
    return 0;
}
